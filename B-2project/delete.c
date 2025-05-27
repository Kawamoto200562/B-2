#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <windows.h>

void delete_user() {
    sqlite3* db;
    if (sqlite3_open("test2.db", &db) != SQLITE_OK) {
        fprintf(stderr, "データベースを開けません: %s\n", sqlite3_errmsg(db));
        return;
    }

    int mode;
    printf("削除方法を選んでください:\n");
    printf("1: 受験者単位の完全削除\n");
    printf("2: 試験を1つ選んで削除（試験日＋科目）\n");
    printf("番号: ");
    scanf("%d", &mode);
    getchar(); // 改行除去

    char* err_msg = NULL;
    sqlite3_stmt* stmt;

    if (sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, &err_msg) != SQLITE_OK) goto error;

    if (mode == 2) {
        int examinee_id;
        printf("受験者IDを入力: ");
        scanf("%d", &examinee_id);
        getchar();

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

            if (score == -1) {
                printf("%6d | %-10s | %-8s | （未入力）\n", exam_id, date, subject);
            }
            else {
                printf("%6d | %-10s | %-8s | %d\n", exam_id, date, subject, score);
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

        printf("試験ID %d を削除しました。\n", target_exam_id);
    }
    else if (mode == 1) {
        int search;
        printf("検索方法（1: 名前, 2: ID）: ");
        scanf("%d", &search);
        getchar();

        if (search == 1) {
            char name[60];
            printf("削除する受験者の名前: ");
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = '\0'; // 改行除去

            const char* sqls[] = {
                "DELETE FROM score WHERE exam_id IN (SELECT exam_id FROM exam WHERE examinee_id IN (SELECT examinee_id FROM examinee WHERE examinee_name=?));",
                "DELETE FROM exam WHERE examinee_id IN (SELECT examinee_id FROM examinee WHERE examinee_name=?);",
                "DELETE FROM examinee WHERE examinee_name=?;"
            };

            int i;
            for (i = 0; i < 3; ++i) {
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

            const char* sqls[] = {
                "DELETE FROM score WHERE exam_id IN (SELECT exam_id FROM exam WHERE examinee_id=?);",
                "DELETE FROM exam WHERE examinee_id=?;",
                "DELETE FROM examinee WHERE examinee_id=?;"
            };

            int i;
            for (i = 0; i < 3; ++i) {
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
    }

    if (sqlite3_exec(db, "COMMIT;", 0, 0, &err_msg) != SQLITE_OK) goto error;
    printf("削除完了。\n");
    sqlite3_close(db);
    return;

rollback:
    sqlite3_exec(db, "ROLLBACK;", 0, 0, NULL);
    sqlite3_close(db);
    return;

error:
    fprintf(stderr, "削除失敗: %s\n", err_msg ? err_msg : sqlite3_errmsg(db));
    if (err_msg) sqlite3_free(err_msg);
    sqlite3_exec(db, "ROLLBACK;", 0, 0, NULL);
    sqlite3_close(db);
}
