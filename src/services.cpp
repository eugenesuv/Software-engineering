#include "car_rental/services.h"

#include "car_rental/errors.h"
#include "car_rental/utils.h"

#include <Poco/Exception.h>
#include <Poco/Timespan.h>

#include <regex>

namespace car_rental {
namespace {

const std::regex kLoginRegex(R"(^[A-Za-z0-9_.-]{3,32}$)");
const std::regex kEmailRegex(R"(^[^@\s]+@[^@\s]+\.[^@\s]+$)");
const std::regex kPhoneRegex(R"(^[+0-9() -]{7,20}$)");
const std::regex kVinRegex(R"(^[A-HJ-NPR-Z0-9]{11,17}$)");

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

UserService::UserService(UserStore& store, const PasswordHasher& passwordHasher)
    : store_(store)
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
    return store_.createCustomer(request, userId, salt, hash, now);
}

UserDto UserService::seedManager(const std::string& login, const std::string& password) const
{
    const std::string trimmedLogin = trimCopy(login);
    const std::string trimmedPassword = trimCopy(password);
    if (trimmedLogin.empty() || trimmedPassword.size() < 6)
        throw ApiException(400, "validation_error", "Seed manager credentials are invalid.");

    const std::string salt = passwordHasher_.generateSalt();
    const std::string now = nowUtcIsoString();
    const std::string hash = passwordHasher_.hashPassword(trimmedPassword, salt);
    return store_.upsertManager(trimmedLogin, salt, hash, now);
}

UserDto UserService::findByLogin(const std::string& login) const
{
    const std::string trimmedLogin = trimCopy(login);
    if (trimmedLogin.empty())
        throw ApiException(400, "validation_error", "Query parameter 'login' is required.");

    if (auto user = store_.findByLogin(trimmedLogin))
        return *user;
    throw ApiException(404, "not_found", "User not found.");
}

UserDto UserService::getById(const std::string& userId) const
{
    if (auto user = store_.findById(userId))
        return *user;
    throw ApiException(404, "not_found", "User not found.");
}

UserRecord UserService::getRecordByLogin(const std::string& login) const
{
    const std::string trimmedLogin = trimCopy(login);
    if (auto record = store_.findRecordByLogin(trimmedLogin))
        return *record;
    throw ApiException(404, "not_found", "User not found.");
}

std::vector<UserDto> UserService::search(const std::string& nameMask, const std::string& surnameMask) const
{
    const std::string name = trimCopy(nameMask);
    const std::string surname = trimCopy(surnameMask);
    if (name.empty() && surname.empty())
        throw ApiException(400, "validation_error", "At least one of nameMask or surnameMask must be provided.");

    return store_.search(name, surname);
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

FleetService::FleetService(FleetStore& store)
    : store_(store)
{
}

CarDto FleetService::addCar(const CreateCarRequest& rawRequest) const
{
    CreateCarRequest request = rawRequest;
    request.vin = upperCopy(trimCopy(request.vin));
    request.brand = trimCopy(request.brand);
    request.model = trimCopy(request.model);

    validateCarRequest(request);
    return store_.addCar(request, generateUuid(), nowUtcIsoString());
}

std::vector<CarDto> FleetService::listAvailable() const
{
    return store_.listAvailable();
}

std::vector<CarDto> FleetService::searchByClass(CarClass carClass) const
{
    return store_.searchByClass(carClass);
}

CarDto FleetService::getById(const std::string& carId) const
{
    if (auto car = store_.findById(carId))
        return *car;
    throw ApiException(404, "not_found", "Car not found.");
}

void FleetService::setStatus(const std::string& carId, CarStatus status) const
{
    store_.updateStatus(carId, status);
}

RentalService::RentalService(
    RentalStore& rentalStore,
    RentalWorkflowCoordinator& rentalWorkflowCoordinator,
    const UserService& userService,
    FleetService& fleetService,
    const LicenseVerifier& licenseVerifier,
    const PaymentGateway& paymentGateway)
    : rentalStore_(rentalStore)
    , rentalWorkflowCoordinator_(rentalWorkflowCoordinator)
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

    const UserDto user = userService_.getById(request.userId);
    const CarDto car = fleetService_.getById(request.carId);
    if (car.status != CarStatus::Available)
        throw ApiException(409, "conflict", "Car is not available for rental.");

    if (!licenseVerifier_.verify(user.driverLicenseNumber))
        throw ApiException(409, "conflict", "Driver license verification failed.");

    const double priceTotal = calculateRentalPrice(request.startAt, request.endAt, car.pricePerDay);
    const PaymentResult payment = paymentGateway_.preauthorize(user.id, priceTotal);
    if (!payment.approved)
        throw ApiException(409, "conflict", "Payment preauthorization failed.");

    return rentalWorkflowCoordinator_.createActiveRental(RentalWriteRequest{
        generateUuid(),
        request.userId,
        request.carId,
        user,
        car,
        request.startAt,
        request.endAt,
        priceTotal,
        nowUtcIsoString(),
        payment.reference});
}

std::vector<RentalDto> RentalService::listActiveRentals(const AuthenticatedUser& principal, const std::string& userId) const
{
    ensureOwnerOrManager(principal, userId);
    userService_.getById(userId);
    return rentalStore_.listActiveByUser(userId);
}

std::vector<RentalDto> RentalService::rentalHistory(const AuthenticatedUser& principal, const std::string& userId) const
{
    ensureOwnerOrManager(principal, userId);
    userService_.getById(userId);
    return rentalStore_.listHistoryByUser(userId);
}

RentalDto RentalService::completeRental(const AuthenticatedUser& principal, const std::string& rentalId) const
{
    if (!principal.authenticated)
        throw ApiException(401, "unauthorized", "Authentication required.");

    const auto rental = rentalStore_.findById(rentalId);
    if (!rental)
        throw ApiException(404, "not_found", "Rental not found.");

    if (principal.role != Role::FleetManager && principal.id != rental->userId)
        throw ApiException(403, "forbidden", "You are not allowed to complete this rental.");
    if (rental->status != RentalStatus::Active)
        throw ApiException(409, "conflict", "Rental has already been completed.");

    return rentalWorkflowCoordinator_.completeRental(RentalCompletionRequest{rentalId, nowUtcIsoString()});
}

} // namespace car_rental
