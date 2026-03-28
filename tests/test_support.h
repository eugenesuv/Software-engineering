#pragma once

#include "car_rental/models.h"
#include "car_rental/server.h"
#include "car_rental/security.h"

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>

#include <memory>
#include <string>

struct HttpResult
{
    int status{};
    std::string body;
    std::string contentType;
};

struct RegisteredUserFixture
{
    std::string id;
    std::string login;
    std::string password;
    std::string token;
};

struct CarFixture
{
    std::string id;
    std::string vin;
};

class TestServer
{
public:
    TestServer();
    ~TestServer();

    HttpResult request(const std::string& method, const std::string& path, const std::string& body = "", const std::string& token = "") const;
    Poco::JSON::Object::Ptr parseObject(const std::string& payload) const;
    Poco::JSON::Array::Ptr parseArray(const std::string& payload) const;

    RegisteredUserFixture createCustomer(const std::string& prefix = "customer");
    CarFixture createCar(car_rental::CarClass carClass = car_rental::CarClass::Economy);
    std::string login(const std::string& login, const std::string& password) const;
    std::string managerToken() const;
    std::string makeTokenFor(const std::string& userId, car_rental::Role role, const std::string& login, long ttlSeconds = 3600) const;

    int scalarInt(const std::string& sql) const;
    std::string scalarText(const std::string& sql) const;

    std::string uniqueLogin(const std::string& prefix = "customer") const;
    std::string uniqueVin() const;
    const car_rental::ServerConfig& config() const;

private:
    std::string stringify(const Poco::JSON::Object::Ptr& object) const;
    static int nextSequence();

    car_rental::ServerConfig config_;
    std::unique_ptr<car_rental::ApiServer> server_;
};
