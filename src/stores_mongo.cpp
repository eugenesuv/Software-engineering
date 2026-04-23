#include "car_rental/stores.h"

#include "car_rental/errors.h"
#include "car_rental/mongo_database.h"
#include "car_rental/utils.h"

#include <Poco/Dynamic/Var.h>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>

#include <bson/bson.h>
#include <mongoc/mongoc.h>

#include <cctype>
#include <cstdint>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace car_rental {
namespace {

using BsonPtr = std::unique_ptr<bson_t, decltype(&bson_destroy)>;
using CollectionPtr = std::unique_ptr<mongoc_collection_t, decltype(&mongoc_collection_destroy)>;
using CursorPtr = std::unique_ptr<mongoc_cursor_t, decltype(&mongoc_cursor_destroy)>;
using SessionPtr = std::unique_ptr<mongoc_client_session_t, decltype(&mongoc_client_session_destroy)>;

constexpr std::string_view kUsersCollection = "users";
constexpr std::string_view kCarsCollection = "cars";
constexpr std::string_view kRentalsCollection = "rentals";
constexpr std::string_view kOutboxCollection = "outbox_events";

bool isHex(char ch)
{
    return std::isdigit(static_cast<unsigned char>(ch)) != 0
        || (ch >= 'a' && ch <= 'f')
        || (ch >= 'A' && ch <= 'F');
}

bool looksLikeUuid(const std::string& value)
{
    if (value.size() != 36)
        return false;

    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const bool mustBeDash = index == 8 || index == 13 || index == 18 || index == 23;
        if (mustBeDash)
        {
            if (value[index] != '-')
                return false;
            continue;
        }

        if (!isHex(value[index]))
            return false;
    }

    return true;
}

std::string stringifyJson(const Poco::JSON::Object::Ptr& object)
{
    std::ostringstream buffer;
    Poco::JSON::Stringifier::stringify(object, buffer);
    return buffer.str();
}

Poco::JSON::Object::Ptr parseJsonObject(const std::string& json)
{
    Poco::JSON::Parser parser;
    return parser.parse(json).extract<Poco::JSON::Object::Ptr>();
}

BsonPtr bsonFromJson(const std::string& json)
{
    bson_error_t error;
    bson_t* document = bson_new_from_json(reinterpret_cast<const std::uint8_t*>(json.data()), -1, &error);
    if (!document)
        throw std::runtime_error("Failed to convert JSON to BSON: " + std::string(error.message));
    return {document, bson_destroy};
}

BsonPtr bsonFromObject(const Poco::JSON::Object::Ptr& object)
{
    return bsonFromJson(stringifyJson(object));
}

Poco::JSON::Object::Ptr objectFromBson(const bson_t* document)
{
    std::size_t length = 0;
    char* json = bson_as_relaxed_extended_json(document, &length);
    const std::string payload(json, length);
    bson_free(json);
    return parseJsonObject(payload);
}

[[noreturn]] void throwMongoFailure(const std::string& message, const bson_error_t& error)
{
    throw std::runtime_error(message + ": " + std::string(error.message));
}

CollectionPtr getCollection(const MongoDatabase& database, std::string_view name)
{
    return {
        mongoc_client_get_collection(database.client(), database.databaseName().c_str(), std::string(name).c_str()),
        mongoc_collection_destroy};
}

SessionPtr startSession(const MongoDatabase& database)
{
    bson_error_t error;
    mongoc_client_session_t* session = mongoc_client_start_session(database.client(), nullptr, &error);
    if (!session)
        throwMongoFailure("Failed to start MongoDB session", error);
    return {session, mongoc_client_session_destroy};
}

BsonPtr sessionOpts(mongoc_client_session_t* session)
{
    bson_error_t error;
    BsonPtr opts(bson_new(), bson_destroy);
    if (!mongoc_client_session_append(session, opts.get(), &error))
        throwMongoFailure("Failed to attach MongoDB session to operation options", error);
    return opts;
}

