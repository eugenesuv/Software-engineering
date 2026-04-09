#include "car_rental/stores.h"

#include "car_rental/database.h"
#include "car_rental/errors.h"
#include "car_rental/utils.h"

#include <Poco/JSON/Object.h>
#include <Poco/JSON/Stringifier.h>

#include <pqxx/pqxx>

#include <cctype>
#include <memory>
#include <sstream>
#include <string_view>

namespace car_rental {
namespace {

constexpr const char* kIsoUtcExpression = "YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"";

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

std::string sqlState(const pqxx::sql_error& error)
{
    return std::string(error.sqlstate());
}

UserDto userFromRow(const pqxx::row& row)
{
    return UserDto{
        row["id"].c_str(),
        row["login"].c_str(),
        row["first_name"].c_str(),
        row["last_name"].c_str(),
        row["email"].c_str(),
        row["phone"].c_str(),
        row["driver_license_number"].c_str(),
        roleFromString(row["role"].c_str()),
        row["created_at"].c_str()};
}

UserRecord userRecordFromRow(const pqxx::row& row)
{
    UserRecord record;
    record.user = UserDto{
        row["id"].c_str(),
        row["login"].c_str(),
        row["first_name"].c_str(),
        row["last_name"].c_str(),
        row["email"].c_str(),
        row["phone"].c_str(),
        row["driver_license_number"].c_str(),
        roleFromString(row["role"].c_str()),
        row["created_at"].c_str()};
    record.passwordSalt = row["password_salt"].c_str();
    record.passwordHash = row["password_hash"].c_str();
    return record;
}

CarDto carFromRow(const pqxx::row& row)
{
    return CarDto{
        row["id"].c_str(),
        row["vin"].c_str(),
        row["brand"].c_str(),
        row["model"].c_str(),
        carClassFromString(row["class"].c_str()),
        carStatusFromString(row["status"].c_str()),
        row["price_per_day"].as<double>(),
        row["created_at"].c_str()};
}

RentalDto rentalFromRow(const pqxx::row& row)
{
    return RentalDto{
        row["id"].c_str(),
        row["user_id"].c_str(),
        row["car_id"].c_str(),
        row["start_at"].c_str(),
        row["end_at"].c_str(),
        rentalStatusFromString(row["status"].c_str()),
        row["price_total"].as<double>(),
        row["created_at"].c_str(),
        row["closed_at"].c_str(),
        row["payment_reference"].c_str()};
}

std::string stringifyJson(const Poco::JSON::Object::Ptr& payload)
{
    std::ostringstream buffer;
    Poco::JSON::Stringifier::stringify(payload, buffer);
    return buffer.str();
}

std::string buildRentalTimestampSelect(const char* columnName)
{
    return "TO_CHAR(" + std::string(columnName) + " AT TIME ZONE 'UTC', '" + kIsoUtcExpression + "') AS " + columnName;
}

std::string buildNullableRentalTimestampSelect(const char* columnName)
{
    return "COALESCE(TO_CHAR(" + std::string(columnName) + " AT TIME ZONE 'UTC', '" + kIsoUtcExpression + "'), '') AS " + columnName;
}

void insertOutboxEvent(pqxx::transaction_base& tx, const std::string& aggregateId, const std::string& eventType, const RentalDto& rental)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("eventId", generateUuid());
    payload->set("rentalId", rental.id);
    payload->set("userId", rental.userId);
    payload->set("carId", rental.carId);
    payload->set("status", toString(rental.status));
    payload->set("createdAt", nowUtcIsoString());

    tx.exec_params(
        R"SQL(
            INSERT INTO rental_service.outbox_events (
                id,
                aggregate_type,
                aggregate_id,
                event_type,
                payload,
                created_at
            )
            VALUES ($1::uuid, 'rental', $2::uuid, $3, $4::jsonb, $5::timestamptz)
        )SQL",
        generateUuid(),
        aggregateId,
        eventType,
        stringifyJson(payload),
        nowUtcIsoString());
}

