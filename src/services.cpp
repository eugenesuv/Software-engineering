#include "car_rental/services.h"

#include "car_rental/errors.h"
#include "car_rental/utils.h"

#include <Poco/Exception.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/Timespan.h>

#include <optional>
#include <regex>
#include <sstream>

namespace car_rental {
namespace {

const std::regex kLoginRegex(R"(^[A-Za-z0-9_.-]{3,32}$)");
const std::regex kEmailRegex(R"(^[^@\s]+@[^@\s]+\.[^@\s]+$)");
const std::regex kPhoneRegex(R"(^[+0-9() -]{7,20}$)");
const std::regex kVinRegex(R"(^[A-HJ-NPR-Z0-9]{11,17}$)");

UserDto userFromStatement(sqlite3_stmt* stmt)
{
    return UserDto{
        columnString(stmt, 0),
        columnString(stmt, 1),
        columnString(stmt, 2),
        columnString(stmt, 3),
        columnString(stmt, 4),
        columnString(stmt, 5),
        columnString(stmt, 6),
        roleFromString(columnString(stmt, 7)),
        columnString(stmt, 8)};
}

UserRecord userRecordFromStatement(sqlite3_stmt* stmt)
{
    UserRecord record;
    record.user = UserDto{
        columnString(stmt, 0),
        columnString(stmt, 1),
        columnString(stmt, 2),
        columnString(stmt, 3),
        columnString(stmt, 4),
        columnString(stmt, 5),
        columnString(stmt, 6),
        roleFromString(columnString(stmt, 7)),
        columnString(stmt, 8)};
    record.passwordSalt = columnString(stmt, 9);
    record.passwordHash = columnString(stmt, 10);
    return record;
}

CarDto carFromStatement(sqlite3_stmt* stmt)
{
    return CarDto{
        columnString(stmt, 0),
        columnString(stmt, 1),
        columnString(stmt, 2),
        columnString(stmt, 3),
        carClassFromString(columnString(stmt, 4)),
        carStatusFromString(columnString(stmt, 5)),
        columnDouble(stmt, 6),
        columnString(stmt, 7)};
}

RentalDto rentalFromStatement(sqlite3_stmt* stmt)
{
    return RentalDto{
        columnString(stmt, 0),
        columnString(stmt, 1),
        columnString(stmt, 2),
        columnString(stmt, 3),
        columnString(stmt, 4),
        rentalStatusFromString(columnString(stmt, 5)),
        columnDouble(stmt, 6),
        columnString(stmt, 7),
        columnString(stmt, 8),
        columnString(stmt, 9)};
}

std::optional<UserDto> tryLoadUserById(sqlite3* db, const std::string& userId)
{
    SqliteStatement stmt(
        db,
        "SELECT id, login, first_name, last_name, email, phone, driver_license_number, role, created_at "
        "FROM users WHERE id = ?");
    stmt.bind(1, userId);
    if (stmt.step() == SQLITE_ROW)
        return userFromStatement(stmt.handle());
    return std::nullopt;
}

std::optional<UserDto> tryLoadUserByLogin(sqlite3* db, const std::string& login)
{
    SqliteStatement stmt(
        db,
        "SELECT id, login, first_name, last_name, email, phone, driver_license_number, role, created_at "
        "FROM users WHERE login = ?");
    stmt.bind(1, login);
    if (stmt.step() == SQLITE_ROW)
        return userFromStatement(stmt.handle());
    return std::nullopt;
}

std::optional<UserRecord> tryLoadUserRecordByLogin(sqlite3* db, const std::string& login)
{
    SqliteStatement stmt(
        db,
        "SELECT id, login, first_name, last_name, email, phone, driver_license_number, role, created_at, password_salt, password_hash "
        "FROM users WHERE login = ?");
    stmt.bind(1, login);
    if (stmt.step() == SQLITE_ROW)
        return userRecordFromStatement(stmt.handle());
    return std::nullopt;
}

std::optional<CarDto> tryLoadCarById(sqlite3* db, const std::string& carId)
{
    SqliteStatement stmt(
        db,
        "SELECT id, vin, brand, model, class, status, price_per_day, created_at "
        "FROM cars WHERE id = ?");
    stmt.bind(1, carId);
    if (stmt.step() == SQLITE_ROW)
        return carFromStatement(stmt.handle());
    return std::nullopt;
}

std::optional<RentalDto> tryLoadRentalById(sqlite3* db, const std::string& rentalId)
{
    SqliteStatement stmt(
        db,
        "SELECT id, user_id, car_id, start_at, end_at, status, price_total, created_at, closed_at, payment_reference "
        "FROM rentals WHERE id = ?");
    stmt.bind(1, rentalId);
    if (stmt.step() == SQLITE_ROW)
        return rentalFromStatement(stmt.handle());
    return std::nullopt;
}

void validateCommonUserFields(const RegisterUserRequest& request)
{
    if (!std::regex_match(request.login, kLoginRegex))
        throw ApiException(400, "validation_error", "Login must contain 3-32 latin letters, digits or ._- characters.");
    if (request.password.size() < 6)
        throw ApiException(400, "validation_error", "Password must contain at least 6 characters.");
    if (trimCopy(request.firstName).empty() || trimCopy(request.lastName).empty())
        throw ApiException(400, "validation_error", "First name and last name are required.");
    if (!std::regex_match(request.email, kEmailRegex))
        throw ApiException(400, "validation_error", "Email must be valid.");
    if (!std::regex_match(request.phone, kPhoneRegex))
        throw ApiException(400, "validation_error", "Phone number must be valid.");
    if (trimCopy(request.driverLicenseNumber).size() < 5)
        throw ApiException(400, "validation_error", "Driver license number is required.");
}

void validateLoginRequest(const LoginRequest& request)
{
    if (trimCopy(request.login).empty() || trimCopy(request.password).empty())
        throw ApiException(400, "validation_error", "Login and password are required.");
}

void validateCarRequest(const CreateCarRequest& request)
{
    if (!std::regex_match(request.vin, kVinRegex))
        throw ApiException(400, "validation_error", "VIN must contain 11-17 valid VIN symbols.");
    if (trimCopy(request.brand).empty() || trimCopy(request.model).empty())
        throw ApiException(400, "validation_error", "Brand and model are required.");
    if (request.pricePerDay <= 0.0)
        throw ApiException(400, "validation_error", "Price per day must be greater than zero.");
}

void validateRentalRequest(const CreateRentalRequest& request)
{
    if (trimCopy(request.userId).empty() || trimCopy(request.carId).empty())
        throw ApiException(400, "validation_error", "userId and carId are required.");
    if (trimCopy(request.startAt).empty() || trimCopy(request.endAt).empty())
        throw ApiException(400, "validation_error", "startAt and endAt are required.");

    try
    {
        const auto start = parseUtcTimestamp(request.startAt);
        const auto end = parseUtcTimestamp(request.endAt);
        if ((end - start) <= 0)
            throw ApiException(400, "validation_error", "endAt must be after startAt.");
    }
    catch (const Poco::SyntaxException&)
    {
        throw ApiException(400, "validation_error", "Dates must be in UTC ISO-8601 format: YYYY-MM-DDTHH:MM:SSZ.");
    }
}

void ensureOwnerOrManager(const AuthenticatedUser& principal, const std::string& userId)
{
    if (!principal.authenticated)
        throw ApiException(401, "unauthorized", "Authentication required.");
    if (principal.role == Role::FleetManager)
        return;
    if (principal.id != userId)
        throw ApiException(403, "forbidden", "You are not allowed to access another user's rentals.");
}

void insertOutboxEvent(sqlite3* db, const std::string& aggregateId, const std::string& eventType, const RentalDto& rental)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("eventId", generateUuid());
    payload->set("rentalId", rental.id);
    payload->set("userId", rental.userId);
    payload->set("carId", rental.carId);
    payload->set("status", toString(rental.status));
    payload->set("createdAt", nowUtcIsoString());

