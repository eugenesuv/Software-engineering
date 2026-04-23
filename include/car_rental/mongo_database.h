#pragma once

#include <memory>
#include <string>

typedef struct _mongoc_client_t mongoc_client_t;
typedef struct _mongoc_database_t mongoc_database_t;

namespace car_rental {

class MongoDatabase
{
public:
    MongoDatabase(std::string connectionString, std::string databaseName);
    ~MongoDatabase();

    MongoDatabase(const MongoDatabase&) = delete;
    MongoDatabase& operator=(const MongoDatabase&) = delete;

    const std::string& connectionString() const noexcept;
    const std::string& databaseName() const noexcept;
    mongoc_client_t* client() const noexcept;
    mongoc_database_t* database() const noexcept;

    void verifyConnection() const;
    void ensureCollections() const;

private:
    std::string connectionString_;
    std::string databaseName_;
    std::unique_ptr<mongoc_client_t, void (*)(mongoc_client_t*)> client_;
    std::unique_ptr<mongoc_database_t, void (*)(mongoc_database_t*)> database_;
};

} // namespace car_rental
