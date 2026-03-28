#include "test_support.h"

#include <Poco/JSON/Object.h>
#include <Poco/JSON/Stringifier.h>

#include <doctest/doctest.h>

#include <filesystem>
#include <sstream>

namespace {

std::string json(const Poco::JSON::Object::Ptr& object)
{
    std::ostringstream buffer;
    Poco::JSON::Stringifier::stringify(object, buffer);
    return buffer.str();
}

std::string rentalBody(const std::string& userId, const std::string& carId, const std::string& startAt = "2030-04-01T09:00:00Z", const std::string& endAt = "2030-04-04T09:00:00Z")
{
    Poco::JSON::Object::Ptr object = new Poco::JSON::Object;
    object->set("userId", userId);
    object->set("carId", carId);
    object->set("startAt", startAt);
    object->set("endAt", endAt);
    return json(object);
}

} // namespace

TEST_SUITE("rentals")
{
    TEST_CASE("customer can create and complete rental with history transition")
    {
        TestServer server;
        const auto customer = server.createCustomer();
        const auto car = server.createCar();

        const HttpResult created = server.request("POST", "/rentals", rentalBody(customer.id, car.id), customer.token);
        REQUIRE(created.status == 201);
        const auto createdPayload = server.parseObject(created.body);
        const std::string rentalId = createdPayload->getValue<std::string>("id");

        const HttpResult active = server.request("GET", "/users/" + customer.id + "/rentals/active", "", customer.token);
        REQUIRE(active.status == 200);
        CHECK(server.parseArray(active.body)->size() == 1);
        CHECK(server.scalarText("SELECT status FROM cars WHERE id = '" + car.id + "'") == "IN_RENT");
        CHECK(server.scalarInt("SELECT COUNT(*) FROM outbox_events") == 1);

        const HttpResult completed = server.request("POST", "/rentals/" + rentalId + "/complete", "", customer.token);
        REQUIRE(completed.status == 200);
        CHECK(server.parseObject(completed.body)->getValue<std::string>("status") == "COMPLETED");
        CHECK(server.scalarText("SELECT status FROM cars WHERE id = '" + car.id + "'") == "AVAILABLE");
        CHECK(server.scalarInt("SELECT COUNT(*) FROM outbox_events") == 2);

        const HttpResult activeAfter = server.request("GET", "/users/" + customer.id + "/rentals/active", "", customer.token);
        CHECK(server.parseArray(activeAfter.body)->size() == 0);

        const HttpResult history = server.request("GET", "/users/" + customer.id + "/rentals/history", "", customer.token);
        REQUIRE(history.status == 200);
        CHECK(server.parseArray(history.body)->size() == 1);
    }

    TEST_CASE("create rental rejects missing car")
    {
        TestServer server;
        const auto customer = server.createCustomer();

        const HttpResult response = server.request("POST", "/rentals", rentalBody(customer.id, "missing-car-id"), customer.token);
        CHECK(response.status == 404);
    }

    TEST_CASE("create rental returns not found for unknown user from signed token")
    {
        TestServer server;
        const auto car = server.createCar();
        const std::string ghostId = "ghost-user-id";
        const std::string token = server.makeTokenFor(ghostId, car_rental::Role::Customer, "ghost");

        const HttpResult response = server.request("POST", "/rentals", rentalBody(ghostId, car.id), token);
        CHECK(response.status == 404);
    }

    TEST_CASE("car cannot be rented twice while active")
    {
        TestServer server;
        const auto firstCustomer = server.createCustomer("first");
        const auto secondCustomer = server.createCustomer("second");
        const auto car = server.createCar();

        REQUIRE(server.request("POST", "/rentals", rentalBody(firstCustomer.id, car.id), firstCustomer.token).status == 201);
        const HttpResult conflict = server.request("POST", "/rentals", rentalBody(secondCustomer.id, car.id), secondCustomer.token);
        CHECK(conflict.status == 409);
    }

    TEST_CASE("invalid rental dates are rejected")
    {
        TestServer server;
        const auto customer = server.createCustomer();
        const auto car = server.createCar();

        const HttpResult response = server.request(
            "POST",
            "/rentals",
            rentalBody(customer.id, car.id, "2030-04-04T09:00:00Z", "2030-04-01T09:00:00Z"),
            customer.token);

        CHECK(response.status == 400);
    }

    TEST_CASE("repeated completion returns conflict")
    {
        TestServer server;
        const auto customer = server.createCustomer();
        const auto car = server.createCar();
        const auto created = server.request("POST", "/rentals", rentalBody(customer.id, car.id), customer.token);
        const std::string rentalId = server.parseObject(created.body)->getValue<std::string>("id");

        REQUIRE(server.request("POST", "/rentals/" + rentalId + "/complete", "", customer.token).status == 200);
        CHECK(server.request("POST", "/rentals/" + rentalId + "/complete", "", customer.token).status == 409);
    }

    TEST_CASE("foreign customer cannot access or complete another users rental")
    {
        TestServer server;
        const auto owner = server.createCustomer("owner");
        const auto intruder = server.createCustomer("intruder");
        const auto car = server.createCar();

        const auto created = server.request("POST", "/rentals", rentalBody(owner.id, car.id), owner.token);
        const std::string rentalId = server.parseObject(created.body)->getValue<std::string>("id");

        CHECK(server.request("GET", "/users/" + owner.id + "/rentals/active", "", intruder.token).status == 403);
        CHECK(server.request("POST", "/rentals/" + rentalId + "/complete", "", intruder.token).status == 403);
    }
}