    std::ostringstream buffer;
    Poco::JSON::Stringifier::stringify(payload, buffer);

    SqliteStatement stmt(
        db,
        "INSERT INTO outbox_events (id, aggregate_type, aggregate_id, event_type, payload, created_at) "
        "VALUES (?, 'rental', ?, ?, ?, ?)");
    stmt.bind(1, generateUuid());
    stmt.bind(2, aggregateId);
    stmt.bind(3, eventType);
    stmt.bind(4, buffer.str());
    stmt.bind(5, nowUtcIsoString());
    if (stmt.step() != SQLITE_DONE)
        throw std::runtime_error("Failed to insert outbox event.");
}

double calculateRentalPrice(const std::string& startAt, const std::string& endAt, double pricePerDay)
{
    const Poco::Timestamp start = parseUtcTimestamp(startAt);
    const Poco::Timestamp end = parseUtcTimestamp(endAt);
    const Poco::Timespan duration(end - start);
    const auto totalMicros = duration.totalMicroseconds();
    if (totalMicros <= 0)
        throw ApiException(400, "validation_error", "endAt must be after startAt.");

    constexpr long long microsPerDay = 24LL * 60 * 60 * 1000000;
    const long long billedDays = (totalMicros + microsPerDay - 1) / microsPerDay;
    return static_cast<double>(billedDays) * pricePerDay;
}

void rollbackQuietly(sqlite3* db)
{
    try
    {
        execSql(db, "ROLLBACK;");
    }
    catch (...)
    {
    }
}

} // namespace

