#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace car_rental {

class TtlJsonCache
{
public:
    bool get(const std::string& key, std::string& value);
    void put(const std::string& key, std::string value, std::chrono::seconds ttl);
    void invalidate(const std::string& key);
    void invalidatePrefix(const std::string& prefix);

private:
    struct Entry
    {
        std::string value;
        std::chrono::steady_clock::time_point expiresAt;
    };

    std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
};

struct RateLimitResult
{
    bool allowed{true};
    int limit{};
    int remaining{};
    std::chrono::system_clock::time_point resetAt;
    long retryAfterSeconds{};
};

class FixedWindowRateLimiter
{
public:
    RateLimitResult allow(const std::string& key, int limit, std::chrono::seconds window);

private:
    struct Window
    {
        int count{};
        std::chrono::system_clock::time_point resetAt;
    };

    std::mutex mutex_;
    std::unordered_map<std::string, Window> windows_;
};

} // namespace car_rental
