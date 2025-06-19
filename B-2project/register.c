#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <windows.h>
#include <locale.h>
#include "inputcansel.h"
#include "DB_Connect.h"
#include <stdbool.h>
#include <wctype.h>
#include <wchar.h>
#include "refer3.h"

typedef struct {
    int count;
    int first_print_done;
    char prev_exam_date[16];
} CallbackData;

// 複数行や複数列の結果を処理
int callback(void* NotUsed, int argc, char** argv, char** azColName) {
    for (int i = 0; i < argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

//  ふりがなの重複確認用
int count_callback(void* data, int argc, char** argv, char** col) {
    if (argc > 0 && argv[0]) {
        *(int*)data = atoi(argv[0]);
    }
    return 0;
}

// 受験者情報確認用(変更機能で使ってます)
int print_callback(void* data, int argc, char** argv, char** azColName) {
    CallbackData* cb_data = (CallbackData*)data;
    cb_data->count++;

    const char* name = argv[0] ? argv[0] : "NULL";
    const char* furigana = argv[1] ? argv[1] : "NULL";
    const char* subject = argv[2] ? argv[2] : "NULL";
    const char* score = argv[3] ? argv[3] : "NULL";

    // 最初に名前・ふりがなを出力
    if (!cb_data->first_print_done) {
        printf("\n名前: %s\n", name);
        printf("ふりがな: %s\n\n", furigana);
        cb_data->first_print_done = 1;
    }

    // 試験日の出力処理（フォーマット付き）
    char raw_exam_date[16];
    strncpy(raw_exam_date, argv[4] ? argv[4] : "NULL", sizeof(raw_exam_date) - 1);
    raw_exam_date[sizeof(raw_exam_date) - 1] = '\0';

    if (strcmp(cb_data->prev_exam_date, raw_exam_date) != 0) {
        char formatted_date[16] = "";
        if (strlen(raw_exam_date) == 8 && strspn(raw_exam_date, "0123456789") == 8) {
            snprintf(formatted_date, sizeof(formatted_date),
                "%.4s-%.2s-%.2s", raw_exam_date, raw_exam_date + 4, raw_exam_date + 6);
        }
        else {
            snprintf(formatted_date, sizeof(formatted_date), "%s", raw_exam_date);
        }
        printf("\n試験日: %s\n", formatted_date);
        strncpy(cb_data->prev_exam_date, raw_exam_date, sizeof(cb_data->prev_exam_date) - 1);
        cb_data->prev_exam_date[sizeof(cb_data->prev_exam_date) - 1] = '\0';
    }

    // 科目と点数
    printf("  ");
    if (subject != NULL) {
        print_aligned_japanese(subject, 10);
    }
    else {
        print_aligned_japanese("NULL", 10);
    }
    printf(": %3s点\n", score);

    return 0;
}



// 不正な値を受け取ったらバッファをクリアする処理
void clear_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

int is_kanji_or_katakana_only(const char* str) {
    wchar_t wstr[256];
    mbstowcs(wstr, str, sizeof(wstr) / sizeof(wchar_t));

    for (int i = 0; wstr[i] != L'\0'; i++) {
        wchar_t c = wstr[i];

        // カタカナ
        if (c >= 0x30A0 && c <= 0x30FF)
            continue;

        // 漢字（CJK統合漢字＋一部拡張）
        if ((c >= 0x4E00 && c <= 0x9FFF) || 
            (c >= 0x3400 && c <= 0x4DBF) || 
            (c >= 0xF900 && c <= 0xFAFF))    
            continue;

        return 0; // 該当しない文字があればNG
    }

    return 1; // すべてOK
}

// ひらがなだけか判定（ふりがな用）
int is_hiragana_only(const char* str) {
    wchar_t wstr[256];
    mbstowcs(wstr, str, sizeof(wstr) / sizeof(wchar_t));
    for (int i = 0; wstr[i] != L'\0'; i++) {
        if (!(wstr[i] >= 0x3040 && wstr[i] <= 0x309F))
            return 0;
    }
    return 1;
}



// 新規登録機能
int New_register() {
    setlocale(LC_ALL, "");
    char* err_msg = NULL;
    int ret;
    char sql[256];
    char input[60];
    char name[60];
    char furigana[59];
    char date_input[16];
    int namecount = 0;
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    sqlite3* db = DB_connect();
    if (!db) {
        fprintf(stderr, "データベース接続失敗\n");
        return 1;
    }

    while (1) {
        printf("氏名（漢字またはカタカナ）を入力してください（bで戻る）: ");
        if (scanf("%59s", name) != 1) {
            while (getchar() != '\n'); // 入力バッファクリア
            continue;
        }
        while (getchar() != '\n'); // 入力バッファクリア

        if (strcmp(name, "b") == 0) return;

        if (strlen(name) > 30) {
            printf("氏名は30文字以内で入力してください。\n");
            continue;
        }

        if (!is_kanji_or_katakana_only(name)) {
            printf("氏名は漢字またはカタカナで入力してください。\n");
            continue;
        }

        break;
    }

    // ふりがな入力（ひらがなチェック付き）
    while (1) {
        printf("ふりがなを入力してください（bで戻る）:\n> ");
        if (scanf("%59s", input) != 1) return 1;
        if (strcmp(input, "b") == 0) return 0;
        if (!is_hiragana_only(input)) {
            printf("ふりがなはひらがなで入力してください。\n");
            continue;
        }
        strcpy(furigana, input);
        break;
    }

    int exam_date = 0;
    while (1) {
        printf("試験日を半角8桁で入力してください（例: 20250501、bで戻る）: ");
        if (scanf("%15s", input) != 1) {
            clear_stdin();
            fprintf(stderr, "試験日入力失敗。\n");
            return 0;
        }

        if (strcmp(input, "b") == 0) return 0;

        if (strlen(input) != 8 || strspn(input, "0123456789") != 8) {
            printf("試験日は8桁の数字で入力してください。\n");
            continue;
        }

        exam_date = atoi(input);
        break;
    }
    exam_date = atoi(input);
    int year = exam_date / 10000;
    int month = (exam_date / 100) % 100;
    int day = exam_date % 100;

    const char* subjects[] = { "国語", "数学", "英語" };
    const char* choicesubjects_1[] = { "日本史", "世界史", "地理", "物理", "化学", "生物" };
    int basicscore[3];
    int choice_subject_score[2];
    int choicenumber_1 = -1, choicenumber_2 = -1;

    for (int i = 0; i < 3; i++) {
        while (1) {
            printf("%sの点数を入力してください (0〜100、bで戻る): ", subjects[i]);
            if (scanf("%59s", input) != 1) return 1;
            if (strcmp(input, "b") == 0) return 0;
            basicscore[i] = atoi(input);
            if (basicscore[i] >= 0 && basicscore[i] <= 100) break;
            printf("0〜100の数字で入力してください。\n");
        }
    }

    printf("選択科目（文系）を1つ選んでください（bで戻る）:\n");
    for (int i = 0; i < 3; i++) printf("%d: %s\n", i + 1, choicesubjects_1[i]);
    while (1) {
        printf("番号を入力 (1〜3): ");
        if (scanf("%59s", input) != 1) return 1;
        if (strcmp(input, "b") == 0) return 0;
        choicenumber_1 = atoi(input);
        if (choicenumber_1 >= 1 && choicenumber_1 <= 3) break;
        printf("1〜3の数字で入力してください。\n");
    }

    printf("選択科目（理系）を1つ選んでください（bで戻る）:\n");
    for (int i = 3; i < 6; i++) printf("%d: %s\n", i + 1, choicesubjects_1[i]);
    while (1) {
        printf("番号を入力 (4〜6): ");
        if (scanf("%59s", input) != 1) return 1;
        if (strcmp(input, "b") == 0) return 0;
        choicenumber_2 = atoi(input);
        if (choicenumber_2 >= 4 && choicenumber_2 <= 6) break;
        printf("4〜6の数字で入力してください。\n");
    }

    printf("%sの点数を入力してください (0〜100、bで戻る): ", choicesubjects_1[choicenumber_1 - 1]);
    while (1) {
        if (scanf("%59s", input) != 1) return 1;
        if (strcmp(input, "b") == 0) return 0;
        choice_subject_score[0] = atoi(input);
        if (choice_subject_score[0] >= 0 && choice_subject_score[0] <= 100) break;
        printf("0〜100の数字で入力してください。\n");
    }

    printf("%sの点数を入力してください (0〜100、bで戻る): ", choicesubjects_1[choicenumber_2 - 1]);
    while (1) {
        if (scanf("%59s", input) != 1) return 1;
        if (strcmp(input, "b") == 0) return 0;
        choice_subject_score[1] = atoi(input);
        if (choice_subject_score[1] >= 0 && choice_subject_score[1] <= 100) break;
        printf("0〜100の数字で入力してください。\n");
    }

    printf("\n名前: %s\nふりがな: %s\n受験日: %04d年%02d月%02d日\n",
        name, furigana, year, month, day);
    for (int i = 0; i < 3; i++) printf("%s: %d点\n", subjects[i], basicscore[i]);
    printf("%s: %d点\n", choicesubjects_1[choicenumber_1 - 1], choice_subject_score[0]);
    printf("%s: %d点\n", choicesubjects_1[choicenumber_2 - 1], choice_subject_score[1]);

    while (1) {
        printf("1: 登録する、2: 戻る > ");
        if (scanf("%59s", input) != 1) return 1;

        if (strcmp(input, "1") == 0) {
            break; // 登録続行
        }
        else if (strcmp(input, "2") == 0) {
            return 0; // 登録メニューに戻る
        }
        else {
            printf("1または2で入力してください。\n");
        }
    }

    snprintf(sql, sizeof(sql),
        "INSERT INTO examinee (examinee_name, furigana) VALUES ('%s', '%s');",
        name, furigana);
    ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "examinee 挿入エラー: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }
    int examinee_id = (int)sqlite3_last_insert_rowid(db);

    snprintf(sql, sizeof(sql),
        "INSERT INTO exam (examinee_id, exam_date) VALUES (%d, %d);",
        examinee_id, exam_date);
    ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "exam 挿入エラー: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }
    int exam_id = (int)sqlite3_last_insert_rowid(db);

    for (int i = 0; i < 3; i++) {
        snprintf(sql, sizeof(sql),
            "INSERT INTO score (exam_id, examinee_id, subject_id, score) VALUES (%d, %d, %d, %d);",
            exam_id, examinee_id, i + 1, basicscore[i]);
        sqlite3_exec(db, sql, 0, 0, NULL);
    }

    snprintf(sql, sizeof(sql),
        "INSERT INTO score (exam_id, examinee_id, subject_id, score) VALUES (%d, %d, %d, %d);",
        exam_id, examinee_id, choicenumber_1 + 3, choice_subject_score[0]);
    sqlite3_exec(db, sql, 0, 0, NULL);

    snprintf(sql, sizeof(sql),
        "INSERT INTO score (exam_id, examinee_id, subject_id, score) VALUES (%d, %d, %d, %d);",
        exam_id, examinee_id, choicenumber_2 + 3, choice_subject_score[1]);
    sqlite3_exec(db, sql, 0, 0, NULL);

    printf("登録が完了しました。\n");
    sqlite3_close(db);
    return 0;
}




void Add_register() {
    char* err_msg = NULL;
    int ret;
    char sql[1024];
    char name[59] = "";
    char furigana[59];
    char date_input[16];
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    sqlite3* db = DB_connect();
    if (!db) {
        fprintf(stderr, "データベース接続に失敗しました。\n");
        return;
    }

    // 受験者一覧表示
    snprintf(sql, sizeof(sql), "SELECT examinee_id, examinee_name, furigana FROM examinee;");
    sqlite3_stmt* stmt;
    ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "受験者一覧の取得に失敗しました: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    int examinee_ids[100];
    int count = 0;
    printf("\n受験者一覧:\n");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char* name_col = sqlite3_column_text(stmt, 1);
        const unsigned char* furigana_col = sqlite3_column_text(stmt, 2);
        printf("%d: %s（%s）\n", count + 1, name_col, furigana_col);
        examinee_ids[count] = id;
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0) {
        printf("登録された受験者が存在しません。\n");
        sqlite3_close(db);
        return;
    }

    int selected = -1;
    while (1) {
        printf("登録する受験者の番号を入力してください（bで戻る）: ");
        char input[16];
        if (scanf("%15s", input) != 1) continue;
        if (strcmp(input, "b") == 0) {
            sqlite3_close(db);
            return;
        }
        int num = atoi(input);
        if (num >= 1 && num <= count) {
            selected = num - 1;
            break;
        }
        printf("正しい番号を入力してください。\n");
    }

    int examinee_id = examinee_ids[selected];

    // 氏名とふりがなを取得
    snprintf(sql, sizeof(sql), "SELECT examinee_name, furigana FROM examinee WHERE examinee_id = %d;", examinee_id);
    ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (ret != SQLITE_OK || sqlite3_step(stmt) != SQLITE_ROW) {
        fprintf(stderr, "受験者情報の取得に失敗しました。\n");
        sqlite3_close(db);
        return;
    }
    snprintf(name, sizeof(name), "%s", sqlite3_column_text(stmt, 0));
    snprintf(furigana, sizeof(furigana), "%s", sqlite3_column_text(stmt, 1));
    sqlite3_finalize(stmt);

    // 試験日入力
    int exam_date = 0;
    while (1) {
        printf("試験日を半角8桁で入力してください（例: 20250501、bで戻る）: ");
        if (scanf("%15s", date_input) != 1) {
            clear_stdin();
            continue;
        }
        if (strcmp(date_input, "b") == 0) {
            sqlite3_close(db);
            return;
        }
        if (strlen(date_input) == 8 && strspn(date_input, "0123456789") == 8) {
            exam_date = atoi(date_input);
            break;
        }
        else {
            printf("試験日は8桁の半角数字で入力してください。\n");
        }
    }
    int year = exam_date / 10000, month = (exam_date / 100) % 100, day = exam_date % 100;

    const char* subjects[] = { "国語", "数学", "英語" };
    const char* choicesubjects_1[] = { "日本史", "世界史", "地理", "物理", "化学", "生物" };
    int basicscore[3], choice_subject_score[2];
    int choicenumber_1 = -1, choicenumber_2 = -1;

    // 必須科目入力
    for (int i = 0; i < 3; i++) {
        while (1) {
            char input[16];
            printf("%sの点数を入力してください (0〜100、bで戻る): ", subjects[i]);
            if (scanf("%15s", input) != 1) continue;
            if (strcmp(input, "b") == 0) {
                sqlite3_close(db);
                return;
            }
            int score = atoi(input);
            if (score >= 0 && score <= 100) {
                basicscore[i] = score;
                break;
            }
            printf("0〜100の範囲で入力してください。\n");
        }
    }

    // 文系選択
    printf("選択科目（文系）を1つ選んでください:\n");
    for (int i = 0; i < 3; i++) printf("%d: %s\n", i + 1, choicesubjects_1[i]);
    while (1) {
        char input[16];
        printf("番号を入力 (1〜3、bで戻る): ");
        if (scanf("%15s", input) != 1) continue;
        if (strcmp(input, "b") == 0) {
            sqlite3_close(db);
            return;
        }
        int num = atoi(input);
        if (num >= 1 && num <= 3) {
            choicenumber_1 = num;
            break;
        }
        printf("1〜3の番号で入力してください。\n");
    }

    // 理系選択
    printf("選択科目（理系）を1つ選んでください:\n");
    for (int i = 3; i < 6; i++) printf("%d: %s\n", i + 1, choicesubjects_1[i]);
    while (1) {
        char input[16];
        printf("番号を入力 (4〜6、bで戻る): ");
        if (scanf("%15s", input) != 1) continue;
        if (strcmp(input, "b") == 0) {
            sqlite3_close(db);
            return;
        }
        int num = atoi(input);
        if (num >= 4 && num <= 6) {
            choicenumber_2 = num;
            break;
        }
        printf("4〜6の番号で入力してください。\n");
    }

    // 選択科目点数
    for (int i = 0; i < 2; i++) {
        while (1) {
            char input[16];
            const char* subject = (i == 0) ? choicesubjects_1[choicenumber_1 - 1] : choicesubjects_1[choicenumber_2 - 1];
            printf("%sの点数を入力してください (0〜100、bで戻る): ", subject);
            if (scanf("%15s", input) != 1) continue;
            if (strcmp(input, "b") == 0) {
                sqlite3_close(db);
                return;
            }
            int score = atoi(input);
            if (score >= 0 && score <= 100) {
                choice_subject_score[i] = score;
                break;
            }
            printf("0〜100の範囲で入力してください。\n");
        }
    }

    // 確認
    SetConsoleOutputCP(65001);
    printf("\n入力内容の確認:\n");
    printf("名前:%s\nふりがな:%s\n受験日: %d年%02d月%02d日\n", name, furigana, year, month, day);
    for (int i = 0; i < 3; i++) printf("%s: %d点\n", subjects[i], basicscore[i]);
    printf("%s: %d点\n", choicesubjects_1[choicenumber_1 - 1], choice_subject_score[0]);
    printf("%s: %d点\n", choicesubjects_1[choicenumber_2 - 1], choice_subject_score[1]);

    char confirm[16];
    while (1) {
        printf("1: 登録する、2: キャンセル > ");
        if (scanf("%15s", confirm) != 1) continue;
        if (strcmp(confirm, "2") == 0) {
            sqlite3_close(db);
            return;
        }
        if (strcmp(confirm, "1") == 0) break;
        printf("1 または 2 を入力してください。\n");
    }

    // 登録処理
    snprintf(sql, sizeof(sql),
        "INSERT INTO exam (examinee_id, exam_date) VALUES (%d, %d);", examinee_id, exam_date);
    ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "exam 登録エラー: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return;
    }
    int exam_id = (int)sqlite3_last_insert_rowid(db);

    for (int i = 0; i < 3; i++) {
        snprintf(sql, sizeof(sql),
            "INSERT INTO score (exam_id, examinee_id, subject_id, score) VALUES (%d, %d, %d, %d);",
            exam_id, examinee_id, i + 1, basicscore[i]);
        sqlite3_exec(db, sql, 0, 0, NULL);
    }

    snprintf(sql, sizeof(sql),
        "INSERT INTO score (exam_id, examinee_id, subject_id, score) VALUES (%d, %d, %d, %d);",
        exam_id, examinee_id, choicenumber_1 + 3, choice_subject_score[0]);
    sqlite3_exec(db, sql, 0, 0, NULL);

    snprintf(sql, sizeof(sql),
        "INSERT INTO score (exam_id, examinee_id, subject_id, score) VALUES (%d, %d, %d, %d);",
        exam_id, examinee_id, choicenumber_2 + 3, choice_subject_score[1]);
    sqlite3_exec(db, sql, 0, 0, NULL);

    printf("登録が完了しました。\n");
    sqlite3_close(db);
}