bool DeterministicLicenseVerifier::verify(const std::string& driverLicenseNumber) const
{
    return upperCopy(driverLicenseNumber).rfind("INVALID", 0) != 0;
}

PaymentResult DeterministicPaymentGateway::preauthorize(const std::string& userId, double amount) const
{
    if (userId.find("blocked") != std::string::npos || amount <= 0.0)
        return PaymentResult{false, {}};
    return PaymentResult{true, "pay_" + generateUuid()};
}

UserService::UserService(const Database& database, const PasswordHasher& passwordHasher)
    : database_(database)
    , passwordHasher_(passwordHasher)
{
}

UserDto UserService::registerCustomer(const RegisterUserRequest& rawRequest) const
{
    RegisterUserRequest request = rawRequest;
    request.login = trimCopy(request.login);
    request.firstName = trimCopy(request.firstName);
    request.lastName = trimCopy(request.lastName);
    request.email = trimCopy(request.email);
    request.phone = trimCopy(request.phone);
    request.driverLicenseNumber = trimCopy(request.driverLicenseNumber);

    validateCommonUserFields(request);

    const std::string userId = generateUuid();
    const std::string salt = passwordHasher_.generateSalt();
    const std::string now = nowUtcIsoString();
    const std::string hash = passwordHasher_.hashPassword(request.password, salt);

    return database_.withConnection([&](sqlite3* db) {
        SqliteStatement stmt(
            db,
            "INSERT INTO users (id, login, password_salt, password_hash, first_name, last_name, email, phone, driver_license_number, role, created_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'CUSTOMER', ?)");
        stmt.bind(1, userId);
        stmt.bind(2, request.login);
        stmt.bind(3, salt);
        stmt.bind(4, hash);
        stmt.bind(5, request.firstName);
        stmt.bind(6, request.lastName);
        stmt.bind(7, request.email);
        stmt.bind(8, request.phone);
        stmt.bind(9, request.driverLicenseNumber);
        stmt.bind(10, now);

        const int rc = stmt.step();
        if (rc != SQLITE_DONE)
        {
            if (sqlite3_extended_errcode(db) == SQLITE_CONSTRAINT_UNIQUE)
                throw ApiException(409, "conflict", "User with this login already exists.");
            throw std::runtime_error("Failed to create user.");
        }

        return UserDto{userId, request.login, request.firstName, request.lastName, request.email, request.phone, request.driverLicenseNumber, Role::Customer, now};
    });
}