class PostgresUserStore final : public UserStore
{
public:
    explicit PostgresUserStore(const Database& database)
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
        auto connection = database_.connect();
        pqxx::work tx(*connection);

        try
        {
            tx.exec_params(
                R"SQL(
                    INSERT INTO user_service.users (
                        id,
                        login,
                        password_salt,
                        password_hash,
                        first_name,
                        last_name,
                        email,
                        phone,
                        driver_license_number,
                        role,
                        created_at
                    )
                    VALUES (
                        $1::uuid,
                        $2,
                        $3,
                        $4,
                        $5,
                        $6,
                        $7,
                        $8,
                        $9,
                        'CUSTOMER',
                        $10::timestamptz
                    )
                )SQL",
                userId,
                request.login,
                passwordSalt,
                passwordHash,
                request.firstName,
                request.lastName,
                request.email,
                request.phone,
                request.driverLicenseNumber,
                createdAt);
            tx.commit();
        }
        catch (const pqxx::sql_error& error)
        {
            if (sqlState(error) == "23505")
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
        auto connection = database_.connect();
        pqxx::work tx(*connection);

        const pqxx::result existing = tx.exec_params(
            "SELECT id::text AS id, login, first_name, last_name, email, phone, driver_license_number, role, "
            "TO_CHAR(created_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS created_at "
            "FROM user_service.users WHERE login = $1",
            login);

        if (!existing.empty())
        {
            tx.commit();
            return userFromRow(existing[0]);
        }

        const std::string userId = generateUuid();
        tx.exec_params(
            R"SQL(
                INSERT INTO user_service.users (
                    id,
                    login,
                    password_salt,
                    password_hash,
                    first_name,
                    last_name,
                    email,
                    phone,
                    driver_license_number,
                    role,
                    created_at
                )
                VALUES (
                    $1::uuid,
                    $2,
                    $3,
                    $4,
                    'Fleet',
                    'Manager',
                    'manager@example.com',
                    '+10000000000',
                    'MANAGER-0001',
                    'FLEET_MANAGER',
                    $5::timestamptz
                )
            )SQL",
            userId,
            login,
            passwordSalt,
            passwordHash,
            createdAt);
        tx.commit();

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

        auto connection = database_.connect();
        pqxx::read_transaction tx(*connection);
        const pqxx::result rows = tx.exec_params(
            "SELECT id::text AS id, login, first_name, last_name, email, phone, driver_license_number, role, "
            "TO_CHAR(created_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS created_at "
            "FROM user_service.users WHERE id = $1::uuid",
            userId);

        if (rows.empty())
            return std::nullopt;
        return userFromRow(rows[0]);
    }

    std::optional<UserDto> findByLogin(const std::string& login) const override
    {
        auto connection = database_.connect();
        pqxx::read_transaction tx(*connection);
        const pqxx::result rows = tx.exec_params(
            "SELECT id::text AS id, login, first_name, last_name, email, phone, driver_license_number, role, "
            "TO_CHAR(created_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS created_at "
            "FROM user_service.users WHERE login = $1",
            login);

        if (rows.empty())
            return std::nullopt;
        return userFromRow(rows[0]);
    }

    std::optional<UserRecord> findRecordByLogin(const std::string& login) const override
    {
        auto connection = database_.connect();
        pqxx::read_transaction tx(*connection);
        const pqxx::result rows = tx.exec_params(
            "SELECT id::text AS id, login, first_name, last_name, email, phone, driver_license_number, role, "
            "TO_CHAR(created_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS created_at, "
            "password_salt, password_hash "
            "FROM user_service.users WHERE login = $1",
            login);

        if (rows.empty())
            return std::nullopt;
        return userRecordFromRow(rows[0]);
    }

    std::vector<UserDto> search(const std::string& nameMask, const std::string& surnameMask) const override
    {
        auto connection = database_.connect();
        pqxx::read_transaction tx(*connection);
        const pqxx::result rows = tx.exec_params(
            "SELECT id::text AS id, login, first_name, last_name, email, phone, driver_license_number, role, "
            "TO_CHAR(created_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS created_at "
            "FROM user_service.users "
            "WHERE LOWER(first_name) LIKE $1 AND LOWER(last_name) LIKE $2 "
            "ORDER BY user_service.users.created_at ASC, user_service.users.id ASC",
            nameMask.empty() ? "%" : "%" + lowerCopy(nameMask) + "%",
            surnameMask.empty() ? "%" : "%" + lowerCopy(surnameMask) + "%");

        std::vector<UserDto> users;
        users.reserve(rows.size());
        for (const auto& row : rows)
            users.push_back(userFromRow(row));
        return users;
    }

