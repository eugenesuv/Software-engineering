#include "test_support.h"

#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/URI.h>

#include <pqxx/pqxx>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace {

std::atomic<int> gSequence{0};

std::string adminDatabaseUrl()
{
    if (const char* value = std::getenv("CAR_RENTAL_TEST_ADMIN_URL"))
    {
        if (*value != '\0')
            return value;
    }
    return "postgresql://postgres:postgres@127.0.0.1:5432/postgres";
}

std::string databaseUrlFor(const std::string& adminUrl, const std::string& databaseName)
{
    Poco::URI uri(adminUrl);
    uri.setPath("/" + databaseName);
    return uri.toString();
}

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("Failed to open file: " + path.string());

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void recreateDatabase(const std::string& adminUrl, const std::string& databaseName)
{
    pqxx::connection connection(adminUrl);
    pqxx::nontransaction tx(connection);
    tx.exec0("DROP DATABASE IF EXISTS " + tx.quote_name(databaseName) + " WITH (FORCE)");
    tx.exec0("CREATE DATABASE " + tx.quote_name(databaseName));
}

void dropDatabase(const std::string& adminUrl, const std::string& databaseName)
{
    pqxx::connection connection(adminUrl);
    pqxx::nontransaction tx(connection);
    tx.exec0("DROP DATABASE IF EXISTS " + tx.quote_name(databaseName) + " WITH (FORCE)");
}

void applySqlScript(const std::string& databaseUrl, const std::string& sql)
{
    pqxx::connection connection(databaseUrl);
    pqxx::nontransaction tx(connection);
    tx.exec(sql);
}

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
    const std::filesystem::path root = std::filesystem::path(__FILE__).parent_path().parent_path();

    config_.host = "127.0.0.1";
    config_.port = 0;
    adminDatabaseUrl_ = adminDatabaseUrl();
    databaseName_ = "car_rental_test_" + std::to_string(sequence);
    config_.databaseUrl = databaseUrlFor(adminDatabaseUrl_, databaseName_);
    config_.jwtSecret = "test-secret";
    config_.jwtTtlSeconds = 3600;
    config_.managerLogin = "manager";
    config_.managerPassword = "Manager123!";

    recreateDatabase(adminDatabaseUrl_, databaseName_);
    applySqlScript(config_.databaseUrl, readTextFile(root / "schema.sql"));

    server_ = std::make_unique<car_rental::ApiServer>(config_);
    server_->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

TestServer::~TestServer()
{
    if (server_)
        server_->stop();
    if (!databaseName_.empty() && !adminDatabaseUrl_.empty())
        dropDatabase(adminDatabaseUrl_, databaseName_);
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
    pqxx::connection connection(config_.databaseUrl);
    pqxx::read_transaction tx(connection);
    const pqxx::result result = tx.exec(sql);
    if (result.empty() || result[0].empty())
        return 0;
    return result[0][0].as<int>();
}

std::string TestServer::scalarText(const std::string& sql) const
{
    pqxx::connection connection(config_.databaseUrl);
    pqxx::read_transaction tx(connection);
    const pqxx::result result = tx.exec(sql);
    if (result.empty() || result[0].empty() || result[0][0].is_null())
        return {};
    return result[0][0].c_str();
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