UserDto UserService::seedManager(const std::string& login, const std::string& password) const
{
    const std::string trimmedLogin = trimCopy(login);
    const std::string trimmedPassword = trimCopy(password);
    if (trimmedLogin.empty() || trimmedPassword.size() < 6)
        throw ApiException(400, "validation_error", "Seed manager credentials are invalid.");

    return database_.withConnection([&](sqlite3* db) -> UserDto {
        if (auto existing = tryLoadUserByLogin(db, trimmedLogin))
            return *existing;

        const std::string userId = generateUuid();
        const std::string salt = passwordHasher_.generateSalt();
        const std::string now = nowUtcIsoString();
        const std::string hash = passwordHasher_.hashPassword(trimmedPassword, salt);

        SqliteStatement stmt(
            db,
            "INSERT INTO users (id, login, password_salt, password_hash, first_name, last_name, email, phone, driver_license_number, role, created_at) "
            "VALUES (?, ?, ?, ?, 'Fleet', 'Manager', 'manager@example.com', '+10000000000', 'MANAGER-0001', 'FLEET_MANAGER', ?)");
        stmt.bind(1, userId);
        stmt.bind(2, trimmedLogin);
        stmt.bind(3, salt);
        stmt.bind(4, hash);
        stmt.bind(5, now);
        if (stmt.step() != SQLITE_DONE)
            throw std::runtime_error("Failed to seed fleet manager.");

        return UserDto{userId, trimmedLogin, "Fleet", "Manager", "manager@example.com", "+10000000000", "MANAGER-0001", Role::FleetManager, now};
    });
}

UserDto UserService::findByLogin(const std::string& login) const
{
    const std::string trimmedLogin = trimCopy(login);
    if (trimmedLogin.empty())
        throw ApiException(400, "validation_error", "Query parameter 'login' is required.");

    return database_.withConnection([&](sqlite3* db) -> UserDto {
        if (auto user = tryLoadUserByLogin(db, trimmedLogin))
            return *user;
        throw ApiException(404, "not_found", "User not found.");
    });
}

UserDto UserService::getById(const std::string& userId) const
{
    return database_.withConnection([&](sqlite3* db) -> UserDto {
        if (auto user = tryLoadUserById(db, userId))
            return *user;
        throw ApiException(404, "not_found", "User not found.");
    });
}

UserRecord UserService::getRecordByLogin(const std::string& login) const
{
    const std::string trimmedLogin = trimCopy(login);
    return database_.withConnection([&](sqlite3* db) -> UserRecord {
        if (auto record = tryLoadUserRecordByLogin(db, trimmedLogin))
            return *record;
        throw ApiException(404, "not_found", "User not found.");
    });
}

std::vector<UserDto> UserService::search(const std::string& nameMask, const std::string& surnameMask) const
{
    const std::string name = trimCopy(nameMask);
    const std::string surname = trimCopy(surnameMask);
    if (name.empty() && surname.empty())
        throw ApiException(400, "validation_error", "At least one of nameMask or surnameMask must be provided.");

    return database_.withConnection([&](sqlite3* db) {
        std::vector<UserDto> results;
        SqliteStatement stmt(
            db,
            "SELECT id, login, first_name, last_name, email, phone, driver_license_number, role, created_at "
            "FROM users WHERE first_name LIKE ? AND last_name LIKE ? ORDER BY created_at ASC");
        stmt.bind(1, name.empty() ? "%" : "%" + name + "%");
        stmt.bind(2, surname.empty() ? "%" : "%" + surname + "%");

        while (stmt.step() == SQLITE_ROW)
            results.push_back(userFromStatement(stmt.handle()));
        return results;
    });
}

AuthService::AuthService(const UserService& userService, const PasswordHasher& passwordHasher, const JwtService& jwtService)
    : userService_(userService)
    , passwordHasher_(passwordHasher)
    , jwtService_(jwtService)
{
}