std::optional<Poco::JSON::Object::Ptr> findOne(
    mongoc_collection_t* collection,
    const bson_t* filter,
    const bson_t* opts = nullptr)
{
    CursorPtr cursor(mongoc_collection_find_with_opts(collection, filter, opts, nullptr), mongoc_cursor_destroy);
    const bson_t* document = nullptr;

    if (mongoc_cursor_next(cursor.get(), &document))
        return objectFromBson(document);

    bson_error_t error;
    if (mongoc_cursor_error(cursor.get(), &error))
        throwMongoFailure("MongoDB cursor operation failed", error);

    return std::nullopt;
}

std::vector<Poco::JSON::Object::Ptr> findMany(
    mongoc_collection_t* collection,
    const bson_t* filter,
    const bson_t* opts = nullptr)
{
    CursorPtr cursor(mongoc_collection_find_with_opts(collection, filter, opts, nullptr), mongoc_cursor_destroy);
    std::vector<Poco::JSON::Object::Ptr> documents;
    const bson_t* document = nullptr;

    while (mongoc_cursor_next(cursor.get(), &document))
        documents.push_back(objectFromBson(document));

    bson_error_t error;
    if (mongoc_cursor_error(cursor.get(), &error))
        throwMongoFailure("MongoDB cursor operation failed", error);

    return documents;
}

long long fieldAsInteger(const Poco::JSON::Object::Ptr& object, const std::string& key)
{
    if (!object->has(key))
        return 0;

    const Poco::Dynamic::Var value = object->get(key);
    if (value.isInteger())
        return value.convert<long long>();
    if (value.isNumeric())
        return static_cast<long long>(value.convert<double>());
    return 0;
}

std::string objectStringOr(const Poco::JSON::Object::Ptr& object, const std::string& key, const std::string& fallback = {})
{
    if (!object->has(key))
        return fallback;
    return object->getValue<std::string>(key);
}

double objectDoubleOr(const Poco::JSON::Object::Ptr& object, const std::string& key, double fallback = 0.0)
{
    if (!object->has(key))
        return fallback;
    return object->getValue<double>(key);
}

Poco::JSON::Object::Ptr userToJson(const UserDto& user)
{
    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("id", user.id);
    object->set("login", user.login);
    object->set("firstName", user.firstName);
    object->set("lastName", user.lastName);
    object->set("email", user.email);
    object->set("phone", user.phone);
    object->set("driverLicenseNumber", user.driverLicenseNumber);
    object->set("role", toString(user.role));
    object->set("createdAt", user.createdAt);
    return object;
}

Poco::JSON::Object::Ptr carToJson(const CarDto& car)
{
    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("id", car.id);
    object->set("vin", car.vin);
    object->set("brand", car.brand);
    object->set("model", car.model);
    object->set("class", toString(car.carClass));
    object->set("status", toString(car.status));
    object->set("pricePerDay", car.pricePerDay);
    object->set("createdAt", car.createdAt);
    return object;
}

Poco::JSON::Object::Ptr userDocument(
    const std::string& userId,
    const RegisterUserRequest& request,
    const std::string& passwordSalt,
    const std::string& passwordHash,
    const std::string& createdAt,
    Role role)
{
    Poco::JSON::Object::Ptr credentials = new Poco::JSON::Object;
    credentials->set("salt", passwordSalt);
    credentials->set("hash", passwordHash);

    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("_id", userId);
    object->set("login", request.login);
    object->set("firstName", request.firstName);
    object->set("lastName", request.lastName);
    object->set("email", request.email);
    object->set("phone", request.phone);
    object->set("driverLicenseNumber", request.driverLicenseNumber);
    object->set("role", toString(role));
    object->set("createdAt", createdAt);
    object->set("credentials", credentials);
    return object;
}

Poco::JSON::Object::Ptr managerDocument(
    const std::string& userId,
    const std::string& login,
    const std::string& passwordSalt,
    const std::string& passwordHash,
    const std::string& createdAt)
{
    RegisterUserRequest request;
    request.login = login;
    request.firstName = "Fleet";
    request.lastName = "Manager";
    request.email = "manager@example.com";
    request.phone = "+10000000000";
    request.driverLicenseNumber = "MANAGER-0001";
    return userDocument(userId, request, passwordSalt, passwordHash, createdAt, Role::FleetManager);
}

