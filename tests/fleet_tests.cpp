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

} // namespace

TEST_SUITE("fleet")
{
    TEST_CASE("fleet manager can add car")
    {
        TestServer server;
        const auto car = server.createCar(car_rental::CarClass::Business);

        CHECK_FALSE(car.id.empty());
        CHECK(server.scalarInt("SELECT COUNT(*) FROM fleet_service.cars") == 1);
    }

    TEST_CASE("invalid vin is rejected")
    {
        TestServer server;

        Poco::JSON::Object::Ptr body = new Poco::JSON::Object;
        body->set("vin", "BADVIN");
        body->set("brand", "BMW");
        body->set("model", "X1");
        body->set("class", "SUV");
        body->set("pricePerDay", 200.0);

        const HttpResult response = server.request("POST", "/cars", json(body), server.managerToken());
        CHECK(response.status == 400);
    }

    TEST_CASE("duplicate vin is rejected")
    {
        TestServer server;
        const std::string vin = server.uniqueVin();

        Poco::JSON::Object::Ptr body = new Poco::JSON::Object;
        body->set("vin", vin);
        body->set("brand", "Kia");
        body->set("model", "Rio");
        body->set("class", "ECONOMY");
        body->set("pricePerDay", 99.0);

        CHECK(server.request("POST", "/cars", json(body), server.managerToken()).status == 201);
        CHECK(server.request("POST", "/cars", json(body), server.managerToken()).status == 409);
    }

    TEST_CASE("available list hides cars in rent")
    {
        TestServer server;
        const auto customer = server.createCustomer();
        const auto car = server.createCar(car_rental::CarClass::Comfort);

        REQUIRE(server.request("POST", "/rentals", rentalJson(customer.id, car.id), customer.token).status == 201);

        const HttpResult available = server.request("GET", "/cars/available");
        REQUIRE(available.status == 200);
        CHECK(server.parseArray(available.body)->size() == 0);
    }

    TEST_CASE("search by class returns matching cars")
    {
        TestServer server;
        const auto businessCar = server.createCar(car_rental::CarClass::Business);
        const auto economyCar = server.createCar(car_rental::CarClass::Economy);

        const HttpResult response = server.request("GET", "/cars/search?class=BUSINESS");
        REQUIRE(response.status == 200);
        const auto payload = server.parseArray(response.body);

        CHECK(payload->size() == 1);
        CHECK(payload->getObject(0)->getValue<std::string>("id") == businessCar.id);
        CHECK(economyCar.id.size() > 0);
    }
}
