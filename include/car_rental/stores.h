#pragma once

#include "car_rental/models.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace car_rental {

class Database;

struct RentalWriteRequest
{
    std::string id;
    std::string userId;
    std::string carId;
    std::string startAt;
    std::string endAt;
    double priceTotal{};
    std::string createdAt;
    std::string paymentReference;
};

struct RentalCompletionRequest
{
    std::string rentalId;
    std::string closedAt;
};

class UserStore
{
public:
    virtual ~UserStore() = default;

    virtual UserDto createCustomer(
        const RegisterUserRequest& request,
        const std::string& userId,
        const std::string& passwordSalt,
        const std::string& passwordHash,
        const std::string& createdAt) const = 0;

    virtual UserDto upsertManager(
        const std::string& login,
        const std::string& passwordSalt,
        const std::string& passwordHash,
        const std::string& createdAt) const = 0;

    virtual std::optional<UserDto> findById(const std::string& userId) const = 0;
    virtual std::optional<UserDto> findByLogin(const std::string& login) const = 0;
    virtual std::optional<UserRecord> findRecordByLogin(const std::string& login) const = 0;
    virtual std::vector<UserDto> search(const std::string& nameMask, const std::string& surnameMask) const = 0;
};

class FleetStore
{
public:
    virtual ~FleetStore() = default;

    virtual CarDto addCar(
        const CreateCarRequest& request,
        const std::string& carId,
        const std::string& createdAt) const = 0;

    virtual std::optional<CarDto> findById(const std::string& carId) const = 0;
    virtual std::vector<CarDto> listAvailable() const = 0;
    virtual std::vector<CarDto> searchByClass(CarClass carClass) const = 0;
    virtual void updateStatus(const std::string& carId, CarStatus status) const = 0;
};

class RentalStore
{
public:
    virtual ~RentalStore() = default;

    virtual std::optional<RentalDto> findById(const std::string& rentalId) const = 0;
    virtual std::vector<RentalDto> listActiveByUser(const std::string& userId) const = 0;
    virtual std::vector<RentalDto> listHistoryByUser(const std::string& userId) const = 0;
};

class RentalWorkflowCoordinator
{
public:
    virtual ~RentalWorkflowCoordinator() = default;

    virtual RentalDto createActiveRental(const RentalWriteRequest& request) const = 0;
    virtual RentalDto completeRental(const RentalCompletionRequest& request) const = 0;
};

std::unique_ptr<UserStore> makePostgresUserStore(const Database& database);
std::unique_ptr<FleetStore> makePostgresFleetStore(const Database& database);
std::unique_ptr<RentalStore> makePostgresRentalStore(const Database& database);
std::unique_ptr<RentalWorkflowCoordinator> makePostgresRentalWorkflowCoordinator(const Database& database);

} // namespace car_rental