private:
    static std::string lowerCopy(std::string value)
    {
        for (char& ch : value)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return value;
    }

    const Database& database_;
};

class PostgresFleetStore final : public FleetStore
{
public:
    explicit PostgresFleetStore(const Database& database)
        : database_(database)
    {
    }

    CarDto addCar(
        const CreateCarRequest& request,
        const std::string& carId,
        const std::string& createdAt) const override
    {
        auto connection = database_.connect();
        pqxx::work tx(*connection);

        try
        {
            tx.exec_params(
                R"SQL(
                    INSERT INTO fleet_service.cars (
                        id,
                        vin,
                        brand,
                        model,
                        class,
                        status,
                        price_per_day,
                        created_at
                    )
                    VALUES (
                        $1::uuid,
                        $2,
                        $3,
                        $4,
                        $5,
                        'AVAILABLE',
                        $6::numeric,
                        $7::timestamptz
                    )
                )SQL",
                carId,
                request.vin,
                request.brand,
                request.model,
                toString(request.carClass),
                request.pricePerDay,
                createdAt);
            tx.commit();
        }
        catch (const pqxx::sql_error& error)
        {
            if (sqlState(error) == "23505")
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

        auto connection = database_.connect();
        pqxx::read_transaction tx(*connection);
        const pqxx::result rows = tx.exec_params(
            "SELECT id::text AS id, vin, brand, model, class, status, price_per_day, "
            "TO_CHAR(created_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS created_at "
            "FROM fleet_service.cars WHERE id = $1::uuid",
            carId);

        if (rows.empty())
            return std::nullopt;
        return carFromRow(rows[0]);
    }

    std::vector<CarDto> listAvailable() const override
    {
        auto connection = database_.connect();
        pqxx::read_transaction tx(*connection);
        const pqxx::result rows = tx.exec(
            "SELECT id::text AS id, vin, brand, model, class, status, price_per_day, "
            "TO_CHAR(created_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS created_at "
            "FROM fleet_service.cars "
            "WHERE status = 'AVAILABLE' "
            "ORDER BY fleet_service.cars.created_at ASC, fleet_service.cars.id ASC");

        std::vector<CarDto> cars;
        cars.reserve(rows.size());
        for (const auto& row : rows)
            cars.push_back(carFromRow(row));
        return cars;
    }

    std::vector<CarDto> searchByClass(CarClass carClass) const override
    {
        auto connection = database_.connect();
        pqxx::read_transaction tx(*connection);
        const pqxx::result rows = tx.exec_params(
            "SELECT id::text AS id, vin, brand, model, class, status, price_per_day, "
            "TO_CHAR(created_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS created_at "
            "FROM fleet_service.cars "
            "WHERE class = $1 "
            "ORDER BY fleet_service.cars.created_at ASC, fleet_service.cars.id ASC",
            toString(carClass));

        std::vector<CarDto> cars;
        cars.reserve(rows.size());
        for (const auto& row : rows)
            cars.push_back(carFromRow(row));
        return cars;
    }

    void updateStatus(const std::string& carId, CarStatus status) const override
    {
        if (!looksLikeUuid(carId))
            throw ApiException(404, "not_found", "Car not found.");

        auto connection = database_.connect();
        pqxx::work tx(*connection);
        const pqxx::result rows = tx.exec_params(
            "UPDATE fleet_service.cars SET status = $1 WHERE id = $2::uuid RETURNING id::text AS id",
            toString(status),
            carId);
        if (rows.empty())
            throw ApiException(404, "not_found", "Car not found.");
        tx.commit();
    }

private:
    const Database& database_;
};