LoginResponseDto AuthService::login(const LoginRequest& request) const
{
    validateLoginRequest(request);

    UserRecord record;
    try
    {
        record = userService_.getRecordByLogin(request.login);
    }
    catch (const ApiException&)
    {
        throw ApiException(401, "unauthorized", "Invalid login or password.");
    }

    if (!passwordHasher_.verify(request.password, record.passwordSalt, record.passwordHash))
        throw ApiException(401, "unauthorized", "Invalid login or password.");

    const AuthenticatedUser principal{record.user.id, record.user.login, record.user.role, true};
    return LoginResponseDto{jwtService_.issueToken(principal), "Bearer", jwtService_.ttlSeconds(), record.user};
}

AuthenticatedUser AuthService::authenticateAuthorizationHeader(const std::string& authorizationHeader) const
{
    constexpr const char* prefix = "Bearer ";
    if (authorizationHeader.rfind(prefix, 0) != 0)
        throw ApiException(401, "unauthorized", "Bearer token is required.");

    const auto token = authorizationHeader.substr(std::char_traits<char>::length(prefix));
    const TokenPayload payload = jwtService_.verifyToken(token);
    return AuthenticatedUser{payload.subject, payload.login, payload.role, true};
}

FleetService::FleetService(const Database& database)
    : database_(database)
{
}

CarDto FleetService::addCar(const CreateCarRequest& rawRequest) const
{
    CreateCarRequest request = rawRequest;
    request.vin = upperCopy(trimCopy(request.vin));
    request.brand = trimCopy(request.brand);
    request.model = trimCopy(request.model);

    validateCarRequest(request);

    const std::string carId = generateUuid();
    const std::string now = nowUtcIsoString();

    return database_.withConnection([&](sqlite3* db) {
        SqliteStatement stmt(
            db,
            "INSERT INTO cars (id, vin, brand, model, class, status, price_per_day, created_at) "
            "VALUES (?, ?, ?, ?, ?, 'AVAILABLE', ?, ?)");
        stmt.bind(1, carId);
        stmt.bind(2, request.vin);
        stmt.bind(3, request.brand);
        stmt.bind(4, request.model);
        stmt.bind(5, toString(request.carClass));
        stmt.bind(6, request.pricePerDay);
        stmt.bind(7, now);

        const int rc = stmt.step();
        if (rc != SQLITE_DONE)
        {
            if (sqlite3_extended_errcode(db) == SQLITE_CONSTRAINT_UNIQUE)
                throw ApiException(409, "conflict", "Car with this VIN already exists.");
            throw std::runtime_error("Failed to create car.");
        }

        return CarDto{carId, request.vin, request.brand, request.model, request.carClass, CarStatus::Available, request.pricePerDay, now};
    });
}

std::vector<CarDto> FleetService::listAvailable() const
{
    return database_.withConnection([&](sqlite3* db) {
        std::vector<CarDto> results;
        SqliteStatement stmt(
            db,
            "SELECT id, vin, brand, model, class, status, price_per_day, created_at "
            "FROM cars WHERE status = 'AVAILABLE' ORDER BY created_at ASC");

        while (stmt.step() == SQLITE_ROW)
            results.push_back(carFromStatement(stmt.handle()));
        return results;
    });
}

std::vector<CarDto> FleetService::searchByClass(CarClass carClass) const
{
    return database_.withConnection([&](sqlite3* db) {
        std::vector<CarDto> results;
        SqliteStatement stmt(
            db,
            "SELECT id, vin, brand, model, class, status, price_per_day, created_at "
            "FROM cars WHERE class = ? ORDER BY created_at ASC");
        stmt.bind(1, toString(carClass));

        while (stmt.step() == SQLITE_ROW)
            results.push_back(carFromStatement(stmt.handle()));
        return results;
    });
}

CarDto FleetService::getById(const std::string& carId) const
{
    return database_.withConnection([&](sqlite3* db) -> CarDto {
        if (auto car = tryLoadCarById(db, carId))
            return *car;
        throw ApiException(404, "not_found", "Car not found.");
    });
}