Poco::JSON::Object::Ptr carDocument(
    const CreateCarRequest& request,
    const std::string& carId,
    const std::string& createdAt)
{
    Poco::JSON::Array::Ptr features = new Poco::JSON::Array;
    features->add("air_conditioning");
    features->add("bluetooth");

    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("_id", carId);
    object->set("vin", request.vin);
    object->set("brand", request.brand);
    object->set("model", request.model);
    object->set("class", toString(request.carClass));
    object->set("status", "AVAILABLE");
    object->set("pricePerDay", request.pricePerDay);
    object->set("createdAt", createdAt);
    object->set("features", features);
    return object;
}

Poco::JSON::Object::Ptr rentalHistoryEntry(const std::string& status, const std::string& changedAt, const std::string& actor)
{
    Poco::JSON::Object::Ptr entry = new Poco::JSON::Object;
    entry->set("status", status);
    entry->set("changedAt", changedAt);
    entry->set("actor", actor);
    return entry;
}

Poco::JSON::Object::Ptr rentalDocument(const RentalWriteRequest& request)
{
    Poco::JSON::Array::Ptr statusHistory = new Poco::JSON::Array;
    statusHistory->add(rentalHistoryEntry("ACTIVE", request.createdAt, "system"));

    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("_id", request.id);
    object->set("userId", request.userId);
    object->set("carId", request.carId);
    object->set("userSnapshot", userToJson(request.userSnapshot));
    object->set("carSnapshot", carToJson(request.carSnapshot));
    object->set("startAt", request.startAt);
    object->set("endAt", request.endAt);
    object->set("status", "ACTIVE");
    object->set("priceTotal", request.priceTotal);
    object->set("createdAt", request.createdAt);
    object->set("paymentReference", request.paymentReference);
    object->set("statusHistory", statusHistory);
    return object;
}

Poco::JSON::Object::Ptr rentalDtoToPayload(const RentalDto& rental)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("eventId", generateUuid());
    payload->set("rentalId", rental.id);
    payload->set("userId", rental.userId);
    payload->set("carId", rental.carId);
    payload->set("status", toString(rental.status));
    payload->set("createdAt", nowUtcIsoString());
    return payload;
}

Poco::JSON::Object::Ptr outboxEventDocument(const std::string& aggregateId, const std::string& eventType, const RentalDto& rental)
{
    const std::string createdAt = nowUtcIsoString();

    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("_id", generateUuid());
    object->set("aggregateType", "rental");
    object->set("aggregateId", aggregateId);
    object->set("eventType", eventType);
    object->set("payload", rentalDtoToPayload(rental));
    object->set("createdAt", createdAt);
    return object;
}

UserDto userFromObject(const Poco::JSON::Object::Ptr& object)
{
    return UserDto{
        object->getValue<std::string>("_id"),
        object->getValue<std::string>("login"),
        object->getValue<std::string>("firstName"),
        object->getValue<std::string>("lastName"),
        object->getValue<std::string>("email"),
        object->getValue<std::string>("phone"),
        object->getValue<std::string>("driverLicenseNumber"),
        roleFromString(object->getValue<std::string>("role")),
        object->getValue<std::string>("createdAt")};
}

UserRecord userRecordFromObject(const Poco::JSON::Object::Ptr& object)
{
    const auto credentials = object->getObject("credentials");
    return UserRecord{
        userFromObject(object),
        credentials->getValue<std::string>("salt"),
        credentials->getValue<std::string>("hash")};
}

CarDto carFromObject(const Poco::JSON::Object::Ptr& object)
{
    return CarDto{
        object->getValue<std::string>("_id"),
        object->getValue<std::string>("vin"),
        object->getValue<std::string>("brand"),
        object->getValue<std::string>("model"),
        carClassFromString(object->getValue<std::string>("class")),
        carStatusFromString(object->getValue<std::string>("status")),
        object->getValue<double>("pricePerDay"),
        object->getValue<std::string>("createdAt")};
}

