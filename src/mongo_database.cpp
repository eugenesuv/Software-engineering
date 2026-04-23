#include "car_rental/mongo_database.h"

#include "car_rental/errors.h"

#include <bson/bson.h>
#include <mongoc/mongoc.h>

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Stringifier.h>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace car_rental {
namespace {

std::once_flag gMongoRuntimeInitFlag;

std::string stringifyJson(const Poco::JSON::Object::Ptr& object)
{
    std::ostringstream buffer;
    Poco::JSON::Stringifier::stringify(object, buffer);
    return buffer.str();
}

std::unique_ptr<bson_t, decltype(&bson_destroy)> bsonFromJson(const std::string& json)
{
    bson_error_t error;
    bson_t* document = bson_new_from_json(reinterpret_cast<const std::uint8_t*>(json.data()), static_cast<ssize_t>(json.size()), &error);
    if (!document)
        throw std::runtime_error("Failed to build BSON document from JSON: " + std::string(error.message));
    return {document, bson_destroy};
}

bool shouldIgnoreMongoError(int code, const std::vector<int>& ignoredCodes)
{
    return std::find(ignoredCodes.begin(), ignoredCodes.end(), code) != ignoredCodes.end();
}

void runWriteCommand(const MongoDatabase& database, const Poco::JSON::Object::Ptr& command, const std::vector<int>& ignoredCodes = {})
{
    const auto bsonCommand = bsonFromJson(stringifyJson(command));
    bson_t reply = BSON_INITIALIZER;
    bson_error_t error;

    const bool ok = mongoc_client_write_command_with_opts(
        database.client(),
        database.databaseName().c_str(),
        bsonCommand.get(),
        nullptr,
        &reply,
        &error);

    bson_destroy(&reply);

    if (!ok && !shouldIgnoreMongoError(error.code, ignoredCodes))
        throw std::runtime_error("MongoDB command failed: " + std::string(error.message));
}

void runWriteCommand(const MongoDatabase& database, const std::string& jsonCommand, const std::vector<int>& ignoredCodes = {})
{
    const auto bsonCommand = bsonFromJson(jsonCommand);
    bson_t reply = BSON_INITIALIZER;
    bson_error_t error;

    const bool ok = mongoc_client_write_command_with_opts(
        database.client(),
        database.databaseName().c_str(),
        bsonCommand.get(),
        nullptr,
        &reply,
        &error);

    bson_destroy(&reply);

    if (!ok && !shouldIgnoreMongoError(error.code, ignoredCodes))
        throw std::runtime_error("MongoDB command failed: " + std::string(error.message));
}

Poco::JSON::Object::Ptr createIndexesCommand(const std::string& collectionName, const Poco::JSON::Array::Ptr& indexes)
{
    Poco::JSON::Object::Ptr command = new Poco::JSON::Object;
    command->set("createIndexes", collectionName);
    command->set("indexes", indexes);
    return command;
}

Poco::JSON::Object::Ptr namedCollectionCreateCommand(const std::string& collectionName)
{
    Poco::JSON::Object::Ptr command = new Poco::JSON::Object;
    command->set("create", collectionName);
    return command;
}

Poco::JSON::Object::Ptr indexDocument(
    const std::string& name,
    const Poco::JSON::Object::Ptr& key,
    bool unique = false)
{
    Poco::JSON::Object::Ptr index = new Poco::JSON::Object;
    index->set("name", name);
    index->set("key", key);
    if (unique)
        index->set("unique", true);
    return index;
}

Poco::JSON::Object::Ptr sortKey(std::initializer_list<std::pair<std::string, int>> fields)
{
    Poco::JSON::Object::Ptr key = new Poco::JSON::Object;
    for (const auto& [name, order] : fields)
        key->set(name, order);
    return key;
}

void ensureCollection(const MongoDatabase& database, const std::string& collectionName)
{
    runWriteCommand(database, namedCollectionCreateCommand(collectionName), {48});
}

void ensureIndexes(const MongoDatabase& database)
{
    runWriteCommand(
        database,
        R"({"createIndexes":"users","indexes":[{"name":"uniq_login","key":{"login":1},"unique":true},{"name":"idx_users_created","key":{"createdAt":1,"_id":1}}]})");

    runWriteCommand(
        database,
        R"({"createIndexes":"cars","indexes":[{"name":"uniq_vin","key":{"vin":1},"unique":true},{"name":"idx_cars_status_created","key":{"status":1,"createdAt":1,"_id":1}},{"name":"idx_cars_class_created","key":{"class":1,"createdAt":1,"_id":1}}]})");

    runWriteCommand(
        database,
        R"({"createIndexes":"rentals","indexes":[{"name":"idx_rentals_user_status_created","key":{"userId":1,"status":1,"createdAt":1,"_id":1}},{"name":"idx_rentals_car_status_period","key":{"carId":1,"status":1,"startAt":1,"endAt":1}}]})");

    runWriteCommand(
        database,
        R"({"createIndexes":"outbox_events","indexes":[{"name":"idx_outbox_created","key":{"createdAt":1,"_id":1}},{"name":"idx_outbox_aggregate","key":{"aggregateId":1}}]})");
}

} // namespace

MongoDatabase::MongoDatabase(std::string connectionString, std::string databaseName)
    : connectionString_(std::move(connectionString))
    , databaseName_(std::move(databaseName))
    , client_(nullptr, mongoc_client_destroy)
    , database_(nullptr, mongoc_database_destroy)
{
    std::call_once(gMongoRuntimeInitFlag, []() { mongoc_init(); });

    bson_error_t error;
    mongoc_uri_t* uri = mongoc_uri_new_with_error(connectionString_.c_str(), &error);
    if (!uri)
        throw std::runtime_error("Failed to parse MongoDB URI: " + std::string(error.message));

    mongoc_client_t* client = mongoc_client_new_from_uri(uri);
    mongoc_uri_destroy(uri);
    if (!client)
        throw std::runtime_error("Failed to create MongoDB client.");

    mongoc_client_set_error_api(client, 2);

    client_.reset(client);
    database_.reset(mongoc_client_get_database(client_.get(), databaseName_.c_str()));
}

MongoDatabase::~MongoDatabase()
{
    database_.reset();
    client_.reset();
}

const std::string& MongoDatabase::connectionString() const noexcept
{
    return connectionString_;
}

const std::string& MongoDatabase::databaseName() const noexcept
{
    return databaseName_;
}

mongoc_client_t* MongoDatabase::client() const noexcept
{
    return client_.get();
}

mongoc_database_t* MongoDatabase::database() const noexcept
{
    return database_.get();
}

void MongoDatabase::verifyConnection() const
{
    bson_t command = BSON_INITIALIZER;
    bson_t reply = BSON_INITIALIZER;
    bson_error_t error;

    BSON_APPEND_INT32(&command, "ping", 1);
    const bool ok = mongoc_client_command_simple(client_.get(), "admin", &command, nullptr, &reply, &error);
    bson_destroy(&command);
    bson_destroy(&reply);

    if (!ok)
        throw ApiException(500, "internal_error", "Failed to open MongoDB connection.", error.message);
}

void MongoDatabase::ensureCollections() const
{
    ensureCollection(*this, "users");
    ensureCollection(*this, "cars");
    ensureCollection(*this, "rentals");
    ensureCollection(*this, "outbox_events");
    ensureIndexes(*this);
}

} // namespace car_rental
