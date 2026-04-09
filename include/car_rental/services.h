#pragma once

#include "car_rental/models.h"
#include "car_rental/security.h"
#include "car_rental/stores.h"

#include <memory>
#include <string>
#include <vector>

namespace car_rental {

struct PaymentResult
{
    bool approved{false};
    std::string reference;
};

class LicenseVerifier
{
public:
    virtual ~LicenseVerifier() = default;
    virtual bool verify(const std::string& driverLicenseNumber) const = 0;
};

class PaymentGateway
{
public:
    virtual ~PaymentGateway() = default;
    virtual PaymentResult preauthorize(const std::string& userId, double amount) const = 0;
};

class DeterministicLicenseVerifier : public LicenseVerifier
{
public:
    bool verify(const std::string& driverLicenseNumber) const override;
};

class DeterministicPaymentGateway : public PaymentGateway
{
public:
    PaymentResult preauthorize(const std::string& userId, double amount) const override;
};

class UserService
{
public:
    UserService(UserStore& store, const PasswordHasher& passwordHasher);

    UserDto registerCustomer(const RegisterUserRequest& request) const;
    UserDto seedManager(const std::string& login, const std::string& password) const;
    UserDto findByLogin(const std::string& login) const;
    UserDto getById(const std::string& userId) const;
    UserRecord getRecordByLogin(const std::string& login) const;
    std::vector<UserDto> search(const std::string& nameMask, const std::string& surnameMask) const;

private:
    UserStore& store_;
    const PasswordHasher& passwordHasher_;
};

class AuthService
{
public:
    AuthService(const UserService& userService, const PasswordHasher& passwordHasher, const JwtService& jwtService);

    LoginResponseDto login(const LoginRequest& request) const;
    AuthenticatedUser authenticateAuthorizationHeader(const std::string& authorizationHeader) const;

private:
    const UserService& userService_;
    const PasswordHasher& passwordHasher_;
    const JwtService& jwtService_;
};

class FleetService
{
public:
    explicit FleetService(FleetStore& store);

    CarDto addCar(const CreateCarRequest& request) const;
    std::vector<CarDto> listAvailable() const;
    std::vector<CarDto> searchByClass(CarClass carClass) const;
    CarDto getById(const std::string& carId) const;
    void setStatus(const std::string& carId, CarStatus status) const;

private:
    FleetStore& store_;
};

class RentalService
{
public:
    RentalService(
        RentalStore& rentalStore,
        RentalWorkflowCoordinator& rentalWorkflowCoordinator,
        const UserService& userService,
        FleetService& fleetService,
        const LicenseVerifier& licenseVerifier,
        const PaymentGateway& paymentGateway);

    RentalDto createRental(const AuthenticatedUser& principal, const CreateRentalRequest& request) const;
    std::vector<RentalDto> listActiveRentals(const AuthenticatedUser& principal, const std::string& userId) const;
    std::vector<RentalDto> rentalHistory(const AuthenticatedUser& principal, const std::string& userId) const;
    RentalDto completeRental(const AuthenticatedUser& principal, const std::string& rentalId) const;

private:
    RentalStore& rentalStore_;
    RentalWorkflowCoordinator& rentalWorkflowCoordinator_;
    const UserService& userService_;
    FleetService& fleetService_;
    const LicenseVerifier& licenseVerifier_;
    const PaymentGateway& paymentGateway_;
};

} // namespace car_rental