RentalDto rentalFromObject(const Poco::JSON::Object::Ptr& object)
{
    return RentalDto{
        object->getValue<std::string>("_id"),
        object->getValue<std::string>("userId"),
        object->getValue<std::string>("carId"),
        object->getValue<std::string>("startAt"),
        object->getValue<std::string>("endAt"),
        rentalStatusFromString(object->getValue<std::string>("status")),
        object->getValue<double>("priceTotal"),
        object->getValue<std::string>("createdAt"),
        objectStringOr(object, "closedAt"),
        objectStringOr(object, "paymentReference")};
}

std::string escapeRegex(const std::string& value)
{
    std::string result;
    result.reserve(value.size() * 2);
    for (const char ch : value)
    {
        switch (ch)
        {
        case '\\':
        case '^':
        case '$':
        case '.':
        case '|':
        case '?':
        case '*':
        case '+':
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
            result.push_back('\\');
            break;
        default:
            break;
        }
        result.push_back(ch);
    }
    return result;
}

Poco::JSON::Object::Ptr sortedFindOptions()
{
    Poco::JSON::Object::Ptr sort = new Poco::JSON::Object;
    sort->set("createdAt", 1);
    sort->set("_id", 1);

    Poco::JSON::Object::Ptr options = new Poco::JSON::Object;
    options->set("sort", sort);
    return options;
}

void insertOneOrThrow(mongoc_collection_t* collection, const Poco::JSON::Object::Ptr& document, const bson_t* opts = nullptr)
{
    const auto bsonDocument = bsonFromObject(document);
    bson_error_t error;
    if (!mongoc_collection_insert_one(collection, bsonDocument.get(), opts, nullptr, &error))
        throwMongoFailure("MongoDB insert failed", error);
}

bool messageContainsDuplicateKey(const std::string& message)
{
    return message.find("E11000") != std::string::npos || message.find("duplicate key") != std::string::npos;
}

long long matchedCount(const bson_t& reply)
{
    const auto object = objectFromBson(&reply);
    const long long direct = fieldAsInteger(object, "matchedCount");
    if (direct != 0)
        return direct;
    const long long legacy = fieldAsInteger(object, "n");
    if (legacy != 0)
        return legacy;
    return fieldAsInteger(object, "nMatched");
}

void commitTransaction(mongoc_client_session_t* session)
{
    bson_t reply = BSON_INITIALIZER;
    bson_error_t error;
    if (!mongoc_client_session_commit_transaction(session, &reply, &error))
    {
        bson_destroy(&reply);
        throwMongoFailure("MongoDB transaction commit failed", error);
    }
    bson_destroy(&reply);
}

class MongoUserStore final : public UserStore
{
public:
    explicit MongoUserStore(const MongoDatabase& database)
        : database_(database)
    {
    }

    UserDto createCustomer(
        const RegisterUserRequest& request,
        const std::string& userId,
        const std::string& passwordSalt,
        const std::string& passwordHash,
        const std::string& createdAt) const override
    {
        auto collection = getCollection(database_, kUsersCollection);

        try
        {
            insertOneOrThrow(
                collection.get(),
                userDocument(userId, request, passwordSalt, passwordHash, createdAt, Role::Customer));
        }
        catch (const std::runtime_error& error)
        {
            if (messageContainsDuplicateKey(error.what()))
                throw ApiException(409, "conflict", "User with this login already exists.");
            throw;
        }

        return UserDto{
            userId,
            request.login,
            request.firstName,
            request.lastName,
            request.email,
            request.phone,
            request.driverLicenseNumber,
            Role::Customer,
            createdAt};
    }

