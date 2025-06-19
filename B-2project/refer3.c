#define _CRT_SECURE_NO_WARNINGS
#define PAGE_SIZE 2
#include <stdio.h>
#include <sqlite3.h>
#include <wchar.h>
#include <locale.h>
#include <windows.h>
#include "wcwidth.h"

// 表示幅を考慮した出力関数
void print_aligned_japanese(const char* str, int total_width) {
    wchar_t wstr[256];
    mbstowcs(wstr, str, sizeof(wstr) / sizeof(wchar_t));

    int actual_width = wcswidth(wstr);
    printf("%s", str);
    for (int i = 0; i < total_width - actual_width; i++) putchar(' ');
}

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

    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        found = 1;
        const unsigned char* subject = sqlite3_column_text(stmt, 0);
        double avg = sqlite3_column_double(stmt, 1);
        printf("%s: %.2f\n", subject, avg);
    }

    if (!found) {
        printf("データがありません。");
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
        if (sqlite3_column_type(stmt, 0) == SQLITE_NULL) {
            printf("\n【全科目の平均点数】\nデータがありません。\n");
        }
        else {
            double avg = sqlite3_column_double(stmt, 0);
            printf("\n【全科目の平均点数】\n平均点: %.2f\n", avg);
        }
    }
    else {
        printf("\n【全科目の平均点数】\nデータがありません。\n");
    }

    sqlite3_finalize(stmt);
}

// 各科目平均点以下の受験者一覧（機能11）
void show_examinees_below_subject_avg(sqlite3* db) {
    setlocale(LC_ALL, "");

    int subject_id;
    const char* subject_names[] = {
        "国語", "数学", "英語", "日本史", "世界史", "地理", "物理", "化学", "生物"
    };

    // 科目選択
    while (1) {
        printf("■ 科目を選んでください:\n");
        for (int i = 0; i < 9; i++) {
            printf("%d: %s\n", i + 1, subject_names[i]);
        }
        printf("0: メニューに戻る\n");

        printf("番号を入力: ");
        if (scanf("%d", &subject_id) != 1) {
            printf("無効な入力です。1〜9または0を入力してください。\n");
            while (getchar() != '\n');
            continue;
        }

        if (subject_id == 0) return;
        if (subject_id < 1 || subject_id > 9) {
            printf("無効な入力です。1〜9の番号を入力してください。\n");
            while (getchar() != '\n');
            continue;
        }

        while (getchar() != '\n');
        system("cls");
        break;
    }

    // 平均点取得
    char avg_sql[128];
    sqlite3_stmt* avg_stmt;
    double avg_score = 0.0;

    snprintf(avg_sql, sizeof(avg_sql),
        "SELECT AVG(score) FROM score WHERE subject_id = %d;", subject_id);

    if (sqlite3_prepare_v2(db, avg_sql, -1, &avg_stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "平均取得SQLエラー: %s\n", sqlite3_errmsg(db));
        return;
    }

    if (sqlite3_step(avg_stmt) == SQLITE_ROW) {
        avg_score = sqlite3_column_double(avg_stmt, 0);
    }
    sqlite3_finalize(avg_stmt);

    // 対象受験者データ取得
    char sql[512];
    sqlite3_stmt* stmt;

    snprintf(sql, sizeof(sql),
        "SELECT e.examinee_name, e.furigana, ex.exam_date, s.score "
        "FROM examinee e "
        "JOIN exam ex ON e.examinee_id = ex.examinee_id "
        "JOIN score s ON ex.exam_id = s.exam_id "
        "WHERE s.subject_id = %d AND s.score <= %.2f;",
        subject_id, avg_score);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL準備失敗: %s\n", sqlite3_errmsg(db));
        return;
    }

    typedef struct {
        char name[64];
        char kana[64];
        char date[11];
        int score;
    } Record;

    Record records[1000];
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* name = sqlite3_column_text(stmt, 0);
        const unsigned char* kana = sqlite3_column_text(stmt, 1);
        int raw_date = sqlite3_column_int(stmt, 2);
        int score = sqlite3_column_int(stmt, 3);

        int y = raw_date / 10000, m = (raw_date / 100) % 100, d = raw_date % 100;
        snprintf(records[count].date, sizeof(records[count].date), "%04d-%02d-%02d", y, m, d);
        snprintf(records[count].name, sizeof(records[count].name), "%s", name);
        snprintf(records[count].kana, sizeof(records[count].kana), "%s", kana);
        records[count].score = score;
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0) {
        printf("【%s】の平均点以下の受験者（平均点: %.2f）\n", subject_names[subject_id - 1], avg_score);
        printf("データがありません。\n");
        return;
    }

    // ページング表示
    int page = 0;
    int total_pages = (count + 49) / 50;
    char cmd[16];

    while (1) {
        system("cls");
        printf("\n【%s】の平均点以下の受験者（平均点: %.2f）\n", subject_names[subject_id - 1], avg_score);
        printf("（ページ %d / %d）\n", page + 1, total_pages);
        printf("------------------------------------------------------------------------------------\n");
        printf("| %-22s | %-28s | %-13s | %-8s |\n", "名前", "ふりがな", "試験日", "点数");
        printf("------------------------------------------------------------------------------------\n");

        int start = page * 50;
        int end = (start + 50 < count) ? start + 50 : count;
        for (int i = start; i < end; i++) {
            printf("| ");
            print_aligned_japanese(records[i].name, 20);
            printf(" | ");
            print_aligned_japanese(records[i].kana, 24);
            printf(" | %-10s | %6d |\n", records[i].date, records[i].score);
        }
        printf("------------------------------------------------------------------------------------\n");

        while (1) {
            if (total_pages == 1) {
                printf("0: 終了 > ");
            }
            else if (page == 0) {
                printf("1: 次のページ, 0: 終了 > ");
            }
            else if (page == total_pages - 1) {
                printf("2: 前のページ, 0: 終了 > ");
            }
            else {
                printf("1: 次のページ, 2: 前のページ, 0: 終了 > ");
            }

            fgets(cmd, sizeof(cmd), stdin);
            if (cmd[0] == '0' && cmd[1] == '\n') {
                return;
            }
            if (cmd[0] == '1' && cmd[1] == '\n' && page < total_pages - 1) {
                page++;
                break;
            }
            if (cmd[0] == '2' && cmd[1] == '\n' && page > 0) {
                page--;
                break;
            }
            printf("無効な入力です。指示された番号を半角で入力してください。\n");
        }
    }
}

