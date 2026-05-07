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

std::string rentalJson(const std::string& userId, const std::string& carId)
{
    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("userId", userId);
    object->set("carId", carId);
    object->set("startAt", "2030-03-01T10:00:00Z");
    object->set("endAt", "2030-03-03T10:00:00Z");
    return json(object);
}

std::string headerValue(const HttpResult& response, const std::string& name)
{
    const auto found = response.headers.find(name);
    return found == response.headers.end() ? "" : found->second;
}

Poco::JSON::Object::Ptr loginPayload()
{
    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("login", "missing-login");
    object->set("password", "WrongPassword!");
    return object;
}

} // namespace

TEST_SUITE("performance")
{
    TEST_CASE("available cars endpoint uses cache")
    {
        TestServer server;
        const auto car = server.createCar(car_rental::CarClass::Comfort);

        const HttpResult first = server.request("GET", "/cars/available");
        REQUIRE(first.status == 200);
        CHECK(headerValue(first, "X-Cache") == "MISS");
        CHECK(server.parseArray(first.body)->size() == 1);

        const HttpResult second = server.request("GET", "/cars/available");
        REQUIRE(second.status == 200);
        CHECK(headerValue(second, "X-Cache") == "HIT");
        CHECK(server.parseArray(second.body)->getObject(0)->getValue<std::string>("id") == car.id);
    }

    TEST_CASE("available cars cache is invalidated after rental creation")
    {
        TestServer server;
        const auto customer = server.createCustomer();
        const auto car = server.createCar(car_rental::CarClass::Comfort);

        REQUIRE(server.request("GET", "/cars/available").status == 200);
        CHECK(headerValue(server.request("GET", "/cars/available"), "X-Cache") == "HIT");

        REQUIRE(server.request("POST", "/rentals", rentalJson(customer.id, car.id), customer.token).status == 201);

        const HttpResult afterRental = server.request("GET", "/cars/available");
        REQUIRE(afterRental.status == 200);
        CHECK(headerValue(afterRental, "X-Cache") == "MISS");
        CHECK(server.parseArray(afterRental.body)->size() == 0);
    }

    TEST_CASE("class search cache is invalidated after adding car")
    {
        TestServer server;

        const HttpResult first = server.request("GET", "/cars/search?class=BUSINESS");
        REQUIRE(first.status == 200);
        CHECK(headerValue(first, "X-Cache") == "MISS");
        CHECK(server.parseArray(first.body)->size() == 0);

        const HttpResult second = server.request("GET", "/cars/search?class=BUSINESS");
        REQUIRE(second.status == 200);
        CHECK(headerValue(second, "X-Cache") == "HIT");

        const auto car = server.createCar(car_rental::CarClass::Business);

        const HttpResult afterAdd = server.request("GET", "/cars/search?class=BUSINESS");
        REQUIRE(afterAdd.status == 200);
        CHECK(headerValue(afterAdd, "X-Cache") == "MISS");
        const auto payload = server.parseArray(afterAdd.body);
        REQUIRE(payload->size() == 1);
        CHECK(payload->getObject(0)->getValue<std::string>("id") == car.id);
    }

    TEST_CASE("login endpoint returns 429 and rate limit headers after fixed window is exhausted")
    {
        TestServer server;
        const std::string body = json(loginPayload());

        for (int attempt = 0; attempt < 5; ++attempt)
        {
            const HttpResult response = server.request("POST", "/auth/login", body);
            CHECK(response.status == 401);
            CHECK(headerValue(response, "X-RateLimit-Limit") == "5");
            CHECK_FALSE(headerValue(response, "X-RateLimit-Remaining").empty());
            CHECK_FALSE(headerValue(response, "X-RateLimit-Reset").empty());
        }

        const HttpResult limited = server.request("POST", "/auth/login", body);
        CHECK(limited.status == 429);
        CHECK(headerValue(limited, "X-RateLimit-Limit") == "5");
        CHECK(headerValue(limited, "X-RateLimit-Remaining") == "0");
        CHECK_FALSE(headerValue(limited, "X-RateLimit-Reset").empty());
        CHECK_FALSE(headerValue(limited, "Retry-After").empty());
    }
}