    UserDto upsertManager(
        const std::string& login,
        const std::string& passwordSalt,
        const std::string& passwordHash,
        const std::string& createdAt) const override
    {
        if (const auto existing = findByLogin(login))
            return *existing;

        const std::string userId = generateUuid();
        auto collection = getCollection(database_, kUsersCollection);
        insertOneOrThrow(collection.get(), managerDocument(userId, login, passwordSalt, passwordHash, createdAt));
        return UserDto{
            userId,
            login,
            "Fleet",
            "Manager",
            "manager@example.com",
            "+10000000000",
            "MANAGER-0001",
            Role::FleetManager,
            createdAt};
    }

    std::optional<UserDto> findById(const std::string& userId) const override
    {
        if (!looksLikeUuid(userId))
            return std::nullopt;

        auto collection = getCollection(database_, kUsersCollection);
        const auto filter = bsonFromJson("{\"_id\":\"" + userId + "\"}");
        const auto document = findOne(collection.get(), filter.get());
        if (!document)
            return std::nullopt;
        return userFromObject(*document);
    }

    std::optional<UserDto> findByLogin(const std::string& login) const override
    {
        auto collection = getCollection(database_, kUsersCollection);
        const auto filter = bsonFromJson("{\"login\":\"" + login + "\"}");
        const auto document = findOne(collection.get(), filter.get());
        if (!document)
            return std::nullopt;
        return userFromObject(*document);
    }

    std::optional<UserRecord> findRecordByLogin(const std::string& login) const override
    {
        auto collection = getCollection(database_, kUsersCollection);
        const auto filter = bsonFromJson("{\"login\":\"" + login + "\"}");
        const auto document = findOne(collection.get(), filter.get());
        if (!document)
            return std::nullopt;
        return userRecordFromObject(*document);
    }

    std::vector<UserDto> search(const std::string& nameMask, const std::string& surnameMask) const override
    {
        const std::string nameRegex = ".*" + escapeRegex(nameMask) + ".*";
        const std::string surnameRegex = ".*" + escapeRegex(surnameMask) + ".*";

        Poco::JSON::Object::Ptr filter = new Poco::JSON::Object;

        Poco::JSON::Object::Ptr firstName = new Poco::JSON::Object;
        firstName->set("$regex", nameMask.empty() ? ".*" : nameRegex);
        firstName->set("$options", "i");
        filter->set("firstName", firstName);

        Poco::JSON::Object::Ptr lastName = new Poco::JSON::Object;
        lastName->set("$regex", surnameMask.empty() ? ".*" : surnameRegex);
        lastName->set("$options", "i");
        filter->set("lastName", lastName);

        auto collection = getCollection(database_, kUsersCollection);
        const auto bsonFilter = bsonFromObject(filter);
        const auto bsonOptions = bsonFromObject(sortedFindOptions());

        std::vector<UserDto> users;
        for (const auto& document : findMany(collection.get(), bsonFilter.get(), bsonOptions.get()))
            users.push_back(userFromObject(document));
        return users;
    }

private:
    const MongoDatabase& database_;
};

class MongoFleetStore final : public FleetStore
{
public:
    explicit MongoFleetStore(const MongoDatabase& database)
        : database_(database)
    {
    }

    CarDto addCar(
        const CreateCarRequest& request,
        const std::string& carId,
        const std::string& createdAt) const override
    {
        auto collection = getCollection(database_, kCarsCollection);

        try
        {
            insertOneOrThrow(collection.get(), carDocument(request, carId, createdAt));
        }
        catch (const std::runtime_error& error)
        {
            if (messageContainsDuplicateKey(error.what()))
                throw ApiException(409, "conflict", "Car with this VIN already exists.");
            throw;
        }

        return CarDto{
            carId,
            request.vin,
            request.brand,
            request.model,
            request.carClass,
            CarStatus::Available,
            request.pricePerDay,
            createdAt};
    }

    std::optional<CarDto> findById(const std::string& carId) const override
    {
        if (!looksLikeUuid(carId))
            return std::nullopt;

        auto collection = getCollection(database_, kCarsCollection);
        const auto filter = bsonFromJson("{\"_id\":\"" + carId + "\"}");
        const auto document = findOne(collection.get(), filter.get());
        if (!document)
            return std::nullopt;
        return carFromObject(*document);
    }