// 参照機能１２
void show_examinees_below_selected_avg(sqlite3* db) {
    setlocale(LC_ALL, "");

    sqlite3_stmt* stmt;
    const char* sql =
        "WITH selected_only AS ("
        "    SELECT e.examinee_id, e.examinee_name, e.furigana, ex.exam_date, s.subject_name, sc.score "
        "    FROM examinee e "
        "    JOIN exam ex ON e.examinee_id = ex.examinee_id "
        "    JOIN score sc ON ex.exam_id = sc.exam_id AND sc.examinee_id = e.examinee_id "
        "    JOIN subjects s ON sc.subject_id = s.subject_id "
        "    WHERE sc.subject_id > 3"
        "), "
        "subject_combos AS ("
        "    SELECT examinee_id, GROUP_CONCAT(subject_name, ' + ') AS selected_combo "
        "    FROM selected_only "
        "    GROUP BY examinee_id"
        "), "
        "examinee_totals AS ("
        "    SELECT e.examinee_id, e.examinee_name, e.furigana, ex.exam_date, SUM(sc.score) AS total_score "
        "    FROM examinee e "
        "    JOIN exam ex ON e.examinee_id = ex.examinee_id "
        "    JOIN score sc ON ex.exam_id = sc.exam_id AND sc.examinee_id = e.examinee_id "
        "    GROUP BY e.examinee_id"
        "), "
        "with_combo AS ("
        "    SELECT et.*, sc.selected_combo "
        "    FROM examinee_totals et "
        "    JOIN subject_combos sc ON et.examinee_id = sc.examinee_id"
        "), "
        "combo_avg AS ("
        "    SELECT selected_combo, AVG(total_score) AS combo_avg "
        "    FROM with_combo "
        "    GROUP BY selected_combo"
        ") "
        "SELECT wc.examinee_name, wc.furigana, wc.exam_date, wc.selected_combo, wc.total_score, ca.combo_avg "
        "FROM with_combo wc "
        "JOIN combo_avg ca ON wc.selected_combo = ca.selected_combo "
        "WHERE wc.total_score < ca.combo_avg";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        printf("SQL 準備失敗: %s\n", sqlite3_errmsg(db));
        return;
    }

    typedef struct {
        char name[64];
        char kana[64];
        char date[11];
        char combo[64];
        int score;
        double avg;
    } Record;

    Record records[1000];
    int record_count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* name = sqlite3_column_text(stmt, 0);
        const unsigned char* kana = sqlite3_column_text(stmt, 1);
        int raw_date = sqlite3_column_int(stmt, 2);
        const unsigned char* combo = sqlite3_column_text(stmt, 3);
        int score = sqlite3_column_int(stmt, 4);
        double avg = sqlite3_column_double(stmt, 5);

        int year = raw_date / 10000;
        int month = (raw_date / 100) % 100;
        int day = raw_date % 100;
        snprintf(records[record_count].date, sizeof(records[record_count].date), "%04d-%02d-%02d", year, month, day);
        snprintf(records[record_count].name, sizeof(records[record_count].name), "%s", name);
        snprintf(records[record_count].kana, sizeof(records[record_count].kana), "%s", kana);
        snprintf(records[record_count].combo, sizeof(records[record_count].combo), "%s", combo);
        records[record_count].score = score;
        records[record_count].avg = avg;

        record_count++;
    }
    sqlite3_finalize(stmt);

    if (record_count == 0) {
        printf("データがありません。\n");
        return;
    }

    int page = 0;
    const int page_size = 50;
    int total_pages = (record_count + page_size - 1) / page_size;
    char command_line[16];

    while (1) {
        system("cls");
        printf("\n■ 選択科目の組み合わせごとの平均以下の受験者一覧\n");
        printf("（ページ %d / %d）\n", page + 1, total_pages);
        printf("----------------------------------------------------------------------------------------------------------------------\n");
        printf("| %-24s | %-28s | %-13s | %-24s | %11s | %10s |\n", "名前", "ふりがな", "試験日", "選択科目", "合計点", "平均点");
        printf("----------------------------------------------------------------------------------------------------------------------\n");

        int start = page * page_size;
        int end = (start + page_size < record_count) ? start + page_size : record_count;
        for (int i = start; i < end; i++) {
            printf("| ");
            print_aligned_japanese(records[i].name, 22);
            printf(" | ");
            print_aligned_japanese(records[i].kana, 24);
            printf(" | %-10s | ", records[i].date);
            print_aligned_japanese(records[i].combo, 20);
            printf(" | %8d | %7.2f |\n", records[i].score, records[i].avg);
        }
        printf("----------------------------------------------------------------------------------------------------------------------\n");

        printf("操作を選択してください:\n");
        if (total_pages == 1) {
            printf("0: 終了 > ");
        }
        else if (page == 0) {
            printf("1: 次のページ, 0: 終了 > ");
        }
        else if (page == total_pages - 1) {
            printf("2: 前のページ, 0: 終了 > ");
        }
        else {
            printf("1: 次のページ, 2: 前のページ, 0: 終了 > ");
        }

        fgets(command_line, sizeof(command_line), stdin);
        char cmd = command_line[0];

        if (cmd == '0') break;
        else if (cmd == '1' && page < total_pages - 1) {
            page++;
        }
        else if (cmd == '2' && page > 0) {
            page--;
        }
        else {
            printf("無効な入力です。Enterキーを押して続けてください。\n");
            fgets(command_line, sizeof(command_line), stdin);
        }
    }
}




