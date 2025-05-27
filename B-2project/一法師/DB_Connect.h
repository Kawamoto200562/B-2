#ifndef DB_CONNECT_H
#define DB_CONNECT_H

#include <sqlite3.h>

extern sqlite3* db;
sqlite3* DB_connect(void);

#endif