void FleetService::setStatus(const std::string& carId, CarStatus status) const
{
    database_.withConnection([&](sqlite3* db) {
        SqliteStatement stmt(db, "UPDATE cars SET status = ? WHERE id = ?");
        stmt.bind(1, toString(status));
        stmt.bind(2, carId);
        if (stmt.step() != SQLITE_DONE)
            throw std::runtime_error("Failed to update car status.");
        if (sqlite3_changes(db) == 0)
            throw ApiException(404, "not_found", "Car not found.");
    });
}

RentalService::RentalService(
    const Database& database,
    const UserService& userService,
    FleetService& fleetService,
    const LicenseVerifier& licenseVerifier,
    const PaymentGateway& paymentGateway)
    : database_(database)
    , userService_(userService)
    , fleetService_(fleetService)
    , licenseVerifier_(licenseVerifier)
    , paymentGateway_(paymentGateway)
{
}

RentalDto RentalService::createRental(const AuthenticatedUser& principal, const CreateRentalRequest& request) const
{
    if (!principal.authenticated)
        throw ApiException(401, "unauthorized", "Authentication required.");
    if (principal.role != Role::Customer)
        throw ApiException(403, "forbidden", "Only customers can create rentals.");
    if (principal.id != request.userId)
        throw ApiException(403, "forbidden", "Customer can create rental only for itself.");

    validateRentalRequest(request);

    return database_.withConnection([&](sqlite3* db) -> RentalDto {
        execSql(db, "BEGIN IMMEDIATE TRANSACTION;");

        try
        {
            const auto user = tryLoadUserById(db, request.userId);
            if (!user)
                throw ApiException(404, "not_found", "User not found.");

            const auto car = tryLoadCarById(db, request.carId);
            if (!car)
                throw ApiException(404, "not_found", "Car not found.");

            if (car->status != CarStatus::Available)
                throw ApiException(409, "conflict", "Car is not available for rental.");

            int overlappingCount = 0;
            SqliteStatement overlapStmt(
                db,
                "SELECT COUNT(*) FROM rentals "
                "WHERE car_id = ? AND status = 'ACTIVE' AND NOT (? >= end_at OR ? <= start_at)");
            overlapStmt.bind(1, request.carId);
            overlapStmt.bind(2, request.startAt);
            overlapStmt.bind(3, request.endAt);
            if (overlapStmt.step() == SQLITE_ROW)
                overlappingCount = columnInt(overlapStmt.handle(), 0);

            if (overlappingCount > 0)
                throw ApiException(409, "conflict", "Car already has an overlapping active rental.");

            if (!licenseVerifier_.verify(user->driverLicenseNumber))
                throw ApiException(409, "conflict", "Driver license verification failed.");

            const double priceTotal = calculateRentalPrice(request.startAt, request.endAt, car->pricePerDay);
            const PaymentResult payment = paymentGateway_.preauthorize(user->id, priceTotal);
            if (!payment.approved)
                throw ApiException(409, "conflict", "Payment preauthorization failed.");

            const std::string rentalId = generateUuid();
            const std::string now = nowUtcIsoString();

            SqliteStatement insertRental(
                db,
                "INSERT INTO rentals (id, user_id, car_id, start_at, end_at, status, price_total, created_at, closed_at, payment_reference) "
                "VALUES (?, ?, ?, ?, ?, 'ACTIVE', ?, ?, '', ?)");
            insertRental.bind(1, rentalId);
            insertRental.bind(2, request.userId);
            insertRental.bind(3, request.carId);
            insertRental.bind(4, request.startAt);
            insertRental.bind(5, request.endAt);
            insertRental.bind(6, priceTotal);
            insertRental.bind(7, now);
            insertRental.bind(8, payment.reference);
            if (insertRental.step() != SQLITE_DONE)
                throw std::runtime_error("Failed to insert rental.");

            SqliteStatement updateCar(db, "UPDATE cars SET status = 'IN_RENT' WHERE id = ?");
            updateCar.bind(1, request.carId);
            if (updateCar.step() != SQLITE_DONE || sqlite3_changes(db) == 0)
                throw std::runtime_error("Failed to update car status for rental.");

            const auto createdRental = tryLoadRentalById(db, rentalId);
            if (!createdRental)
                throw std::runtime_error("Created rental could not be loaded.");

            insertOutboxEvent(db, rentalId, "RentalCreated", *createdRental);
            execSql(db, "COMMIT;");
            return *createdRental;
        }
        catch (...)
        {
            rollbackQuietly(db);
            throw;
        }
    });
}