class PostgresRentalStore final : public RentalStore
{
public:
    explicit PostgresRentalStore(const Database& database)
        : database_(database)
    {
    }

    std::optional<RentalDto> findById(const std::string& rentalId) const override
    {
        if (!looksLikeUuid(rentalId))
            return std::nullopt;

        auto connection = database_.connect();
        pqxx::read_transaction tx(*connection);
        const pqxx::result rows = tx.exec_params(
            "SELECT id::text AS id, user_id::text AS user_id, car_id::text AS car_id, "
            "TO_CHAR(start_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS start_at, "
            "TO_CHAR(end_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS end_at, "
            "status, price_total, "
            "TO_CHAR(created_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS created_at, "
            "COALESCE(TO_CHAR(closed_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "'), '') AS closed_at, "
            "COALESCE(payment_reference, '') AS payment_reference "
            "FROM rental_service.rentals WHERE id = $1::uuid",
            rentalId);

        if (rows.empty())
            return std::nullopt;
        return rentalFromRow(rows[0]);
    }

    std::vector<RentalDto> listActiveByUser(const std::string& userId) const override
    {
        return listByUserAndPredicate(
            userId,
            "status = 'ACTIVE'");
    }

    std::vector<RentalDto> listHistoryByUser(const std::string& userId) const override
    {
        return listByUserAndPredicate(
            userId,
            "status IN ('COMPLETED', 'CANCELLED')");
    }

private:
    std::vector<RentalDto> listByUserAndPredicate(const std::string& userId, const std::string& predicate) const
    {
        if (!looksLikeUuid(userId))
            return {};

        auto connection = database_.connect();
        pqxx::read_transaction tx(*connection);
        const pqxx::result rows = tx.exec_params(
            "SELECT id::text AS id, user_id::text AS user_id, car_id::text AS car_id, "
            "TO_CHAR(start_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS start_at, "
            "TO_CHAR(end_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS end_at, "
            "status, price_total, "
            "TO_CHAR(created_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS created_at, "
            "COALESCE(TO_CHAR(closed_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "'), '') AS closed_at, "
            "COALESCE(payment_reference, '') AS payment_reference "
            "FROM rental_service.rentals "
            "WHERE user_id = $1::uuid AND " + predicate + " "
            "ORDER BY rental_service.rentals.created_at ASC, rental_service.rentals.id ASC",
            userId);

        std::vector<RentalDto> rentals;
        rentals.reserve(rows.size());
        for (const auto& row : rows)
            rentals.push_back(rentalFromRow(row));
        return rentals;
    }

    const Database& database_;
};

class PostgresRentalWorkflowCoordinator final : public RentalWorkflowCoordinator
{
public:
    explicit PostgresRentalWorkflowCoordinator(const Database& database)
        : database_(database)
    {
    }

    RentalDto createActiveRental(const RentalWriteRequest& request) const override
    {
        if (!looksLikeUuid(request.userId))
            throw ApiException(404, "not_found", "User not found.");
        if (!looksLikeUuid(request.carId))
            throw ApiException(404, "not_found", "Car not found.");

        auto connection = database_.connect();
        pqxx::work tx(*connection);

        try
        {
            const pqxx::result carRows = tx.exec_params(
                "SELECT id::text AS id, vin, brand, model, class, status, price_per_day, "
                "TO_CHAR(created_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS created_at "
                "FROM fleet_service.cars "
                "WHERE id = $1::uuid "
                "FOR UPDATE",
                request.carId);
            if (carRows.empty())
                throw ApiException(404, "not_found", "Car not found.");

            const CarDto car = carFromRow(carRows[0]);
            if (car.status != CarStatus::Available)
                throw ApiException(409, "conflict", "Car is not available for rental.");

            const pqxx::result overlapRows = tx.exec_params(
                R"SQL(
                    SELECT 1
                    FROM rental_service.rentals
                    WHERE car_id = $1::uuid
                      AND status = 'ACTIVE'
                      AND tstzrange(start_at, end_at, '[)')
                          && tstzrange($2::timestamptz, $3::timestamptz, '[)')
                    LIMIT 1
                )SQL",
                request.carId,
                request.startAt,
                request.endAt);
            if (!overlapRows.empty())
                throw ApiException(409, "conflict", "Car already has an overlapping active rental.");

            tx.exec_params(
                R"SQL(
                    INSERT INTO rental_service.rentals (
                        id,
                        user_id,
                        car_id,
                        start_at,
                        end_at,
                        status,
                        price_total,
                        created_at,
                        closed_at,
                        payment_reference
                    )
                    VALUES (
                        $1::uuid,
                        $2::uuid,
                        $3::uuid,
                        $4::timestamptz,
                        $5::timestamptz,
                        'ACTIVE',
                        $6::numeric,
                        $7::timestamptz,
                        NULL,
                        $8
                    )
                )SQL",
                request.id,
                request.userId,
                request.carId,
                request.startAt,
                request.endAt,
                request.priceTotal,
                request.createdAt,
                request.paymentReference);