    std::vector<CarDto> listAvailable() const override
    {
        auto collection = getCollection(database_, kCarsCollection);
        const auto filter = bsonFromJson("{\"status\":\"AVAILABLE\"}");
        const auto options = bsonFromObject(sortedFindOptions());

        std::vector<CarDto> cars;
        for (const auto& document : findMany(collection.get(), filter.get(), options.get()))
            cars.push_back(carFromObject(document));
        return cars;
    }

    std::vector<CarDto> searchByClass(CarClass carClass) const override
    {
        auto collection = getCollection(database_, kCarsCollection);
        const auto filter = bsonFromJson("{\"class\":\"" + toString(carClass) + "\"}");
        const auto options = bsonFromObject(sortedFindOptions());

        std::vector<CarDto> cars;
        for (const auto& document : findMany(collection.get(), filter.get(), options.get()))
            cars.push_back(carFromObject(document));
        return cars;
    }

    void updateStatus(const std::string& carId, CarStatus status) const override
    {
        if (!looksLikeUuid(carId))
            throw ApiException(404, "not_found", "Car not found.");

        auto collection = getCollection(database_, kCarsCollection);
        const auto filter = bsonFromJson("{\"_id\":\"" + carId + "\"}");

        Poco::JSON::Object::Ptr update = new Poco::JSON::Object;
        Poco::JSON::Object::Ptr set = new Poco::JSON::Object;
        set->set("status", toString(status));
        update->set("$set", set);
        if (status == CarStatus::Available)
        {
            Poco::JSON::Object::Ptr unset = new Poco::JSON::Object;
            unset->set("activeRentalId", true);
            update->set("$unset", unset);
        }

        const auto bsonUpdate = bsonFromObject(update);
        bson_t reply = BSON_INITIALIZER;
        bson_error_t error;
        if (!mongoc_collection_update_one(collection.get(), filter.get(), bsonUpdate.get(), nullptr, &reply, &error))
        {
            bson_destroy(&reply);
            throwMongoFailure("MongoDB car status update failed", error);
        }

        if (matchedCount(reply) == 0)
        {
            bson_destroy(&reply);
            throw ApiException(404, "not_found", "Car not found.");
        }

        bson_destroy(&reply);
    }

private:
    const MongoDatabase& database_;
};

class MongoRentalStore final : public RentalStore
{
public:
    explicit MongoRentalStore(const MongoDatabase& database)
        : database_(database)
    {
    }

    std::optional<RentalDto> findById(const std::string& rentalId) const override
    {
        if (!looksLikeUuid(rentalId))
            return std::nullopt;

        auto collection = getCollection(database_, kRentalsCollection);
        const auto filter = bsonFromJson("{\"_id\":\"" + rentalId + "\"}");
        const auto document = findOne(collection.get(), filter.get());
        if (!document)
            return std::nullopt;
        return rentalFromObject(*document);
    }

    std::vector<RentalDto> listActiveByUser(const std::string& userId) const override
    {
        return listByUserAndStatus(userId, "{\"status\":\"ACTIVE\"}");
    }

    std::vector<RentalDto> listHistoryByUser(const std::string& userId) const override
    {
        return listByUserAndStatus(userId, "{\"status\":{\"$in\":[\"COMPLETED\",\"CANCELLED\"]}}");
    }

private:
    std::vector<RentalDto> listByUserAndStatus(const std::string& userId, const std::string& statusPredicate) const
    {
        if (!looksLikeUuid(userId))
            return {};

        const auto filter = bsonFromJson(
            "{\"userId\":\"" + userId + "\",\"$and\":[" + statusPredicate + "]}");
        const auto options = bsonFromObject(sortedFindOptions());
        auto collection = getCollection(database_, kRentalsCollection);

        std::vector<RentalDto> rentals;
        for (const auto& document : findMany(collection.get(), filter.get(), options.get()))
            rentals.push_back(rentalFromObject(document));
        return rentals;
    }

