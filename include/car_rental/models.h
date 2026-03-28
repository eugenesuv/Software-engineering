#pragma once

#include <stdexcept>
#include <string>

namespace car_rental {

enum class Role
{
    Customer,
    FleetManager
};

enum class CarClass
{
    Economy,
    Comfort,
    Business,
    Suv,
    Luxury
};

enum class CarStatus
{
    Available,
    Reserved,
    InRent,
    Maintenance
};

enum class RentalStatus
{
    Active,
    Completed,
    Cancelled
};

inline std::string toString(Role role)
{
    switch (role)
    {
    case Role::Customer:
        return "CUSTOMER";
    case Role::FleetManager:
        return "FLEET_MANAGER";
    }
    throw std::logic_error("Unknown role");
}

inline std::string toString(CarClass value)
{
    switch (value)
    {
    case CarClass::Economy:
        return "ECONOMY";
    case CarClass::Comfort:
        return "COMFORT";
    case CarClass::Business:
        return "BUSINESS";
    case CarClass::Suv:
        return "SUV";
    case CarClass::Luxury:
        return "LUXURY";
    }
    throw std::logic_error("Unknown car class");
}

inline std::string toString(CarStatus value)
{
    switch (value)
    {
    case CarStatus::Available:
        return "AVAILABLE";
    case CarStatus::Reserved:
        return "RESERVED";
    case CarStatus::InRent:
        return "IN_RENT";
    case CarStatus::Maintenance:
        return "MAINTENANCE";
    }
    throw std::logic_error("Unknown car status");
}

inline std::string toString(RentalStatus value)
{
    switch (value)
    {
    case RentalStatus::Active:
        return "ACTIVE";
    case RentalStatus::Completed:
        return "COMPLETED";
    case RentalStatus::Cancelled:
        return "CANCELLED";
    }
    throw std::logic_error("Unknown rental status");
}

inline Role roleFromString(const std::string& value)
{
    if (value == "CUSTOMER")
        return Role::Customer;
    if (value == "FLEET_MANAGER")
        return Role::FleetManager;
    throw std::invalid_argument("Unsupported role: " + value);
}

inline CarClass carClassFromString(const std::string& value)
{
    if (value == "ECONOMY")
        return CarClass::Economy;
    if (value == "COMFORT")
        return CarClass::Comfort;
    if (value == "BUSINESS")
        return CarClass::Business;
    if (value == "SUV")
        return CarClass::Suv;
    if (value == "LUXURY")
        return CarClass::Luxury;
    throw std::invalid_argument("Unsupported car class: " + value);
}

inline CarStatus carStatusFromString(const std::string& value)
{
    if (value == "AVAILABLE")
        return CarStatus::Available;
    if (value == "RESERVED")
        return CarStatus::Reserved;
    if (value == "IN_RENT")
        return CarStatus::InRent;
    if (value == "MAINTENANCE")
        return CarStatus::Maintenance;
    throw std::invalid_argument("Unsupported car status: " + value);
}

inline RentalStatus rentalStatusFromString(const std::string& value)
{
    if (value == "ACTIVE")
        return RentalStatus::Active;
    if (value == "COMPLETED")
        return RentalStatus::Completed;
    if (value == "CANCELLED")
        return RentalStatus::Cancelled;
    throw std::invalid_argument("Unsupported rental status: " + value);
}

inline bool isTerminal(RentalStatus value)
{
    return value == RentalStatus::Completed || value == RentalStatus::Cancelled;
}

struct RegisterUserRequest
{
    std::string login;
    std::string password;
    std::string firstName;
    std::string lastName;
    std::string email;
    std::string phone;
    std::string driverLicenseNumber;
};

struct LoginRequest
{
    std::string login;
    std::string password;
};

struct UserDto
{
    std::string id;
    std::string login;
    std::string firstName;
    std::string lastName;
    std::string email;
    std::string phone;
    std::string driverLicenseNumber;
    Role role;
    std::string createdAt;
};

struct UserRecord
{
    UserDto user;
    std::string passwordSalt;
    std::string passwordHash;
};

struct CarDto
{
    std::string id;
    std::string vin;
    std::string brand;
    std::string model;
    CarClass carClass;
    CarStatus status;
    double pricePerDay{};
    std::string createdAt;
};

struct CreateCarRequest
{
    std::string vin;
    std::string brand;
    std::string model;
    CarClass carClass;
    double pricePerDay{};
};

struct RentalDto
{
    std::string id;
    std::string userId;
    std::string carId;
    std::string startAt;
    std::string endAt;
    RentalStatus status;
    double priceTotal{};
    std::string createdAt;
    std::string closedAt;
    std::string paymentReference;
};

struct CreateRentalRequest
{
    std::string userId;
    std::string carId;
    std::string startAt;
    std::string endAt;
};

struct LoginResponseDto
{
    std::string accessToken;
    std::string tokenType;
    long expiresIn{};
    UserDto user;
};

struct ErrorResponse
{
    std::string error;
    std::string message;
    std::string details;
};

struct AuthenticatedUser
{
    std::string id;
    std::string login;
    Role role{Role::Customer};
    bool authenticated{false};
};

} // namespace car_rental
