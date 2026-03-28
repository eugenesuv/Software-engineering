#pragma once

#include <stdexcept>
#include <string>

namespace car_rental {

class ApiException : public std::runtime_error
{
public:
    ApiException(int status, std::string errorCode, std::string message, std::string details = "");

    int status() const noexcept;
    const std::string& errorCode() const noexcept;
    const std::string& details() const noexcept;

private:
    int status_;
    std::string errorCode_;
    std::string details_;
};

} // namespace car_rental
