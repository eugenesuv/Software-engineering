#pragma once

#include <sqlite3.h>

#include <stdexcept>
#include <string>

namespace car_rental {

class SqliteStatement
{
public:
    SqliteStatement(sqlite3* db, const std::string& sql);
    ~SqliteStatement();

    SqliteStatement(const SqliteStatement&) = delete;
    SqliteStatement& operator=(const SqliteStatement&) = delete;

    sqlite3_stmt* handle() const noexcept;
    void bind(int index, const std::string& value);
    void bind(int index, double value);
    void bind(int index, int value);
    void bindNull(int index);
    int step();
    void reset();

private:
    sqlite3_stmt* stmt_{nullptr};
};

void execSql(sqlite3* db, const std::string& sql);
std::string columnString(sqlite3_stmt* stmt, int index);
double columnDouble(sqlite3_stmt* stmt, int index);
int columnInt(sqlite3_stmt* stmt, int index);

} // namespace car_rental
