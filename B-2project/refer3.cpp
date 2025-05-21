#include <stdio.h>
#include <sqlite3.h>

// 各科目の平均点数（機能9）
void show_average_by_subject(sqlite3* db) {
    const char* sql =
        "SELECT s.subject_name, AVG(sc.score) AS average "
        "FROM score sc "
        "JOIN Subjects s ON sc.Subject_id = s.Subject_id "
        "GROUP BY s.Subject_id;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        printf("SQLエラー（機能9）: %s\n", sqlite3_errmsg(db));
        return;
    }

    printf("\n【各科目の平均点数】\n");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* subject = sqlite3_column_text(stmt, 0);
        double avg = sqlite3_column_double(stmt, 1);
        printf("%s: %.2f\n", subject, avg);
    }
    sqlite3_finalize(stmt);
}

// 全科目の平均点数（機能10）
void show_total_average(sqlite3* db) {
    const char* sql = "SELECT AVG(score) FROM score;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        printf("SQLエラー（機能10）: %s\n", sqlite3_errmsg(db));
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        double avg = sqlite3_column_double(stmt, 0);
        printf("\n【全科目の平均点数】\n平均点: %.2f\n", avg);
    }
    sqlite3_finalize(stmt);
}

// 各科目平均点以下の受験者一覧（機能11）
void show_examinees_below_subject_avg(sqlite3* db) {
    const char* sql =
        "SELECT e.examinee_name, s.subject_name, sc.score, AVG(sc2.score) as avg_score "
        "FROM Score sc "
        "JOIN exam ex ON sc.exam_id = ex.exam_id "
        "JOIN examinee e ON ex.examinee_id = e.examinee_id "
        "JOIN Subjects s ON sc.Subject_id = s.Subject_id "
        "JOIN score sc2 ON sc.Subject_id = sc2.Subject_id "
        "GROUP BY sc.score_id "
        "HAVING sc.score < avg_score;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        printf("SQLエラー（機能11）: %s\n", sqlite3_errmsg(db));
        return;
    }

    printf("\n【各科目平均点以下の受験者】\n");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* name = sqlite3_column_text(stmt, 0);
        const unsigned char* subject = sqlite3_column_text(stmt, 1);
        int score = sqlite3_column_int(stmt, 2);
        double avg = sqlite3_column_double(stmt, 3);
        printf("%s - %s: %d点（平均%.2f点以下）\n", name, subject, score, avg);
    }
    sqlite3_finalize(stmt);
}

// 全科目平均点以下の受験者一覧（機能12）
void show_examinees_below_total_avg(sqlite3* db) {
    const char* sql =
        "WITH total_avg AS (SELECT AVG(score) AS avg_score FROM Score) "
        "SELECT e.Examinee_name, AVG(sc.score) as avg_examinee "
        "FROM score sc "
        "JOIN exam ex ON sc.exam_id = ex.exam_id "
        "JOIN examinee e ON ex.examinee_id = e.examinee_id "
        "GROUP BY e.examinee_id "
        "HAVING avg_examinee < (SELECT avg_score FROM total_avg);";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        printf("SQLエラー（機能12）: %s\n", sqlite3_errmsg(db));
        return;
    }

    printf("\n【全科目平均点以下の受験者】\n");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* name = sqlite3_column_text(stmt, 0);
        double avg = sqlite3_column_double(stmt, 1);
        printf("%s: 平均%.2f点\n", name, avg);
    }
    sqlite3_finalize(stmt);
}
