#include "car_rental/database.h"

#include <filesystem>

namespace car_rental {

Database::Database(std::string path)
    : path_(std::move(path))
{
    const std::filesystem::path databasePath(path_);
    if (databasePath.has_parent_path())
        std::filesystem::create_directories(databasePath.parent_path());
}

const std::string& Database::path() const noexcept
{
    return path_;
}

void Database::initializeSchema() const
{
    withConnection([](sqlite3* db) {
        execSql(
            db,
            R"SQL(
            CREATE TABLE IF NOT EXISTS users (
                id TEXT PRIMARY KEY,
                login TEXT NOT NULL UNIQUE,
                password_salt TEXT NOT NULL,
                password_hash TEXT NOT NULL,
                first_name TEXT NOT NULL,
                last_name TEXT NOT NULL,
                email TEXT NOT NULL,
                phone TEXT NOT NULL,
                driver_license_number TEXT NOT NULL,
                role TEXT NOT NULL CHECK(role IN ('CUSTOMER', 'FLEET_MANAGER')),
                created_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS cars (
                id TEXT PRIMARY KEY,
                vin TEXT NOT NULL UNIQUE,
                brand TEXT NOT NULL,
                model TEXT NOT NULL,
                class TEXT NOT NULL CHECK(class IN ('ECONOMY', 'COMFORT', 'BUSINESS', 'SUV', 'LUXURY')),
                status TEXT NOT NULL CHECK(status IN ('AVAILABLE', 'RESERVED', 'IN_RENT', 'MAINTENANCE')),
                price_per_day REAL NOT NULL,
                created_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS rentals (
                id TEXT PRIMARY KEY,
                user_id TEXT NOT NULL REFERENCES users(id),
                car_id TEXT NOT NULL REFERENCES cars(id),
                start_at TEXT NOT NULL,
                end_at TEXT NOT NULL,
                status TEXT NOT NULL CHECK(status IN ('ACTIVE', 'COMPLETED', 'CANCELLED')),
                price_total REAL NOT NULL,
                created_at TEXT NOT NULL,
                closed_at TEXT NOT NULL DEFAULT '',
                payment_reference TEXT NOT NULL DEFAULT ''
            );

            CREATE TABLE IF NOT EXISTS outbox_events (
                id TEXT PRIMARY KEY,
                aggregate_type TEXT NOT NULL,
                aggregate_id TEXT NOT NULL,
                event_type TEXT NOT NULL,
                payload TEXT NOT NULL,
                created_at TEXT NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_users_login ON users(login);
            CREATE INDEX IF NOT EXISTS idx_cars_status ON cars(status);
            CREATE INDEX IF NOT EXISTS idx_cars_class ON cars(class);
            CREATE INDEX IF NOT EXISTS idx_rentals_user_status ON rentals(user_id, status);
            CREATE INDEX IF NOT EXISTS idx_rentals_car_status ON rentals(car_id, status);
            )SQL");
    });
}

SqliteStatement::SqliteStatement(sqlite3* db, const std::string& sql)
{
    if (sqlite3_prepare_v2(db, sql.c_str(), static_cast<int>(sql.size()), &stmt_, nullptr) != SQLITE_OK)
        throw std::runtime_error("Failed to prepare SQL statement: " + std::string(sqlite3_errmsg(db)));
}

SqliteStatement::~SqliteStatement()
{
    if (stmt_ != nullptr)
        sqlite3_finalize(stmt_);
}

sqlite3_stmt* SqliteStatement::handle() const noexcept
{
    return stmt_;
}

void SqliteStatement::bind(int index, const std::string& value)
{
    if (sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK)
        throw std::runtime_error("Failed to bind SQLite text parameter");
}

void SqliteStatement::bind(int index, double value)
{
    if (sqlite3_bind_double(stmt_, index, value) != SQLITE_OK)
        throw std::runtime_error("Failed to bind SQLite double parameter");
}

void SqliteStatement::bind(int index, int value)
{
    if (sqlite3_bind_int(stmt_, index, value) != SQLITE_OK)
        throw std::runtime_error("Failed to bind SQLite int parameter");
}

void SqliteStatement::bindNull(int index)
{
    if (sqlite3_bind_null(stmt_, index) != SQLITE_OK)
        throw std::runtime_error("Failed to bind SQLite null parameter");
}

int SqliteStatement::step()
{
    return sqlite3_step(stmt_);
}

void SqliteStatement::reset()
{
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

void execSql(sqlite3* db, const std::string& sql)
{
    char* errorMessage = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errorMessage) != SQLITE_OK)
    {
        const std::string message = errorMessage != nullptr ? errorMessage : sqlite3_errmsg(db);
        sqlite3_free(errorMessage);
        throw std::runtime_error("SQLite execution failed: " + message);
    }
}

std::string columnString(sqlite3_stmt* stmt, int index)
{
    const unsigned char* raw = sqlite3_column_text(stmt, index);
    if (raw == nullptr)
        return {};
    return reinterpret_cast<const char*>(raw);
}

double columnDouble(sqlite3_stmt* stmt, int index)
{
    return sqlite3_column_double(stmt, index);
}

int columnInt(sqlite3_stmt* stmt, int index)
{
    return sqlite3_column_int(stmt, index);
}

} // namespace car_rental
