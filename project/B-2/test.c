#include <stdio.h>
#include "sqlite3.h"

int main()
{
    sqlite3* db;
    char* err_msg = 0;

    // データベースを開く（なければ作成される）
    if (sqlite3_open("mydb.db", &db) != SQLITE_OK)
    {
        printf("Cannot open DB: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // テーブル作成SQL
    const char* sql = "CREATE TABLE Students ("
                      "student_id INTEGER PRIMARY KEY,"
                      "student_name TEXT NOT NULL);";

    // 実行
    if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK)
    {
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
    else
    {
        printf("Table created successfully\n");
    }

    sqlite3_close(db);
    return 0;
}
