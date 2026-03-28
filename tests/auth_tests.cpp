#include "test_support.h"

#include <Poco/JSON/Object.h>
#include <Poco/JSON/Stringifier.h>

#include <doctest/doctest.h>

#include <sstream>

namespace {

std::string json(const Poco::JSON::Object::Ptr& object)
{
    std::ostringstream buffer;
    Poco::JSON::Stringifier::stringify(object, buffer);
    return buffer.str();
}

} // namespace

TEST_SUITE("auth")
{
    TEST_CASE("login returns jwt for registered customer")
    {
        TestServer server;
        const auto customer = server.createCustomer();

        CHECK_FALSE(customer.token.empty());
    }

    TEST_CASE("login rejects wrong password")
    {
        TestServer server;
        const auto customer = server.createCustomer();

        Poco::JSON::Object::Ptr body = new Poco::JSON::Object;
        body->set("login", customer.login);
        body->set("password", "WrongPassword!");

        const HttpResult response = server.request("POST", "/auth/login", json(body));
        CHECK(response.status == 401);
        CHECK(server.parseObject(response.body)->getValue<std::string>("error") == "unauthorized");
    }

    TEST_CASE("protected endpoint rejects missing token")
    {
        TestServer server;
        const HttpResult response = server.request("GET", "/users/by-login?login=manager");

        CHECK(response.status == 401);
    }

    TEST_CASE("protected endpoint rejects malformed token")
    {
        TestServer server;
        const HttpResult response = server.request("GET", "/users/by-login?login=manager", "", "not-a-valid-token");

        CHECK(response.status == 401);
    }

    TEST_CASE("protected endpoint rejects expired token")
    {
        TestServer server;
        const auto customer = server.createCustomer();
        const std::string expiredToken = server.makeTokenFor(customer.id, car_rental::Role::Customer, customer.login, -10);

        const HttpResult response = server.request("GET", "/users/by-login?login=" + customer.login, "", expiredToken);
        CHECK(response.status == 401);
    }

    TEST_CASE("customer cannot add cars")
    {
        TestServer server;
        const auto customer = server.createCustomer();

        Poco::JSON::Object::Ptr body = new Poco::JSON::Object;
        body->set("vin", server.uniqueVin());
        body->set("brand", "Skoda");
        body->set("model", "Octavia");
        body->set("class", "COMFORT");
        body->set("pricePerDay", 120.0);

        const HttpResult response = server.request("POST", "/cars", json(body), customer.token);
        CHECK(response.status == 403);
    }
}