            tx.exec_params(
                "UPDATE fleet_service.cars SET status = 'IN_RENT' WHERE id = $1::uuid",
                request.carId);

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

            insertOutboxEvent(tx, request.id, "RentalCreated", created);
            tx.commit();
            return created;
        }
        catch (const pqxx::sql_error& error)
        {
            if (sqlState(error) == "23P01")
                throw ApiException(409, "conflict", "Car already has an overlapping active rental.");
            if (sqlState(error) == "23503")
                throw ApiException(404, "not_found", "User or car not found.");
            throw;
        }
    }

    RentalDto completeRental(const RentalCompletionRequest& request) const override
    {
        if (!looksLikeUuid(request.rentalId))
            throw ApiException(404, "not_found", "Rental not found.");

        auto connection = database_.connect();
        pqxx::work tx(*connection);

        const pqxx::result rows = tx.exec_params(
            "SELECT id::text AS id, user_id::text AS user_id, car_id::text AS car_id, "
            "TO_CHAR(start_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS start_at, "
            "TO_CHAR(end_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS end_at, "
            "status, price_total, "
            "TO_CHAR(created_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "') AS created_at, "
            "COALESCE(TO_CHAR(closed_at AT TIME ZONE 'UTC', '" + std::string(kIsoUtcExpression) + "'), '') AS closed_at, "
            "COALESCE(payment_reference, '') AS payment_reference "
            "FROM rental_service.rentals "
            "WHERE id = $1::uuid "
            "FOR UPDATE",
            request.rentalId);

        if (rows.empty())
            throw ApiException(404, "not_found", "Rental not found.");

        RentalDto rental = rentalFromRow(rows[0]);
        if (rental.status != RentalStatus::Active)
            throw ApiException(409, "conflict", "Rental has already been completed.");

        tx.exec_params(
            "UPDATE rental_service.rentals "
            "SET status = 'COMPLETED', closed_at = $1::timestamptz "
            "WHERE id = $2::uuid",
            request.closedAt,
            request.rentalId);

        tx.exec_params(
            "UPDATE fleet_service.cars SET status = 'AVAILABLE' WHERE id = $1::uuid",
            rental.carId);

        rental.status = RentalStatus::Completed;
        rental.closedAt = request.closedAt;
        insertOutboxEvent(tx, request.rentalId, "RentalCompleted", rental);
        tx.commit();
        return rental;
    }

private:
    const Database& database_;
};

} // namespace

std::unique_ptr<UserStore> makePostgresUserStore(const Database& database)
{
    return std::make_unique<PostgresUserStore>(database);
}

std::unique_ptr<FleetStore> makePostgresFleetStore(const Database& database)
{
    return std::make_unique<PostgresFleetStore>(database);
}

std::unique_ptr<RentalStore> makePostgresRentalStore(const Database& database)
{
    return std::make_unique<PostgresRentalStore>(database);
}

std::unique_ptr<RentalWorkflowCoordinator> makePostgresRentalWorkflowCoordinator(const Database& database)
{
    return std::make_unique<PostgresRentalWorkflowCoordinator>(database);
}

} // namespace car_rental
