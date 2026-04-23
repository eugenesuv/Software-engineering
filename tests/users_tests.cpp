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

TEST_SUITE("users")
{
    TEST_CASE("customer registration persists user and search by login works")
    {
        TestServer server;
        const auto customer = server.createCustomer();

        CHECK(server.countUsers() == 2);

        const HttpResult response = server.request(
            "GET",
            "/users/by-login?login=" + customer.login,
            "",
            customer.token);

        REQUIRE(response.status == 200);
        const auto payload = server.parseObject(response.body);
        CHECK(payload->getValue<std::string>("login") == customer.login);
        CHECK(payload->getValue<std::string>("role") == "CUSTOMER");
    }

    TEST_CASE("duplicate login returns conflict")
    {
        TestServer server;
        const auto customer = server.createCustomer("same");

        Poco::JSON::Object::Ptr body = new Poco::JSON::Object;
        body->set("login", customer.login);
        body->set("password", "Secret123!");
        body->set("firstName", "Another");
        body->set("lastName", "User");
        body->set("email", "dup@example.com");
        body->set("phone", "+79000000001");
        body->set("driverLicenseNumber", "DL-DUP");

        const HttpResult response = server.request("POST", "/users", json(body));
        CHECK(response.status == 409);
    }

    TEST_CASE("search by mask returns filtered results")
    {
        TestServer server;
        const auto alpha = server.createCustomer("alpha");
        const auto beta = server.createCustomer("beta");

        const HttpResult response = server.request(
            "GET",
            "/users/search?nameMask=Tes&surnameMask=Us",
            "",
            alpha.token);

        REQUIRE(response.status == 200);
        const auto payload = server.parseArray(response.body);
        CHECK(payload->size() >= 2);
        CHECK(beta.id.size() > 0);
    }

    TEST_CASE("by-login returns not found for unknown user")
    {
        TestServer server;
        const auto customer = server.createCustomer();

        const HttpResult response = server.request(
            "GET",
            "/users/by-login?login=missing-user",
            "",
            customer.token);

        CHECK(response.status == 404);
    }

    TEST_CASE("search requires at least one mask")
    {
        TestServer server;
        const auto customer = server.createCustomer();

        const HttpResult response = server.request("GET", "/users/search", "", customer.token);
        CHECK(response.status == 400);
    }
}
