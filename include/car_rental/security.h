#pragma once

#include "car_rental/models.h"

#include <cstdint>
#include <string>

namespace car_rental {

struct TokenPayload
{
    std::string subject;
    std::string login;
    Role role{Role::Customer};
    std::int64_t expiresAtUnix{};
};

class PasswordHasher
{
public:
    std::string generateSalt() const;
    std::string hashPassword(const std::string& password, const std::string& salt) const;
    bool verify(const std::string& password, const std::string& salt, const std::string& expectedHash) const;
};

class JwtService
{
public:
    JwtService(std::string secret, long ttlSeconds);

    std::string issueToken(const AuthenticatedUser& user) const;
    TokenPayload verifyToken(const std::string& token) const;
    long ttlSeconds() const noexcept;

private:
    std::string secret_;
    long ttlSeconds_;
};

} // namespace car_rental
