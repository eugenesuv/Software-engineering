#include "test_support.h"

#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StreamCopier.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <sqlite3.h>

namespace {

std::atomic<int> gSequence{0};

std::string buildCustomerPayload(const std::string& login, const std::string& password)
{
    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("login", login);
    object->set("password", password);
    object->set("firstName", "Test");
    object->set("lastName", "User");
    object->set("email", login + "@example.com");
    object->set("phone", "+79000000000");
    object->set("driverLicenseNumber", "DL-" + login);

    std::ostringstream buffer;
    Poco::JSON::Stringifier::stringify(object, buffer);
    return buffer.str();
}

std::string buildCarPayload(const std::string& vin, car_rental::CarClass carClass)
{
    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("vin", vin);
    object->set("brand", "Toyota");
    object->set("model", "Corolla");
    object->set("class", car_rental::toString(carClass));
    object->set("pricePerDay", 150.0);

    std::ostringstream buffer;
    Poco::JSON::Stringifier::stringify(object, buffer);
    return buffer.str();
}

} // namespace

TestServer::TestServer()
{
    const int sequence = nextSequence();
    const auto tempDir = std::filesystem::temp_directory_path();

    config_.host = "127.0.0.1";
    config_.port = 0;
    config_.databasePath = (tempDir / ("car_rental_test_" + std::to_string(sequence) + ".db")).string();
    config_.jwtSecret = "test-secret";
    config_.jwtTtlSeconds = 3600;
    config_.managerLogin = "manager";
    config_.managerPassword = "Manager123!";

    server_ = std::make_unique<car_rental::ApiServer>(config_);
    server_->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

TestServer::~TestServer()
{
    if (server_)
        server_->stop();
    std::error_code ignored;
    std::filesystem::remove(config_.databasePath, ignored);
}

HttpResult TestServer::request(const std::string& method, const std::string& path, const std::string& body, const std::string& token) const
{
    Poco::Net::HTTPClientSession session(config_.host, server_->port());
    Poco::Net::HTTPRequest request(method, path, Poco::Net::HTTPRequest::HTTP_1_1);
    request.setContentType("application/json");
    if (!token.empty())
        request.set("Authorization", "Bearer " + token);
    request.setContentLength(static_cast<int>(body.size()));

    std::ostream& requestStream = session.sendRequest(request);
    if (!body.empty())
        requestStream << body;

    Poco::Net::HTTPResponse response;
    std::istream& responseStream = session.receiveResponse(response);
    std::ostringstream responseBody;
    Poco::StreamCopier::copyStream(responseStream, responseBody);

    return HttpResult{response.getStatus(), responseBody.str(), response.getContentType()};
}

Poco::JSON::Object::Ptr TestServer::parseObject(const std::string& payload) const
{
    Poco::JSON::Parser parser;
    return parser.parse(payload).extract<Poco::JSON::Object::Ptr>();
}

Poco::JSON::Array::Ptr TestServer::parseArray(const std::string& payload) const
{
    Poco::JSON::Parser parser;
    return parser.parse(payload).extract<Poco::JSON::Array::Ptr>();
}

RegisteredUserFixture TestServer::createCustomer(const std::string& prefix)
{
    RegisteredUserFixture fixture;
    fixture.login = uniqueLogin(prefix);
    fixture.password = "Secret123!";

    const HttpResult registration = request("POST", "/users", buildCustomerPayload(fixture.login, fixture.password));
    if (registration.status != 201)
        throw std::runtime_error("Failed to register customer fixture.");
    const auto registered = parseObject(registration.body);
    fixture.id = registered->getValue<std::string>("id");
    fixture.token = login(fixture.login, fixture.password);
    return fixture;
}

CarFixture TestServer::createCar(car_rental::CarClass carClass)
{
    CarFixture fixture;
    fixture.vin = uniqueVin();
    const HttpResult creation = request("POST", "/cars", buildCarPayload(fixture.vin, carClass), managerToken());
    if (creation.status != 201)
        throw std::runtime_error("Failed to create car fixture.");
    fixture.id = parseObject(creation.body)->getValue<std::string>("id");
    return fixture;
}

std::string TestServer::login(const std::string& loginValue, const std::string& password) const
{
    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("login", loginValue);
    object->set("password", password);

    const HttpResult response = request("POST", "/auth/login", stringify(object));
    if (response.status != 200)
        throw std::runtime_error("Failed to login fixture user.");
    return parseObject(response.body)->getValue<std::string>("accessToken");
}

std::string TestServer::managerToken() const
{
    return login(config_.managerLogin, config_.managerPassword);
}

std::string TestServer::makeTokenFor(const std::string& userId, car_rental::Role role, const std::string& loginValue, long ttlSeconds) const
{
    car_rental::JwtService jwt(config_.jwtSecret, ttlSeconds);
    return jwt.issueToken(car_rental::AuthenticatedUser{userId, loginValue, role, true});
}

int TestServer::scalarInt(const std::string& sql) const
{
    sqlite3* db = nullptr;
    if (sqlite3_open(config_.databasePath.c_str(), &db) != SQLITE_OK)
        throw std::runtime_error("Failed to open sqlite database.");

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), static_cast<int>(sql.size()), &stmt, nullptr) != SQLITE_OK)
    {
        sqlite3_close(db);
        throw std::runtime_error("Failed to prepare sqlite statement.");
    }

    int result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}

std::string TestServer::scalarText(const std::string& sql) const
{
    sqlite3* db = nullptr;
    if (sqlite3_open(config_.databasePath.c_str(), &db) != SQLITE_OK)
        throw std::runtime_error("Failed to open sqlite database.");

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), static_cast<int>(sql.size()), &stmt, nullptr) != SQLITE_OK)
    {
        sqlite3_close(db);
        throw std::runtime_error("Failed to prepare sqlite statement.");
    }

    std::string result;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const auto* raw = sqlite3_column_text(stmt, 0);
        if (raw != nullptr)
            result = reinterpret_cast<const char*>(raw);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}

std::string TestServer::uniqueLogin(const std::string& prefix) const
{
    return prefix + std::to_string(nextSequence());
}

std::string TestServer::uniqueVin() const
{
    const auto sequence = static_cast<unsigned>(nextSequence());
    std::ostringstream vin;
    vin << "CAR";
    vin.width(14);
    vin.fill('0');
    vin << sequence;
    return vin.str();
}

const car_rental::ServerConfig& TestServer::config() const
{
    return config_;
}

std::string TestServer::stringify(const Poco::JSON::Object::Ptr& object) const
{
    std::ostringstream buffer;
    Poco::JSON::Stringifier::stringify(object, buffer);
    return buffer.str();
}

int TestServer::nextSequence()
{
    return ++gSequence;
}
