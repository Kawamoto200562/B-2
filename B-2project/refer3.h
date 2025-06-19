#ifndef REFER3_H
#define REFER3_H

#include <sqlite3.h>

void print_aligned_japanese(const char* str, int total_width);

void show_average_by_subject(sqlite3* db);           // 機能9
void show_total_average(sqlite3* db);                // 機能10
void show_examinees_below_subject_avg(sqlite3* db);  // 機能11
void show_examinees_below_selected_avg(sqlite3* db);    // 機能12

#endif