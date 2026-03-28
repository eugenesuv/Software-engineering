#pragma once

#include <Poco/Timestamp.h>

#include <string>

namespace car_rental {

std::string nowUtcIsoString();
std::string formatUtc(const Poco::Timestamp& timestamp);
Poco::Timestamp parseUtcTimestamp(const std::string& value);
std::string generateUuid();
std::string trimCopy(const std::string& value);
std::string upperCopy(std::string value);

} // namespace car_rental
