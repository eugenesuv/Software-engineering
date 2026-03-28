#pragma once

#include "car_rental/database.h"
#include "car_rental/security.h"
#include "car_rental/services.h"

#include <memory>
#include <string>

namespace Poco::Net {
class HTTPServer;
}

namespace car_rental {

struct ServerConfig
{
    std::string host{"127.0.0.1"};
    int port{8080};
    std::string databasePath{"data/car_rental.db"};
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
    std::unique_ptr<Database> database_;
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
