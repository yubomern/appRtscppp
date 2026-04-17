#include "database.h"
#include <sqlite3.h>
#include <iostream>

Database::Database(const std::string& db_name) {
    if (sqlite3_open(db_name.c_str(), (sqlite3**)&db)) {
        std::cerr << "Can't open database\n";
    }
}

Database::~Database() {
    sqlite3_close((sqlite3*)db);
}

void Database::createTable() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL);";

    char* errMsg = nullptr;
    sqlite3_exec((sqlite3*)db, sql, nullptr, nullptr, &errMsg);
}

void Database::insertUser(const std::string& name) {
    std::string sql = "INSERT INTO users (name) VALUES ('" + name + "');";
    sqlite3_exec((sqlite3*)db, sql.c_str(), nullptr, nullptr, nullptr);
}

void Database::getUsers() {
    const char* sql = "SELECT * FROM users;";

    auto callback = [](void*, int argc, char** argv, char**) {
        for (int i = 0; i < argc; i++) {
            std::cout << argv[i] << " ";
        }
        std::cout << "\n";
        return 0;
    };

    sqlite3_exec((sqlite3*)db, sql, callback, nullptr, nullptr);
}

void Database::updateUser(int id, const std::string& name) {
    std::string sql = "UPDATE users SET name='" + name +
                      "' WHERE id=" + std::to_string(id) + ";";
    sqlite3_exec((sqlite3*)db, sql.c_str(), nullptr, nullptr, nullptr);
}

void Database::deleteUser(int id) {
    std::string sql = "DELETE FROM users WHERE id=" + std::to_string(id) + ";";
    sqlite3_exec((sqlite3*)db, sql.c_str(), nullptr, nullptr, nullptr);
}