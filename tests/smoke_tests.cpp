#include "test_support.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

TEST_SUITE("smoke")
{
    TEST_CASE("health endpoint returns ok")
    {
        TestServer server;
        const HttpResult response = server.request("GET", "/health");

        REQUIRE(response.status == 200);
        CHECK(server.parseObject(response.body)->getValue<std::string>("status") == "ok");
    }

    TEST_CASE("openapi file lists public endpoints")
    {
        const std::filesystem::path root = std::filesystem::path(__FILE__).parent_path().parent_path();
        std::ifstream input(root / "openapi.yaml");
        REQUIRE(input.good());

        std::ostringstream buffer;
        buffer << input.rdbuf();
        const std::string contents = buffer.str();

        CHECK(contents.find("/health:") != std::string::npos);
        CHECK(contents.find("/users:") != std::string::npos);
        CHECK(contents.find("/auth/login:") != std::string::npos);
        CHECK(contents.find("/rentals/{rentalId}/complete:") != std::string::npos);
    }
}
