#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "refer1.h"
#include <wchar.h>
#include <locale.h>
#include <wctype.h>
#include "wcwidth.h"
#include "refer3.h"

void show_top5_by_exam_subject(sqlite3* db) {
    setlocale(LC_ALL, "");
    sqlite3_stmt* stmt;
    char sql[512];
    int ret;

    // 試験実施日一覧取得
    typedef struct { int exam_id, exam_date; } ExamEntry;
    ExamEntry exams[100];
    int exam_count = 0;

    snprintf(sql, sizeof(sql),
        "SELECT exam_id, exam_date FROM exam GROUP BY exam_date ORDER BY exam_date;");
    if ((ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) != SQLITE_OK) {
        fprintf(stderr, "試験日取得エラー: %s\n", sqlite3_errmsg(db));
        return;
    }
    printf("■ 試験実施日一覧:\n");
    while (sqlite3_step(stmt) == SQLITE_ROW && exam_count < 100) {
        exams[exam_count].exam_id = sqlite3_column_int(stmt, 0);
        exams[exam_count].exam_date = sqlite3_column_int(stmt, 1);
        int y = exams[exam_count].exam_date / 10000;
        int m = (exams[exam_count].exam_date / 100) % 100;
        int d = exams[exam_count].exam_date % 100;
        printf("%2d:%04d-%02d-%02d\n", exam_count + 1, y, m, d);
        exam_count++;
    }
    sqlite3_finalize(stmt);
    if (exam_count == 0) {
        printf("試験実施日が見つかりませんでした。\n");
        return;
    }

    //  試験日選択
    int exam_index;
    char buf[16];
    while (1) {
        printf("参照したい試験日の番号を入力（1〜%d、0:戻る）: ", exam_count);
        if (!fgets(buf, sizeof(buf), stdin)) continue;
        if (buf[0] == '0') return;
        if (sscanf(buf, "%d", &exam_index) != 1 || exam_index < 1 || exam_index > exam_count) {
            printf("正しい番号を入力してください。\n");
            continue;
        }
        break;
    }
    int selected_date = exams[exam_index - 1].exam_date;

    // 科目一覧取得
    snprintf(sql, sizeof(sql),
        "SELECT DISTINCT s.subject_id, s.subject_name "
        "FROM score sc "
        "JOIN subjects s ON sc.subject_id = s.subject_id "
        "JOIN exam e ON sc.exam_id = e.exam_id "
        "WHERE e.exam_date = %d;", selected_date);
    if ((ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) != SQLITE_OK) {
        fprintf(stderr, "科目取得エラー: %s\n", sqlite3_errmsg(db));
        return;
    }
    int subject_ids[20], subject_count = 0;
    char subject_names[20][64];
    system("cls");
    printf("\n■ 科目一覧:\n");
    while (sqlite3_step(stmt) == SQLITE_ROW && subject_count < 20) {
        subject_ids[subject_count] = sqlite3_column_int(stmt, 0);
        snprintf(subject_names[subject_count], sizeof(subject_names[subject_count]),
            "%s", sqlite3_column_text(stmt, 1));
        printf("%2d:%s\n", subject_count + 1, subject_names[subject_count]);
        subject_count++;
    }
    sqlite3_finalize(stmt);
    if (subject_count == 0) {
        printf("科目が見つかりませんでした。\n");
        return;
    }

    // ■ STEP4: 科目選択 (0で戻る)
    int subject_index;
    while (1) {
        printf("参照したい科目の番号を入力（1〜%d、0:戻る）: ", subject_count);
        if (!fgets(buf, sizeof(buf), stdin)) continue;
        if (buf[0] == '0') return;
        if (sscanf(buf, "%d", &subject_index) != 1 || subject_index < 1 || subject_index > subject_count) {
            printf("正しい番号を入力してください。\n");
            continue;
        }
        break;
    }
    int sid = subject_ids[subject_index - 1];
    char* sname = subject_names[subject_index - 1];

    // スコア取得＆表示
    snprintf(sql, sizeof(sql),
        "SELECT e.examinee_name, e.furigana, sc.score "
        "FROM score sc "
        "JOIN exam ex ON sc.exam_id = ex.exam_id "
        "JOIN examinee e ON ex.examinee_id = e.examinee_id "
        "WHERE ex.exam_date = %d AND sc.subject_id = %d "
        "ORDER BY sc.score DESC;",
        selected_date, sid);
    if ((ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) != SQLITE_OK) {
        fprintf(stderr, "スコア取得エラー: %s\n", sqlite3_errmsg(db));
        return;
    }

    system("cls");
    printf("\n■ 【%04d-%02d-%02d 】【%s 】 トップ5\n",
        selected_date / 10000, (selected_date / 100) % 100, selected_date % 100, sname);
    printf("--------------------------------------------------------------------------\n");
    printf("| 順位 | %-22s | %-28s | %-5s |\n", "名前", "ふりがな", "点数");
    printf("--------------------------------------------------------------------------\n");

    int dense_rank = 0, prev_score = -1;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* nm = (const char*)sqlite3_column_text(stmt, 0);
        const char* fk = (const char*)sqlite3_column_text(stmt, 1);
        int score = sqlite3_column_int(stmt, 2);

        if (score != prev_score) dense_rank++;
        if (dense_rank > 5) break;

        printf("| %2d位 | ", dense_rank);
        print_aligned_japanese(nm, 20);
        printf(" | ");
        print_aligned_japanese(fk, 24);
        printf(" | %3d  |\n", score);

        prev_score = score;
    }
    printf("--------------------------------------------------------------------------\n");
    sqlite3_finalize(stmt);
}


