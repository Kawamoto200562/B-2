#ifndef DELETE_H
#define DELETE_H

#include <sqlite3.h>

void delete_by_examinee(sqlite3* db);
void delete_by_exam(sqlite3* db);

#endif
