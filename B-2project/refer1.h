#ifndef TOP5_H
#define TOP5_H

#include <sqlite3.h>


//int get_top5_data(sqlite3* db, const char* date, const char* subject, char* buffer, int bufsize);

void show_top5_by_exam_subject(sqlite3* db);

#endif