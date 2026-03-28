#pragma once

#include "car_rental/sqlite_utils.h"

#include <sqlite3.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

namespace car_rental {

class Database
{
public:
    explicit Database(std::string path);

    const std::string& path() const noexcept;
    void initializeSchema() const;

    template <typename Function>
    auto withConnection(Function&& fn) const -> decltype(fn(static_cast<sqlite3*>(nullptr)))
    {
        std::lock_guard<std::mutex> lock(mutex_);

        sqlite3* raw = nullptr;
        if (sqlite3_open(path_.c_str(), &raw) != SQLITE_OK)
        {
            const std::string message = raw != nullptr ? sqlite3_errmsg(raw) : "unknown sqlite error";
            if (raw != nullptr)
                sqlite3_close(raw);
            throw std::runtime_error("Failed to open SQLite database: " + message);
        }

        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> connection(raw, &sqlite3_close);
        sqlite3_busy_timeout(connection.get(), 5000);
        execSql(connection.get(), "PRAGMA foreign_keys = ON;");

        return fn(connection.get());
    }

private:
    std::string path_;
    mutable std::mutex mutex_;
};

} // namespace car_rental