    const MongoDatabase& database_;
};

class MongoRentalWorkflowCoordinator final : public RentalWorkflowCoordinator
{
public:
    explicit MongoRentalWorkflowCoordinator(const MongoDatabase& database)
        : database_(database)
    {
    }

    RentalDto createActiveRental(const RentalWriteRequest& request) const override
    {
        if (!looksLikeUuid(request.userId))
            throw ApiException(404, "not_found", "User not found.");
        if (!looksLikeUuid(request.carId))
            throw ApiException(404, "not_found", "Car not found.");

        auto session = startSession(database_);
        bson_error_t error;
        if (!mongoc_client_session_start_transaction(session.get(), nullptr, &error))
            throwMongoFailure("Failed to start MongoDB rental transaction", error);

        try
        {
            auto cars = getCollection(database_, kCarsCollection);
            auto rentals = getCollection(database_, kRentalsCollection);
            auto outbox = getCollection(database_, kOutboxCollection);
            const auto opts = sessionOpts(session.get());

            const auto carFilter = bsonFromJson("{\"_id\":\"" + request.carId + "\"}");
            const auto currentCar = findOne(cars.get(), carFilter.get(), opts.get());
            if (!currentCar)
                throw ApiException(404, "not_found", "Car not found.");

            if (carFromObject(*currentCar).status != CarStatus::Available)
                throw ApiException(409, "conflict", "Car is not available for rental.");

            const auto overlapFilter = bsonFromJson(
                "{\"carId\":\"" + request.carId + "\","
                "\"status\":\"ACTIVE\","
                "\"startAt\":{\"$lt\":\"" + request.endAt + "\"},"
                "\"endAt\":{\"$gt\":\"" + request.startAt + "\"}}");
            if (findOne(rentals.get(), overlapFilter.get(), opts.get()))
                throw ApiException(409, "conflict", "Car already has an overlapping active rental.");

            {
                Poco::JSON::Object::Ptr update = new Poco::JSON::Object;
                Poco::JSON::Object::Ptr set = new Poco::JSON::Object;
                set->set("status", "IN_RENT");
                set->set("activeRentalId", request.id);
                update->set("$set", set);

                const auto updateBson = bsonFromObject(update);
                const auto availableFilter = bsonFromJson("{\"_id\":\"" + request.carId + "\",\"status\":\"AVAILABLE\"}");

                bson_t reply = BSON_INITIALIZER;
                if (!mongoc_collection_update_one(cars.get(), availableFilter.get(), updateBson.get(), opts.get(), &reply, &error))
                {
                    bson_destroy(&reply);
                    throwMongoFailure("Failed to reserve car in MongoDB transaction", error);
                }
                if (matchedCount(reply) == 0)
                {
                    bson_destroy(&reply);
                    throw ApiException(409, "conflict", "Car is not available for rental.");
                }
                bson_destroy(&reply);
            }

            insertOneOrThrow(rentals.get(), rentalDocument(request), opts.get());

            RentalDto created{
                request.id,
                request.userId,
                request.carId,
                request.startAt,
                request.endAt,
                RentalStatus::Active,
                request.priceTotal,
                request.createdAt,
                "",
                request.paymentReference};
            insertOneOrThrow(outbox.get(), outboxEventDocument(request.id, "RentalCreated", created), opts.get());

            commitTransaction(session.get());
            return created;
        }
        catch (...)
        {
            mongoc_client_session_abort_transaction(session.get(), nullptr);
            throw;
        }
    }

