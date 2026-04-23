#pragma once

#include "car_rental/mongo_database.h"
#include "car_rental/postgres_database.h"
#include "car_rental/security.h"
#include "car_rental/services.h"
#include "car_rental/stores.h"

#include <memory>
#include <string>

namespace Poco::Net {
class HTTPServer;
}

namespace car_rental {

enum class StorageBackend
{
    Postgres,
    Mongo
};

std::string toString(StorageBackend backend);
StorageBackend storageBackendFromString(const std::string& value);

struct ServerConfig
{
    std::string host{"127.0.0.1"};
    int port{8080};
    StorageBackend dataBackend{StorageBackend::Postgres};
    std::string databaseUrl{"postgresql://postgres:postgres@127.0.0.1:5432/car_rental"};
    std::string mongoUrl{"mongodb://127.0.0.1:27017/?replicaSet=rs0"};
    std::string mongoDatabaseName{"car_rental"};
    std::string jwtSecret{"dev-secret-change-me"};
    long jwtTtlSeconds{3600};
    std::string managerLogin{"manager"};
    std::string managerPassword{"Manager123!"};
};

class ApiServer
{
public:
    explicit ApiServer(ServerConfig config);
    ~ApiServer();

    void start();
    void stop();
    std::uint16_t port() const noexcept;
    const ServerConfig& config() const noexcept;

private:
    ServerConfig config_;
    std::unique_ptr<PostgresDatabase> postgresDatabase_;
    std::unique_ptr<MongoDatabase> mongoDatabase_;
    std::unique_ptr<UserStore> userStore_;
    std::unique_ptr<FleetStore> fleetStore_;
    std::unique_ptr<RentalStore> rentalStore_;
    std::unique_ptr<RentalWorkflowCoordinator> rentalWorkflowCoordinator_;
    std::unique_ptr<PasswordHasher> passwordHasher_;
    std::unique_ptr<JwtService> jwtService_;
    std::shared_ptr<LicenseVerifier> licenseVerifier_;
    std::shared_ptr<PaymentGateway> paymentGateway_;
    std::unique_ptr<UserService> userService_;
    std::unique_ptr<AuthService> authService_;
    std::unique_ptr<FleetService> fleetService_;
    std::unique_ptr<RentalService> rentalService_;
    std::unique_ptr<Poco::Net::HTTPServer> server_;
    std::uint16_t actualPort_{0};
};

} // namespace car_rental
