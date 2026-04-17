#include "database.h"

int main() {
    Database db("oxydian.db");

    db.createTable();

    db.insertUser("AYOUB");
    db.insertUser("DOUAA");

    db.getUsers();

    db.updateUser(1, "Ayoub Updated");

    db.deleteUser(2);

    db.getUsers();

    return 0;
}