// 更新機能
int Update() {
    setlocale(LC_CTYPE, "");
    char* err_msg = NULL;
    int ret;
    char sql[1024];
    char input[60];
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    sqlite3* db = DB_connect();
    if (!db) {
        fprintf(stderr, "データベース接続に失敗しました。\n");
        return 1;
    }

    // 受験者一覧表示
    snprintf(sql, sizeof(sql), "SELECT examinee_id, examinee_name, furigana FROM examinee;");
    sqlite3_stmt* stmt;
    ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "受験者一覧取得失敗: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    int ids[100];
    int count = 0;
    printf("■ 受験者一覧\n");
    while (sqlite3_step(stmt) == SQLITE_ROW && count < 100) {
        ids[count] = sqlite3_column_int(stmt, 0);
        const unsigned char* name = sqlite3_column_text(stmt, 1);
        const unsigned char* kana = sqlite3_column_text(stmt, 2);
        printf("%d: %s（%s）\n", count + 1, name, kana);
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0) {
        printf("登録された受験者がいません。\n");
        sqlite3_close(db);
        return 0;
    }

    // 番号で受験者選択
    int index = -1;
    while (1) {
        printf("更新したい受験者の番号を入力してください（1〜%d、bで戻る）: ", count);
        char buf[16];
        if (scanf("%15s", buf) != 1) {
            clear_stdin();
            continue;
        }
        if (strcmp(buf, "b") == 0) {
            sqlite3_close(db);
            return 0;
        }
        index = atoi(buf);
        if (index >= 1 && index <= count) break;
        printf("1〜%dの番号を入力してください。\n", count);
    }

    int examinee_id = ids[index - 1];
    system("cls");
    // 試験データ確認
    snprintf(sql, sizeof(sql),
        "SELECT e.examinee_name, e.furigana, s.subject_name, sc.score, ex.exam_date "
        "FROM examinee e "
        "JOIN exam ex ON e.examinee_id = ex.examinee_id "
        "JOIN score sc ON ex.exam_id = sc.exam_id "
        "JOIN subjects s ON sc.subject_id = s.subject_id "
        "WHERE e.examinee_id = %d;", examinee_id);

    CallbackData cb_data = { 0, 0, "" };
    ret = sqlite3_exec(db, sql, print_callback, &cb_data, &err_msg);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }

    if (cb_data.count == 0) {
        printf("この受験者は試験データが登録されていません。\n");
        sqlite3_close(db);
        return Update();  // 再度実行（ループ的に）
    }

    int choose;
    char buf[16];
    while (1) {
        printf("\n1: 名前の変更\n2: ふりがなの変更\n3: 受験日の変更\n4: 点数の変更\n5: メインメニューに戻る\n");

        while (1) {
            printf("番号を入力してください (1〜5): ");
            if (scanf("%15s", buf) != 1) {
                clear_stdin();
                printf("入力エラー。\n");
                continue;
            }
            choose = atoi(buf);
            if (choose >= 1 && choose <= 5) {
                break;
            }
            printf("1〜5の番号で入力してください。\n");
        }

    if (choose == 1) {
        system("cls");
        char new_name[60];
        while (1) {
            printf("新しい名前を入力してください（漢字またはカタカナ、30文字以内、bで戻る）: ");
            if (scanf("%59s", new_name) != 1) {
                clear_stdin(); // 入力失敗時にバッファクリア
                printf("入力に失敗しました。\n");
                continue;
            }
            clear_stdin(); // 入力成功後にもクリア

            if (strcmp(new_name, "b") == 0) {
                break;
            }

            if (strlen(new_name) > 30) {
                printf("名前は30文字以内で入力してください。\n");
                continue;
            }

            if (!is_kanji_or_katakana_only(new_name)) {
                printf("名前は漢字またはカタカナで入力してください。\n");
                continue;
            }

            // 入力が正常だったら更新
            snprintf(sql, sizeof(sql),
                "UPDATE examinee SET examinee_name = '%s' WHERE examinee_id = %d;",
                new_name, examinee_id);
            ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
            if (ret != SQLITE_OK) {
                fprintf(stderr, "名前の更新に失敗: %s\n", err_msg);
                sqlite3_free(err_msg);
            }
            else {
                printf("氏名を更新しました。\n");
            }
            break;
        }
    }

        else if (choose == 2) {
            system("cls");
            char new_furigana[60];
            while (1) {
                printf("新しいふりがなを入力してください（すべてひらがな,bキーで戻る）: ");
                if (scanf("%59s", new_furigana) != 1) {
                    fprintf(stderr, "入力に失敗しました。\n");
                    return 1;
                }

                if (strcmp(new_furigana, "b") == 0) {
                    break;
                }

                if (!is_hiragana_only(new_furigana)) {
                    printf("ふりがなはすべてひらがなで入力してください。\n");
                    continue; // 再入力
                }

                int count = 0;
                snprintf(sql, sizeof(sql),
                    "SELECT COUNT(*) FROM examinee WHERE furigana = '%s';", new_furigana);
                ret = sqlite3_exec(db, sql, count_callback, &count, &err_msg);
                if (ret != SQLITE_OK) {
                    fprintf(stderr, "確認に失敗: %s\n", err_msg);
                    sqlite3_free(err_msg);
                    return 1;
                }

                if (count > 0) {
                    printf("すでに同じふりがなが登録されています。\n");
                    continue;
                }

                snprintf(sql, sizeof(sql),
                    "UPDATE examinee SET furigana = '%s' WHERE examinee_id = %d;",
                    new_furigana, examinee_id);
                ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
                if (ret != SQLITE_OK) {
                    fprintf(stderr, "ふりがなの更新に失敗: %s\n", err_msg);
                    sqlite3_free(err_msg);
                }
                else {
                    printf("ふりがなを更新しました。\n");
                }
                break;
            }
        }
        else if (choose == 3) {
        system("cls");

        typedef struct {
            int exam_id;
            int exam_date;
        } ExamEntry;

        ExamEntry exams[100];
        int exam_count = 0;

        snprintf(sql, sizeof(sql),
            "SELECT exam_id, exam_date FROM exam WHERE examinee_id = %d;", examinee_id);
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (ret != SQLITE_OK) {
            fprintf(stderr, "受験日取得失敗: %s\n", sqlite3_errmsg(db));
            return 1;
        }

        printf("■ 受験日一覧:\n");
        while (sqlite3_step(stmt) == SQLITE_ROW && exam_count < 100) {
            exams[exam_count].exam_id = sqlite3_column_int(stmt, 0);
            exams[exam_count].exam_date = sqlite3_column_int(stmt, 1);
            int y = exams[exam_count].exam_date / 10000;
            int m = (exams[exam_count].exam_date / 100) % 100;
            int d = exams[exam_count].exam_date % 100;
            printf("%d: %04d-%02d-%02d\n", exam_count + 1, y, m, d);
            exam_count++;
        }
        sqlite3_finalize(stmt);

        if (exam_count == 0) {
            printf("登録されている受験日がありません。\n");
            return 1;
        }

        // 番号選択（bキー対応）
        char selection_input[16];
        int selected_exam_id = -1;
        while (1) {
            printf("変更したい受験日の番号を選んでください（1〜%d、bで戻る）: ", exam_count);
            if (scanf("%15s", selection_input) != 1) {
                clear_stdin();
                continue;
            }
            clear_stdin();

            if (strcmp(selection_input, "b") == 0) {
                system("cls");
                break; // 戻る
            }

            int selection = atoi(selection_input);
            if (selection >= 1 && selection <= exam_count) {
                selected_exam_id = exams[selection - 1].exam_id;
                break;
            }
            printf("正しい番号を入力してください。\n");
        }

        if (selected_exam_id == -1) {
            continue;  // メニューに戻る
        }

        // 新しい受験日を入力
        char new_date_str[16];
        while (1) {
            printf("新しい受験日を半角8桁で入力してください（例: 20250501、bで戻る）: ");
            if (scanf("%15s", new_date_str) != 1) {
                clear_stdin();
                continue;
            }
            clear_stdin();

            if (strcmp(new_date_str, "b") == 0) {
                break;
            }

            if (strlen(new_date_str) == 8 && strspn(new_date_str, "0123456789") == 8) {
                int new_date = atoi(new_date_str);
                snprintf(sql, sizeof(sql),
                    "UPDATE exam SET exam_date = %d WHERE exam_id = %d;",
                    new_date, selected_exam_id);
                ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
                if (ret != SQLITE_OK) {
                    fprintf(stderr, "更新失敗: %s\n", err_msg);
                    sqlite3_free(err_msg);
                }
                else {
                    printf("受験日を更新しました。\n");
                }
                break;
            }
            else {
                printf("8桁の数字で入力してください。\n");
            }
        }
    }

        else if (choose == 4) {
            system("cls");

            snprintf(sql, sizeof(sql),
                "SELECT exam_id, exam_date FROM exam WHERE examinee_id = %d;", examinee_id);
            sqlite3_stmt* stmt;
            ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
            if (ret != SQLITE_OK) {
                fprintf(stderr, "受験日取得失敗: %s\n", sqlite3_errmsg(db));
                return 1;
            }

            int exam_ids[100];
            int dates[100];
            int exam_count = 0;

            printf("受験日一覧:\n");
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                exam_ids[exam_count] = sqlite3_column_int(stmt, 0);
                dates[exam_count] = sqlite3_column_int(stmt, 1);
                int y = dates[exam_count] / 10000;
                int m = (dates[exam_count] / 100) % 100;
                int d = dates[exam_count] % 100;
                printf("%d: %04d-%02d-%02d\n", exam_count + 1, y, m, d);
                exam_count++;
            }
            sqlite3_finalize(stmt);

            if (exam_count == 0) {
                printf("受験日が見つかりませんでした。\n");
                return 1;
            }

            // 受験日選択
            int selected_exam_id = -1;
            char input[16];
            while (1) {
                printf("点数を変更したい受験日の番号を入力してください（1〜%d、bで戻る）: ", exam_count);
                if (scanf("%15s", input) != 1) {
                    clear_stdin();
                    continue;
                }
                clear_stdin();

                if (strcmp(input, "b") == 0) {
                    system("cls");
                    break;  // メニューに戻る
                }

                int selected = atoi(input);
                if (selected >= 1 && selected <= exam_count) {
                    selected_exam_id = exam_ids[selected - 1];
                    break;
                }
                printf("正しい番号を入力してください。\n");
            }

            if (selected_exam_id == -1) {
                continue; // メニューに戻る
            }


            // 選択された受験日の科目一覧を表示
            snprintf(sql, sizeof(sql),
                "SELECT s.subject_name FROM score sc "
                "JOIN subjects s ON sc.subject_id = s.subject_id "
                "WHERE sc.exam_id = %d;", selected_exam_id);

            ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
            if (ret != SQLITE_OK) {
                fprintf(stderr, "科目取得失敗: %s\n", sqlite3_errmsg(db));
                return 1;
            }

            char subject_list[10][64];
            int subject_count = 0;

            printf("科目一覧:\n");
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* sub = sqlite3_column_text(stmt, 0);
                snprintf(subject_list[subject_count], sizeof(subject_list[subject_count]), "%s", sub);
                printf("%d: %s\n", subject_count + 1, sub);
                subject_count++;
            }
            sqlite3_finalize(stmt);

            if (subject_count == 0) {
                printf("科目が見つかりませんでした。\n");
                return 1;
            }

            int subject_index;
            while (1) {
                printf("点数を変更したい科目の番号を入力してください（1〜%d）: ", subject_count);
                if (scanf("%d", &subject_index) != 1 || subject_index < 1 || subject_index > subject_count) {
                    printf("正しい番号を入力してください。\n");
                    while (getchar() != '\n');
                    continue;
                }
                break;
            }

            char* selected_subject = subject_list[subject_index - 1];
            int new_score;

            while (1) {
                printf("%s の新しい点数を入力してください (0〜100): ", selected_subject);
                if (scanf("%d", &new_score) == 1 && new_score >= 0 && new_score <= 100) break;
                printf("0〜100の範囲で入力してください。\n");
                while (getchar() != '\n');
            }

            snprintf(sql, sizeof(sql),
                "UPDATE score SET score = %d WHERE exam_id = %d AND subject_id = (SELECT subject_id FROM subjects WHERE subject_name = '%s');",
                new_score, selected_exam_id, selected_subject);

            ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
            if (ret != SQLITE_OK) {
                fprintf(stderr, "点数の更新失敗: %s\n", err_msg);
                sqlite3_free(err_msg);
            }
            else {
                printf("点数を更新しました。\n");
            }
        }
        else if (choose == 5) {
            break;
        }
        else {
            printf("正しい番号を選んでください。\n");
        }

        // 更新後に再表示
        snprintf(sql, sizeof(sql),
            "SELECT e.examinee_name, e.furigana, s.subject_name, sc.score, ex.exam_date "
            "FROM examinee e "
            "JOIN exam ex ON e.examinee_id = ex.examinee_id "
            "JOIN score sc ON ex.exam_id = sc.exam_id "
            "JOIN Subjects s ON sc.subject_id = s.subject_id "
            "WHERE e.examinee_id = %d;", examinee_id);
      
        CallbackData cb_data = { 0, 0, "" };

        sqlite3_exec(db, sql, print_callback, &cb_data, NULL);
    }

    sqlite3_close(db);
    return 0;
}



 



