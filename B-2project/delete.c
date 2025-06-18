#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

void delete_by_exam(sqlite3* db) {
    sqlite3_stmt* stmt;
    char* err_msg = NULL;

    int examinee_id;
    printf("受験者IDを入力: ");
    scanf("%d", &examinee_id);
    getchar();

    if (sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, &err_msg) != SQLITE_OK) goto error;

    const char* show_sql =
        "SELECT exam.exam_id, exam.exam_date, Subjects.Subject_name, score.score "
        "FROM exam "
        "JOIN score ON exam.exam_id = score.exam_id "
        "JOIN Subjects ON score.Subject_id = Subjects.Subject_id "
        "WHERE exam.examinee_id = ? "
        "ORDER BY exam.exam_date;";

    if (sqlite3_prepare_v2(db, show_sql, -1, &stmt, NULL) != SQLITE_OK) goto error;
    sqlite3_bind_int(stmt, 1, examinee_id);

    printf("\n--- 登録済みの試験一覧 ---\n");
    printf("ExamID | 日付       | 科目     | 点数\n");
    printf("----------------------------------------\n");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int exam_id = sqlite3_column_int(stmt, 0);
        const unsigned char* date = sqlite3_column_text(stmt, 1);
        const unsigned char* subject = sqlite3_column_text(stmt, 2);
        int score = sqlite3_column_type(stmt, 3) == SQLITE_NULL ? -1 : sqlite3_column_int(stmt, 3);

        printf("%6d | %-10s | %-8s | ", exam_id, date, subject);
        if (score == -1) {
            printf("（未入力）\n");
        }
        else {
            printf("%d\n", score);
        }
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0) {
        printf("この受験者には試験データがありません。\n");
        goto rollback;
    }

    int target_exam_id;
    printf("\n削除したい試験の ExamID を入力: ");
    scanf("%d", &target_exam_id);
    getchar();

    // --- 確認表示 ---
    const char* confirm_sql =
        "SELECT exam.exam_date, Subjects.Subject_name, score.score "
        "FROM exam "
        "JOIN score ON exam.exam_id = score.exam_id "
        "JOIN Subjects ON score.Subject_id = Subjects.Subject_id "
        "WHERE exam.exam_id = ?;";

    if (sqlite3_prepare_v2(db, confirm_sql, -1, &stmt, NULL) != SQLITE_OK) goto error;
    sqlite3_bind_int(stmt, 1, target_exam_id);

    printf("\n--- 削除対象の試験情報 ---\n");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* date = sqlite3_column_text(stmt, 0);
        const unsigned char* subject = sqlite3_column_text(stmt, 1);
        int score = sqlite3_column_type(stmt, 2) == SQLITE_NULL ? -1 : sqlite3_column_int(stmt, 2);

        printf("日付: %s | 科目: %s | 点数: ", date, subject);
        if (score == -1) {
            printf("（未入力）\n");
        }
        else {
            printf("%d\n", score);
        }
    }
    sqlite3_finalize(stmt);

    char confirm;
    printf("この試験を削除しますか？ (y/n): ");
    confirm = getchar();
    if (confirm != 'y' && confirm != 'Y') {
        printf("削除をキャンセルしました。\n");
        goto rollback;
    }

    const char* delete_score = "DELETE FROM score WHERE exam_id = ?;";
    if (sqlite3_prepare_v2(db, delete_score, -1, &stmt, NULL) != SQLITE_OK) goto error;
    sqlite3_bind_int(stmt, 1, target_exam_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    const char* delete_exam = "DELETE FROM exam WHERE exam_id = ?;";
    if (sqlite3_prepare_v2(db, delete_exam, -1, &stmt, NULL) != SQLITE_OK) goto error;
    sqlite3_bind_int(stmt, 1, target_exam_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (sqlite3_exec(db, "COMMIT;", 0, 0, &err_msg) != SQLITE_OK) goto error;
    printf("試験ID %d を削除しました。\n", target_exam_id);
    return;

rollback:
    sqlite3_exec(db, "ROLLBACK;", 0, 0, NULL);
    return;

error:
    fprintf(stderr, "削除失敗: %s\n", err_msg ? err_msg : sqlite3_errmsg(db));
    if (err_msg) sqlite3_free(err_msg);
    sqlite3_exec(db, "ROLLBACK;", 0, 0, NULL);
}

void delete_by_examinee(sqlite3* db) {
    sqlite3_stmt* stmt;
    char* err_msg = NULL;

    int search;
    printf("検索方法（1: 名前, 2: ID）: ");
    scanf("%d", &search);
    getchar();

    if (sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, &err_msg) != SQLITE_OK) goto error;

    if (search == 1) {
        char name[60];
        printf("削除する受験者の名前: ");
        fgets(name, sizeof(name), stdin);
        name[strcspn(name, "\n")] = '\0';

        // --- 確認表示 ---
        const char* preview_sql =
            "SELECT e.examinee_id, e.examinee_name, ex.exam_date, s.Subject_name, sc.score "
            "FROM examinee e "
            "LEFT JOIN exam ex ON e.examinee_id = ex.examinee_id "
            "LEFT JOIN score sc ON ex.exam_id = sc.exam_id "
            "LEFT JOIN Subjects s ON sc.Subject_id = s.Subject_id "
            "WHERE e.examinee_name = ?;";

        if (sqlite3_prepare_v2(db, preview_sql, -1, &stmt, NULL) != SQLITE_OK) goto error;
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);

        printf("\n--- 削除対象受験者の情報 ---\n");
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char* ename = sqlite3_column_text(stmt, 1);
            const unsigned char* date = sqlite3_column_text(stmt, 2);
            const unsigned char* subject = sqlite3_column_text(stmt, 3);
            int score = sqlite3_column_type(stmt, 4) == SQLITE_NULL ? -1 : sqlite3_column_int(stmt, 4);

            printf("ID: %d | 名前: %s | 日付: %s | 科目: %s | 点数: ", id, ename,
                date ? (const char*)date : "なし",
                subject ? (const char*)subject : "なし");
            if (score == -1) {
                printf("（未入力）\n");
            }
            else {
                printf("%d\n", score);
            }
        }
        sqlite3_finalize(stmt);

        char confirm;
        printf("この受験者を削除しますか？ (y/n): ");
        confirm = getchar();
        if (confirm != 'y' && confirm != 'Y') {
            printf("削除をキャンセルしました。\n");
            goto rollback;
        }

        const char* sqls[] = {
            "DELETE FROM score WHERE exam_id IN (SELECT exam_id FROM exam WHERE examinee_id IN (SELECT examinee_id FROM examinee WHERE examinee_name=?));",
            "DELETE FROM exam WHERE examinee_id IN (SELECT examinee_id FROM examinee WHERE examinee_name=?);",
            "DELETE FROM examinee WHERE examinee_name=?;"
        };

        for (int i = 0; i < 3; ++i) {
            if (sqlite3_prepare_v2(db, sqls[i], -1, &stmt, NULL) != SQLITE_OK) goto error;
            sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    else if (search == 2) {
        int id;
        printf("削除する受験者ID: ");
        scanf("%d", &id);
        getchar();

        // --- 確認表示 ---
        const char* preview_sql =
            "SELECT e.examinee_id, e.examinee_name, ex.exam_date, s.Subject_name, sc.score "
            "FROM examinee e "
            "LEFT JOIN exam ex ON e.examinee_id = ex.examinee_id "
            "LEFT JOIN score sc ON ex.exam_id = sc.exam_id "
            "LEFT JOIN Subjects s ON sc.Subject_id = s.Subject_id "
            "WHERE e.examinee_id = ?;";

        if (sqlite3_prepare_v2(db, preview_sql, -1, &stmt, NULL) != SQLITE_OK) goto error;
        sqlite3_bind_int(stmt, 1, id);

        printf("\n--- 削除対象受験者の情報 ---\n");
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int uid = sqlite3_column_int(stmt, 0);
            const unsigned char* ename = sqlite3_column_text(stmt, 1);
            const unsigned char* date = sqlite3_column_text(stmt, 2);
            const unsigned char* subject = sqlite3_column_text(stmt, 3);
            int score = sqlite3_column_type(stmt, 4) == SQLITE_NULL ? -1 : sqlite3_column_int(stmt, 4);

            printf("ID: %d | 名前: %s | 日付: %s | 科目: %s | 点数: ", uid, ename,
                date ? (const char*)date : "なし",
                subject ? (const char*)subject : "なし");
            if (score == -1) {
                printf("（未入力）\n");
            }
            else {
                printf("%d\n", score);
            }
        }
        sqlite3_finalize(stmt);

        char confirm;
        printf("この受験者を削除しますか？ (y/n): ");
        confirm = getchar();
        if (confirm != 'y' && confirm != 'Y') {
            printf("削除をキャンセルしました。\n");
            goto rollback;
        }

        const char* sqls[] = {
            "DELETE FROM score WHERE exam_id IN (SELECT exam_id FROM exam WHERE examinee_id=?);",
            "DELETE FROM exam WHERE examinee_id=?;",
            "DELETE FROM examinee WHERE examinee_id=?;"
        };

        for (int i = 0; i < 3; ++i) {
            if (sqlite3_prepare_v2(db, sqls[i], -1, &stmt, NULL) != SQLITE_OK) goto error;
            sqlite3_bind_int(stmt, 1, id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    else {
        printf("無効な入力です。\n");
        goto rollback;
    }

    if (sqlite3_exec(db, "COMMIT;", 0, 0, &err_msg) != SQLITE_OK) goto error;
    printf("受験者のデータを削除しました。\n");
    return;

rollback:
    sqlite3_exec(db, "ROLLBACK;", 0, 0, NULL);
    return;

error:
    fprintf(stderr, "削除失敗: %s\n", err_msg ? err_msg : sqlite3_errmsg(db));
    if (err_msg) sqlite3_free(err_msg);
    sqlite3_exec(db, "ROLLBACK;", 0, 0, NULL);
}