std::vector<RentalDto> RentalService::listActiveRentals(const AuthenticatedUser& principal, const std::string& userId) const
{
    ensureOwnerOrManager(principal, userId);
    userService_.getById(userId);

    return database_.withConnection([&](sqlite3* db) {
        std::vector<RentalDto> results;
        SqliteStatement stmt(
            db,
            "SELECT id, user_id, car_id, start_at, end_at, status, price_total, created_at, closed_at, payment_reference "
            "FROM rentals WHERE user_id = ? AND status = 'ACTIVE' ORDER BY created_at ASC");
        stmt.bind(1, userId);

        while (stmt.step() == SQLITE_ROW)
            results.push_back(rentalFromStatement(stmt.handle()));
        return results;
    });
}

std::vector<RentalDto> RentalService::rentalHistory(const AuthenticatedUser& principal, const std::string& userId) const
{
    ensureOwnerOrManager(principal, userId);
    userService_.getById(userId);

    return database_.withConnection([&](sqlite3* db) {
        std::vector<RentalDto> results;
        SqliteStatement stmt(
            db,
            "SELECT id, user_id, car_id, start_at, end_at, status, price_total, created_at, closed_at, payment_reference "
            "FROM rentals WHERE user_id = ? AND status IN ('COMPLETED', 'CANCELLED') ORDER BY created_at ASC");
        stmt.bind(1, userId);

        while (stmt.step() == SQLITE_ROW)
            results.push_back(rentalFromStatement(stmt.handle()));
        return results;
    });
}

RentalDto RentalService::completeRental(const AuthenticatedUser& principal, const std::string& rentalId) const
{
    if (!principal.authenticated)
        throw ApiException(401, "unauthorized", "Authentication required.");

    return database_.withConnection([&](sqlite3* db) -> RentalDto {
        execSql(db, "BEGIN IMMEDIATE TRANSACTION;");

        try
        {
            const auto rental = tryLoadRentalById(db, rentalId);
            if (!rental)
                throw ApiException(404, "not_found", "Rental not found.");

            if (principal.role != Role::FleetManager && principal.id != rental->userId)
                throw ApiException(403, "forbidden", "You are not allowed to complete this rental.");

            if (rental->status != RentalStatus::Active)
                throw ApiException(409, "conflict", "Rental has already been completed.");

            SqliteStatement updateRental(
                db,
                "UPDATE rentals SET status = 'COMPLETED', closed_at = ? WHERE id = ?");
            const std::string closedAt = nowUtcIsoString();
            updateRental.bind(1, closedAt);
            updateRental.bind(2, rentalId);
            if (updateRental.step() != SQLITE_DONE || sqlite3_changes(db) == 0)
                throw std::runtime_error("Failed to update rental.");

            SqliteStatement updateCar(db, "UPDATE cars SET status = 'AVAILABLE' WHERE id = ?");
            updateCar.bind(1, rental->carId);
            if (updateCar.step() != SQLITE_DONE || sqlite3_changes(db) == 0)
                throw std::runtime_error("Failed to update car status on completion.");

            const auto completedRental = tryLoadRentalById(db, rentalId);
            if (!completedRental)
                throw std::runtime_error("Completed rental could not be loaded.");

            insertOutboxEvent(db, rentalId, "RentalCompleted", *completedRental);
            execSql(db, "COMMIT;");
            return *completedRental;
        }
        catch (...)
        {
            rollbackQuietly(db);
            throw;
        }
    });
}

} // namespace car_rental
