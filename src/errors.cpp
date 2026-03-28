#include "car_rental/errors.h"

namespace car_rental {

ApiException::ApiException(int status, std::string errorCode, std::string message, std::string details)
    : std::runtime_error(std::move(message))
    , status_(status)
    , errorCode_(std::move(errorCode))
    , details_(std::move(details))
{
}

int ApiException::status() const noexcept
{
    return status_;
}

const std::string& ApiException::errorCode() const noexcept
{
    return errorCode_;
}

const std::string& ApiException::details() const noexcept
{
    return details_;
}

} // namespace car_rental
