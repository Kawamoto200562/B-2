#define _CRT_SECURE_NO_WARNINGS
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
    int subject_id;
    const char* subject_names[] = {
        "国語", "数学", "英語", "日本史", "世界史", "地理", "物理", "化学", "生物"
    };

    // 科目選択ループ
    while (1) {
        printf("■ 科目を選んでください:\n");
        for (int i = 0; i < 9; i++) {
            printf("%d: %s\n", i + 1, subject_names[i]);
        }

        printf("番号を入力: ");
        if (scanf("%d", &subject_id) != 1 || subject_id < 1 || subject_id > 9) {
            printf("無効な入力です。1〜9の番号を入力してください。\n\n");
            while (getchar() != '\n');
            continue;
        }
        break;
    }

    // 選択された科目の平均点を取得
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

    // 平均点以下の受験者を抽出
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

    // 表形式で出力
    printf("\n【%s】の平均点以下の受験者（平均点: %.2f）\n", subject_names[subject_id - 1], avg_score);
    printf("-----------------------------------------------------------------------------\n");
    printf("| %-10s | %-15s | %-10s | %-8s |\n", "名前", "ふりがな", "試験日", "点数");
    printf("-----------------------------------------------------------------------------\n");

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* name = sqlite3_column_text(stmt, 0);
        const unsigned char* kana = sqlite3_column_text(stmt, 1);
        int raw_date = sqlite3_column_int(stmt, 2); // 整形必要
        int score = sqlite3_column_int(stmt, 3);

        // 日付整形
        int year = raw_date / 10000;
        int month = (raw_date / 100) % 100;
        int day = raw_date % 100;
        char formatted_date[11];
        snprintf(formatted_date, sizeof(formatted_date), "%04d-%02d-%02d", year, month, day);

        printf("| %-10s | %-15s | %-10s | %8d |\n", name, kana, formatted_date, score);
    }

    printf("-----------------------------------------------------------------------------\n");
    sqlite3_finalize(stmt);
}

// 全科目平均点以下の受験者一覧（機能12）
void show_examinees_below_selected_avg(sqlite3* db) {
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

    printf("\n■ 選択科目の組み合わせごとの平均以下の受験者一覧（選択2科目のみ表示）\n");
    printf("------------------------------------------------------------------------------------------------------------\n");
    printf("| %-15s | %-20s | %-10s | %-22s | %8s | %8s |\n", "名前", "ふりがな", "試験日", "選択科目", "個人合計点", "全体平均");
    printf("------------------------------------------------------------------------------------------------------------\n");

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
        char date[11];
        snprintf(date, sizeof(date), "%04d-%02d-%02d", year, month, day);

        printf("| %-15s | %-20s | %-10s | %-22s | %8d | %8.2f |\n",
            name, kana, date, combo, score, avg);
    }

    printf("------------------------------------------------------------------------------------------------------------\n");   

    sqlite3_finalize(stmt);       
}