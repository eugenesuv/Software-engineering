#include "car_rental/postgres_database.h"

#include <pqxx/pqxx>

#include <stdexcept>
#include <utility>

namespace car_rental {

PostgresDatabase::PostgresDatabase(std::string connectionString)
    : connectionString_(std::move(connectionString))
{
}

const std::string& PostgresDatabase::connectionString() const noexcept
{
    return connectionString_;
}

std::unique_ptr<pqxx::connection> PostgresDatabase::connect() const
{
    auto connection = std::make_unique<pqxx::connection>(connectionString_);
    if (!connection->is_open())
        throw std::runtime_error("Failed to open PostgreSQL connection.");
    return connection;
}

void PostgresDatabase::verifyConnection() const
{
    auto connection = connect();
    pqxx::nontransaction tx(*connection);
    tx.exec("SELECT 1");
}

} // namespace car_rental