    RentalDto completeRental(const RentalCompletionRequest& request) const override
    {
        if (!looksLikeUuid(request.rentalId))
            throw ApiException(404, "not_found", "Rental not found.");

        auto session = startSession(database_);
        bson_error_t error;
        if (!mongoc_client_session_start_transaction(session.get(), nullptr, &error))
            throwMongoFailure("Failed to start MongoDB completion transaction", error);

        try
        {
            auto cars = getCollection(database_, kCarsCollection);
            auto rentals = getCollection(database_, kRentalsCollection);
            auto outbox = getCollection(database_, kOutboxCollection);
            const auto opts = sessionOpts(session.get());

            const auto rentalFilter = bsonFromJson("{\"_id\":\"" + request.rentalId + "\"}");
            const auto rentalDocumentObject = findOne(rentals.get(), rentalFilter.get(), opts.get());
            if (!rentalDocumentObject)
                throw ApiException(404, "not_found", "Rental not found.");

            RentalDto rental = rentalFromObject(*rentalDocumentObject);
            if (rental.status != RentalStatus::Active)
                throw ApiException(409, "conflict", "Rental has already been completed.");

            {
                Poco::JSON::Object::Ptr update = new Poco::JSON::Object;
                Poco::JSON::Object::Ptr set = new Poco::JSON::Object;
                set->set("status", "COMPLETED");
                set->set("closedAt", request.closedAt);
                update->set("$set", set);

                Poco::JSON::Object::Ptr push = new Poco::JSON::Object;
                push->set("statusHistory", rentalHistoryEntry("COMPLETED", request.closedAt, "system"));
                update->set("$push", push);

                const auto updateBson = bsonFromObject(update);
                const auto activeFilter = bsonFromJson("{\"_id\":\"" + request.rentalId + "\",\"status\":\"ACTIVE\"}");

                bson_t reply = BSON_INITIALIZER;
                if (!mongoc_collection_update_one(rentals.get(), activeFilter.get(), updateBson.get(), opts.get(), &reply, &error))
                {
                    bson_destroy(&reply);
                    throwMongoFailure("Failed to complete rental in MongoDB transaction", error);
                }
                if (matchedCount(reply) == 0)
                {
                    bson_destroy(&reply);
                    throw ApiException(409, "conflict", "Rental has already been completed.");
                }
                bson_destroy(&reply);
            }

            {
                Poco::JSON::Object::Ptr update = new Poco::JSON::Object;
                Poco::JSON::Object::Ptr set = new Poco::JSON::Object;
                set->set("status", "AVAILABLE");
                update->set("$set", set);
                Poco::JSON::Object::Ptr unset = new Poco::JSON::Object;
                unset->set("activeRentalId", true);
                update->set("$unset", unset);

                const auto carUpdate = bsonFromObject(update);
                const auto carFilter = bsonFromJson("{\"_id\":\"" + rental.carId + "\"}");

                bson_t reply = BSON_INITIALIZER;
                if (!mongoc_collection_update_one(cars.get(), carFilter.get(), carUpdate.get(), opts.get(), &reply, &error))
                {
                    bson_destroy(&reply);
                    throwMongoFailure("Failed to mark car as available in MongoDB transaction", error);
                }
                bson_destroy(&reply);
            }

            rental.status = RentalStatus::Completed;
            rental.closedAt = request.closedAt;
            insertOneOrThrow(outbox.get(), outboxEventDocument(request.rentalId, "RentalCompleted", rental), opts.get());

            commitTransaction(session.get());
            return rental;
        }
        catch (...)
        {
            mongoc_client_session_abort_transaction(session.get(), nullptr);
            throw;
        }
    }

private:
    const MongoDatabase& database_;
};

} // namespace

std::unique_ptr<UserStore> makeMongoUserStore(const MongoDatabase& database)
{
    return std::make_unique<MongoUserStore>(database);
}

std::unique_ptr<FleetStore> makeMongoFleetStore(const MongoDatabase& database)
{
    return std::make_unique<MongoFleetStore>(database);
}

std::unique_ptr<RentalStore> makeMongoRentalStore(const MongoDatabase& database)
{
    return std::make_unique<MongoRentalStore>(database);
}

std::unique_ptr<RentalWorkflowCoordinator> makeMongoRentalWorkflowCoordinator(const MongoDatabase& database)
{
    return std::make_unique<MongoRentalWorkflowCoordinator>(database);
}

} // namespace car_rental
