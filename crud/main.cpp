#include "database.h"

int main() {
    Database db("oxydian.db");

    db.createTable();

    db.insertUser("Alice");
    db.insertUser("Bob");

    db.getUsers();

    db.updateUser(1, "Alice Updated");

    db.deleteUser(2);

    db.getUsers();

    return 0;
}