#include <sqlite3.h>
#include <stdio.h>

sqlite3* DB_connect() {
    sqlite3* db;
    int ret = sqlite3_open("test.db", &db);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "DBオープンエラー: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db); 
        return NULL;
    }
    return db;
}

