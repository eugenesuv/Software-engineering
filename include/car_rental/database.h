#pragma once

#include <memory>
#include <string>

namespace pqxx {
class connection;
}

namespace car_rental {

class Database
{
public:
    explicit Database(std::string connectionString);

    const std::string& connectionString() const noexcept;
    std::unique_ptr<pqxx::connection> connect() const;
    void verifyConnection() const;

private:
    std::string connectionString_;
};

} // namespace car_rental
