#include "car_rental/utils.h"

#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeParser.h>
#include <Poco/UUIDGenerator.h>

#include <cctype>
#include <stdexcept>

namespace car_rental {
namespace {

constexpr const char* kUtcFormat = "%Y-%m-%dT%H:%M:%SZ";

} // namespace

std::string nowUtcIsoString()
{
    return formatUtc(Poco::Timestamp{});
}

std::string formatUtc(const Poco::Timestamp& timestamp)
{
    return Poco::DateTimeFormatter::format(timestamp, kUtcFormat);
}

Poco::Timestamp parseUtcTimestamp(const std::string& value)
{
    Poco::DateTime parsed;
    int timezoneDifferential = 0;
    Poco::DateTimeParser::parse(kUtcFormat, value, parsed, timezoneDifferential);
    return parsed.timestamp();
}

std::string generateUuid()
{
    return Poco::UUIDGenerator::defaultGenerator().createRandom().toString();
}

std::string trimCopy(const std::string& value)
{
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
        ++start;

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
        --end;

    return value.substr(start, end - start);
}

std::string upperCopy(std::string value)
{
    for (char& ch : value)
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return value;
}

} // namespace car_rental
