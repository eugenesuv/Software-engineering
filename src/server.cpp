#include "car_rental/server.h"

#include "car_rental/errors.h"
#include "car_rental/utils.h"

#include <Poco/Dynamic/Var.h>
#include <Poco/Exception.h>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/StreamCopier.h>
#include <Poco/URI.h>

#include <memory>
#include <regex>
#include <sstream>
#include <utility>

namespace car_rental {
namespace {

struct ApiDependencies
{
    AuthService* auth{nullptr};
    UserService* users{nullptr};
    FleetService* fleet{nullptr};
    RentalService* rentals{nullptr};
};

Poco::JSON::Object::Ptr toJson(const UserDto& user)
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

Poco::JSON::Object::Ptr toJson(const CarDto& car)
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

Poco::JSON::Object::Ptr toJson(const RentalDto& rental)
{
    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("id", rental.id);
    object->set("userId", rental.userId);
    object->set("carId", rental.carId);
    object->set("startAt", rental.startAt);
    object->set("endAt", rental.endAt);
    object->set("status", toString(rental.status));
    object->set("priceTotal", rental.priceTotal);
    object->set("createdAt", rental.createdAt);
    object->set("closedAt", rental.closedAt);
    object->set("paymentReference", rental.paymentReference);
    return object;
}

Poco::JSON::Object::Ptr toJson(const LoginResponseDto& loginResponse)
{
    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("accessToken", loginResponse.accessToken);
    object->set("tokenType", loginResponse.tokenType);
    object->set("expiresIn", loginResponse.expiresIn);
    object->set("user", toJson(loginResponse.user));
    return object;
}

template <typename T>
Poco::JSON::Array::Ptr toArray(const std::vector<T>& items)
{
    Poco::JSON::Array::Ptr array = new Poco::JSON::Array;
    for (const auto& item : items)
        array->add(toJson(item));
    return array;
}

Poco::JSON::Object::Ptr errorJson(const ApiException& exception)
{
    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("error", exception.errorCode());
    object->set("message", exception.what());
    object->set("details", exception.details());
    return object;
}

void sendJson(Poco::Net::HTTPServerResponse& response, Poco::Net::HTTPResponse::HTTPStatus status, const Poco::JSON::Object::Ptr& object)
{
    response.setStatus(status);
    response.setContentType("application/json");
    response.setChunkedTransferEncoding(false);
    std::ostream& output = response.send();
    Poco::JSON::Stringifier::stringify(object, output);
}

void sendJson(Poco::Net::HTTPServerResponse& response, Poco::Net::HTTPResponse::HTTPStatus status, const Poco::JSON::Array::Ptr& array)
{
    response.setStatus(status);
    response.setContentType("application/json");
    response.setChunkedTransferEncoding(false);
    std::ostream& output = response.send();
    Poco::JSON::Stringifier::stringify(array, output);
}

std::string readRequestBody(Poco::Net::HTTPServerRequest& request)
{
    std::ostringstream buffer;
    Poco::StreamCopier::copyStream(request.stream(), buffer);
    return buffer.str();
}

Poco::JSON::Object::Ptr parseBodyObject(Poco::Net::HTTPServerRequest& request)
{
    const std::string body = readRequestBody(request);
    if (trimCopy(body).empty())
        throw ApiException(400, "validation_error", "Request body must not be empty.");

    try
    {
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var parsed = parser.parse(body);
        return parsed.extract<Poco::JSON::Object::Ptr>();
    }
    catch (const Poco::Exception&)
    {
        throw ApiException(400, "validation_error", "Request body must contain a valid JSON object.");
    }
}

std::string requireString(const Poco::JSON::Object::Ptr& object, const std::string& fieldName)
{
    try
    {
        if (!object->has(fieldName))
            throw ApiException(400, "validation_error", "Missing required field: " + fieldName + ".");
        return trimCopy(object->getValue<std::string>(fieldName));
    }
    catch (const Poco::BadCastException&)
    {
        throw ApiException(400, "validation_error", "Field '" + fieldName + "' must be a string.");
    }
}

double requireDouble(const Poco::JSON::Object::Ptr& object, const std::string& fieldName)
{
    try
    {
        if (!object->has(fieldName))
            throw ApiException(400, "validation_error", "Missing required field: " + fieldName + ".");
        return object->getValue<double>(fieldName);
    }
    catch (const Poco::BadCastException&)
    {
        throw ApiException(400, "validation_error", "Field '" + fieldName + "' must be numeric.");
    }
}

std::string queryParam(const Poco::URI& uri, const std::string& name)
{
    for (const auto& parameter : uri.getQueryParameters())
    {
        if (parameter.first == name)
            return parameter.second;
    }
    return {};
}

RegisterUserRequest parseRegisterRequest(const Poco::JSON::Object::Ptr& object)
{
    return RegisterUserRequest{
        requireString(object, "login"),
        requireString(object, "password"),
        requireString(object, "firstName"),
        requireString(object, "lastName"),
        requireString(object, "email"),
        requireString(object, "phone"),
        requireString(object, "driverLicenseNumber")};
}

LoginRequest parseLoginRequest(const Poco::JSON::Object::Ptr& object)
{
    return LoginRequest{requireString(object, "login"), requireString(object, "password")};
}

CreateCarRequest parseCreateCarRequest(const Poco::JSON::Object::Ptr& object)
{
    try
    {
        return CreateCarRequest{
            requireString(object, "vin"),
            requireString(object, "brand"),
            requireString(object, "model"),
            carClassFromString(upperCopy(requireString(object, "class"))),
            requireDouble(object, "pricePerDay")};
    }
    catch (const std::invalid_argument&)
    {
        throw ApiException(400, "validation_error", "Field 'class' must be one of ECONOMY, COMFORT, BUSINESS, SUV, LUXURY.");
    }
}

CreateRentalRequest parseCreateRentalRequest(const Poco::JSON::Object::Ptr& object)
{
    return CreateRentalRequest{
        requireString(object, "userId"),
        requireString(object, "carId"),
        requireString(object, "startAt"),
        requireString(object, "endAt")};
}

AuthenticatedUser requireAuthenticated(const Poco::Net::HTTPServerRequest& request, const ApiDependencies& dependencies)
{
    return dependencies.auth->authenticateAuthorizationHeader(request.get("Authorization", ""));
}

class ApiRequestHandler final : public Poco::Net::HTTPRequestHandler
{
public:
    explicit ApiRequestHandler(ApiDependencies dependencies)
        : dependencies_(std::move(dependencies))
    {
    }

    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override
    {
        try
        {
            const Poco::URI uri(request.getURI());
            const std::string path = uri.getPath().empty() ? "/" : uri.getPath();
            const std::string& method = request.getMethod();

            if (method == Poco::Net::HTTPRequest::HTTP_GET && path == "/health")
            {
                Poco::JSON::Object::Ptr health = new Poco::JSON::Object;
                health->set("status", "ok");
                sendJson(response, Poco::Net::HTTPResponse::HTTP_OK, health);
                return;
            }

            if (method == Poco::Net::HTTPRequest::HTTP_POST && path == "/users")
            {
                const UserDto user = dependencies_.users->registerCustomer(parseRegisterRequest(parseBodyObject(request)));
                sendJson(response, Poco::Net::HTTPResponse::HTTP_CREATED, toJson(user));
                return;
            }

            if (method == Poco::Net::HTTPRequest::HTTP_POST && path == "/auth/login")
            {
                const LoginResponseDto loginResponse = dependencies_.auth->login(parseLoginRequest(parseBodyObject(request)));
                sendJson(response, Poco::Net::HTTPResponse::HTTP_OK, toJson(loginResponse));
                return;
            }

            if (method == Poco::Net::HTTPRequest::HTTP_GET && path == "/users/by-login")
            {
                (void)requireAuthenticated(request, dependencies_);
                const UserDto user = dependencies_.users->findByLogin(queryParam(uri, "login"));
                sendJson(response, Poco::Net::HTTPResponse::HTTP_OK, toJson(user));
                return;
            }

            if (method == Poco::Net::HTTPRequest::HTTP_GET && path == "/users/search")
            {
                (void)requireAuthenticated(request, dependencies_);
                const auto users = dependencies_.users->search(queryParam(uri, "nameMask"), queryParam(uri, "surnameMask"));
                sendJson(response, Poco::Net::HTTPResponse::HTTP_OK, toArray(users));
                return;
            }

            if (method == Poco::Net::HTTPRequest::HTTP_POST && path == "/cars")
            {
                const auto principal = requireAuthenticated(request, dependencies_);
                if (principal.role != Role::FleetManager)
                    throw ApiException(403, "forbidden", "Only fleet manager can add cars.");

                const CarDto car = dependencies_.fleet->addCar(parseCreateCarRequest(parseBodyObject(request)));
                sendJson(response, Poco::Net::HTTPResponse::HTTP_CREATED, toJson(car));
                return;
            }

            if (method == Poco::Net::HTTPRequest::HTTP_GET && path == "/cars/available")
            {
                const auto cars = dependencies_.fleet->listAvailable();
                sendJson(response, Poco::Net::HTTPResponse::HTTP_OK, toArray(cars));
                return;
            }

            if (method == Poco::Net::HTTPRequest::HTTP_GET && path == "/cars/search")
            {
                try
                {
                    const auto cars = dependencies_.fleet->searchByClass(carClassFromString(upperCopy(queryParam(uri, "class"))));
                    sendJson(response, Poco::Net::HTTPResponse::HTTP_OK, toArray(cars));
                    return;
                }
                catch (const std::invalid_argument&)
                {
                    throw ApiException(400, "validation_error", "Query parameter 'class' must be one of ECONOMY, COMFORT, BUSINESS, SUV, LUXURY.");
                }
            }

            if (method == Poco::Net::HTTPRequest::HTTP_POST && path == "/rentals")
            {
                const auto principal = requireAuthenticated(request, dependencies_);
                const RentalDto rental = dependencies_.rentals->createRental(principal, parseCreateRentalRequest(parseBodyObject(request)));
                sendJson(response, Poco::Net::HTTPResponse::HTTP_CREATED, toJson(rental));
                return;
            }

            static const std::regex activePattern(R"(^/users/([^/]+)/rentals/active$)");
            static const std::regex historyPattern(R"(^/users/([^/]+)/rentals/history$)");
            static const std::regex completePattern(R"(^/rentals/([^/]+)/complete$)");
            std::smatch match;

            if (method == Poco::Net::HTTPRequest::HTTP_GET && std::regex_match(path, match, activePattern))
            {
                const auto principal = requireAuthenticated(request, dependencies_);
                const auto rentals = dependencies_.rentals->listActiveRentals(principal, match[1].str());
                sendJson(response, Poco::Net::HTTPResponse::HTTP_OK, toArray(rentals));
                return;
            }

            if (method == Poco::Net::HTTPRequest::HTTP_GET && std::regex_match(path, match, historyPattern))
            {
                const auto principal = requireAuthenticated(request, dependencies_);
                const auto rentals = dependencies_.rentals->rentalHistory(principal, match[1].str());
                sendJson(response, Poco::Net::HTTPResponse::HTTP_OK, toArray(rentals));
                return;
            }

            if (method == Poco::Net::HTTPRequest::HTTP_POST && std::regex_match(path, match, completePattern))
            {
                const auto principal = requireAuthenticated(request, dependencies_);
                const RentalDto rental = dependencies_.rentals->completeRental(principal, match[1].str());
                sendJson(response, Poco::Net::HTTPResponse::HTTP_OK, toJson(rental));
                return;
            }

            throw ApiException(404, "not_found", "Endpoint not found.");
        }
        catch (const ApiException& exception)
        {
            sendJson(
                response,
                static_cast<Poco::Net::HTTPResponse::HTTPStatus>(exception.status()),
                errorJson(exception));
        }
        catch (const Poco::Exception& exception)
        {
            ApiException apiException(500, "internal_error", "Internal server error.", exception.displayText());
            sendJson(response, Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR, errorJson(apiException));
        }
        catch (const std::exception& exception)
        {
            ApiException apiException(500, "internal_error", "Internal server error.", exception.what());
            sendJson(response, Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR, errorJson(apiException));
        }
    }

private:
    ApiDependencies dependencies_;
};

class ApiHandlerFactory final : public Poco::Net::HTTPRequestHandlerFactory
{
public:
    explicit ApiHandlerFactory(ApiDependencies dependencies)
        : dependencies_(std::move(dependencies))
    {
    }

    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override
    {
        return new ApiRequestHandler(dependencies_);
    }

private:
    ApiDependencies dependencies_;
};

} // namespace

ApiServer::ApiServer(ServerConfig config)
    : config_(std::move(config))
    , database_(std::make_unique<Database>(config_.databasePath))
    , passwordHasher_(std::make_unique<PasswordHasher>())
    , jwtService_(std::make_unique<JwtService>(config_.jwtSecret, config_.jwtTtlSeconds))
    , licenseVerifier_(std::make_shared<DeterministicLicenseVerifier>())
    , paymentGateway_(std::make_shared<DeterministicPaymentGateway>())
    , userService_(std::make_unique<UserService>(*database_, *passwordHasher_))
    , authService_(std::make_unique<AuthService>(*userService_, *passwordHasher_, *jwtService_))
    , fleetService_(std::make_unique<FleetService>(*database_))
    , rentalService_(std::make_unique<RentalService>(*database_, *userService_, *fleetService_, *licenseVerifier_, *paymentGateway_))
{
}

ApiServer::~ApiServer()
{
    stop();
}

void ApiServer::start()
{
    database_->initializeSchema();
    userService_->seedManager(config_.managerLogin, config_.managerPassword);

    Poco::Net::ServerSocket socket(Poco::Net::SocketAddress(config_.host, config_.port));
    actualPort_ = socket.address().port();

    auto params = new Poco::Net::HTTPServerParams;
    params->setMaxQueued(64);
    params->setMaxThreads(8);

    ApiDependencies dependencies{authService_.get(), userService_.get(), fleetService_.get(), rentalService_.get()};
    server_ = std::make_unique<Poco::Net::HTTPServer>(new ApiHandlerFactory(dependencies), socket, params);
    server_->start();
}

void ApiServer::stop()
{
    if (server_)
    {
        server_->stopAll();
        server_.reset();
    }
}

std::uint16_t ApiServer::port() const noexcept
{
    return actualPort_;
}

const ServerConfig& ApiServer::config() const noexcept
{
    return config_;
}

} // namespace car_rental
