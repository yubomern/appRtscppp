#pragma once
#include <string>

class Database {
public:
    Database(const std::string& db_name);
    ~Database();

    void createTable();
    void insertUser(const std::string& name);
    void getUsers();
    void updateUser(int id, const std::string& name);
    void deleteUser(int id);

private:
    void* db; // sqlite3*
};