#include "car_rental/performance.h"

#include <algorithm>
#include <utility>

namespace car_rental {

bool TtlJsonCache::get(const std::string& key, std::string& value)
{
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);

    const auto found = entries_.find(key);
    if (found == entries_.end())
        return false;

    if (found->second.expiresAt <= now)
    {
        entries_.erase(found);
        return false;
    }

    value = found->second.value;
    return true;
}

void TtlJsonCache::put(const std::string& key, std::string value, std::chrono::seconds ttl)
{
    if (ttl <= std::chrono::seconds::zero())
        return;

    Entry entry{std::move(value), std::chrono::steady_clock::now() + ttl};
    std::lock_guard<std::mutex> lock(mutex_);
    entries_[key] = std::move(entry);
}

void TtlJsonCache::invalidate(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.erase(key);
}

void TtlJsonCache::invalidatePrefix(const std::string& prefix)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = entries_.begin(); it != entries_.end();)
    {
        if (it->first.rfind(prefix, 0) == 0)
            it = entries_.erase(it);
        else
            ++it;
    }
}

RateLimitResult FixedWindowRateLimiter::allow(const std::string& key, int rawLimit, std::chrono::seconds rawWindow)
{
    const int limit = std::max(rawLimit, 1);
    const auto window = rawWindow <= std::chrono::seconds::zero() ? std::chrono::seconds(60) : rawWindow;
    const auto now = std::chrono::system_clock::now();

    std::lock_guard<std::mutex> lock(mutex_);

    Window& state = windows_[key];
    if (state.resetAt <= now)
    {
        state.count = 0;
        state.resetAt = now + window;
    }

    const bool allowed = state.count < limit;
    if (allowed)
        ++state.count;

    const auto retryAfter = std::chrono::duration_cast<std::chrono::seconds>(state.resetAt - now);
    const auto retryAfterSeconds = std::max<long long>(retryAfter.count(), 0);
    return RateLimitResult{
        allowed,
        limit,
        std::max(limit - state.count, 0),
        state.resetAt,
        static_cast<long>(retryAfterSeconds)};
}

} // namespace car_rental
