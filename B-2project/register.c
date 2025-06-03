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
    const char* exam_date = argv[4] ? argv[4] : "NULL";

    if (!cb_data->first_print_done) {
        printf("名前: %s\n", name);
        printf("ふりがな: %s\n\n", furigana);
        cb_data->first_print_done = 1;
    }

    // 試験日が変わったら表示
    if (strcmp(cb_data->prev_exam_date, exam_date) != 0) {
        printf("試験日: %s\n", exam_date);
        strncpy(cb_data->prev_exam_date, exam_date, sizeof(cb_data->prev_exam_date) - 1);
        cb_data->prev_exam_date[sizeof(cb_data->prev_exam_date) - 1] = '\0';
    }

    printf("  %-6s: %6s点\n", subject, score); // help:点数の表記位置合わせられてないです

    return 0;
}

// 不正な値を受け取ったらバッファをクリアする処理
void clear_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

// 新規登録機能
int New_register() {
    char* err_msg = NULL;
    int ret;
    char sql[256];
    char name[59];
    char furigana[59];
    char date_input[16];
    int namecount = 0;
    SetConsoleOutputCP(CP_UTF8);  // 出力をUTF-8に設定
    SetConsoleCP(CP_UTF8);

    sqlite3* db = DB_connect();
    if (!db) {
        fprintf(stderr, "データベース接続失敗\n");
        return 1;
    }

    //while (1) {
    while (1) {
        printf("氏名を入力してください(漢字またはカタカナ):\n ");
        if (scanf("%59s", name) != 1) {
            clear_stdin();
            fprintf(stderr, "氏名入力失敗\n");
            return 1;
        }
        /*氏名がカタカナor漢字ではない→再入力
        if (!is_katakana_or_kanji_only(name)) {
            printf("氏名は漢字またはカタカナで入力してください(例:麻生花子またはアソウハナコ)。\n");
            continue;
        }*/

        printf("ふりがなを入力してください:\n");
        if (scanf("%59s", furigana) != 1) {
            fprintf(stderr, "ふりがなの入力失敗。\n");
            return 1;
        }
        /* ふりがな以外が入力されたら再入力
        if (!is_hiragana(furigana)) {
            printf("ふりがなはすべてひらがなで入力してください。\n");
            continue;
        }*/
        // ふりがなが重複しているかDB内を確認
        snprintf(sql, sizeof(sql),
            "SELECT COUNT(*) FROM examinee WHERE furigana = '%s';", furigana);
        namecount = 0;
        ret = sqlite3_exec(db, sql, count_callback, &namecount, &err_msg);

        if (ret != SQLITE_OK) {
            fprintf(stderr, "DBエラー: %s\n", err_msg);
            sqlite3_free(err_msg);
            return 1;
        }

        if (namecount > 0) {
            printf("同じふりがながすでに登録されています。\n"); //ふりがな重複チェック
            continue;
        }
        break;
        // ふりがなが重複していない→試験日入力へ
    }

        int exam_date = 0;

        printf("試験日を半角8桁で入力してください（例: 20250501）: ");
        if (scanf("%15s", date_input) != 1) {
            clear_stdin();
            fprintf(stderr, "試験日入力失敗。\n");
            return 0;
        }
        //8桁以外の数字を弾く
        if (strlen(date_input) != 8 || strspn(date_input, "0123456789") != 8) {
            fprintf(stderr, "試験日は8桁の半角数字で入力してください。\n");
            return 0;
        }

        exam_date = atoi(date_input);
        int year = exam_date / 10000;
        int month = (exam_date / 100) % 100;
        int day = exam_date % 100;
        const char* subjects[] = { "国語", "数学", "英語" }; //必須科目用配列
        const char* choicesubjects_1[] = { "日本史", "世界史", "地理", "物理", "化学", "生物" }; //選択科目用
        int basicscore[3]; //必須三科目の得点用
        int choice_subject_score[2]; // 選択科目用
        int choicenumber_1 = -1, choicenumber_2 = -1; // 文系、理系別の選択肢用:(-1)→まだ何も選ばれてない状態

        // 科目毎の点数入力処理
        for (int i = 0; i < 3; i++) {
            while (1) {
                printf("%sの点数を入力してください (0〜100): ", subjects[i]);
                if (scanf("%d", &basicscore[i]) != 1) {
                    fprintf(stderr, "数値入力失敗。\n");
                    return 1;
                }
                // 0～100外で入力されたら再入力
                if (basicscore[i] >= 0 && basicscore[i] <= 100) break;
                printf("0〜100の範囲で入力してください。\n");
            }
        }
        //選択科目入力(文系科目)
        printf("選択科目（文系）を1つ選んでください:\n");
        for (int i = 0; i < 3; i++) {
            printf("%d: %s\n", i + 1, choicesubjects_1[i]);
        }
        while (1) {
            printf("番号を入力 (1〜3): ");
            if (scanf("%d", &choicenumber_1) != 1) {
                fprintf(stderr, "番号の入力に失敗しました。\n");
                return 1;
            }
            if (choicenumber_1 >= 1 && choicenumber_1 <= 3)
                break;
            printf("正しい番号を入力してください。\n");
        }
        //選択科目入力(理系科目)
        printf("選択科目（理系）を1つ選んでください:\n");
        for (int i = 3; i < 6; i++) {
            printf("%d: %s\n", i + 1, choicesubjects_1[i]);
        }
        while (1) {
            printf("番号を入力 (4〜6): ");
            if (scanf("%d", &choicenumber_2) != 1) {
                fprintf(stderr, "番号の入力に失敗しました。\n");
                return 1;
            }
            if (choicenumber_2 >= 4 && choicenumber_2 <= 6)
                break;
            printf("正しい番号を入力してください。\n");
        }

        // 文系選択科目の点数入力
        printf("%sの点数を入力してください (0〜100): ", choicesubjects_1[choicenumber_1 - 1]);
        while (1) {
            if (scanf("%d", &choice_subject_score[0]) != 1) {
                fprintf(stderr, "点数入力失敗\n");
                return 1;
            }
            if (choice_subject_score[0] >= 0 && choice_subject_score[0] <= 100)
                break;
            printf("正しい点数を入力してください。\n");
        }
        // 理系選択科目の点数入力
        printf("%sの点数を入力してください (0〜100): ", choicesubjects_1[choicenumber_2 - 1]);
        while (1) {
            if (scanf("%d", &choice_subject_score[1]) != 1) {
                fprintf(stderr, "点数入力失敗\n");
                return 1;
            }
            if (choice_subject_score[1] >= 0 && choice_subject_score[1] <= 100) break;
            printf("正しい点数を入力してください。\n");
        }
        // 登録情報確認画面
        SetConsoleOutputCP(65001);
        printf("\n入力内容の確認:\n");
        printf("名前:%s\n", name);
        printf("ふりがな:%s\n", furigana);
        printf("受験日: %d年%02d月%02d日\n", year, month, day);

        for (int i = 0; i < 3; i++) {
            printf("%s: %d点\n", subjects[i], basicscore[i]);
        }
        printf("%s: %d点\n", choicesubjects_1[choicenumber_1 - 1], choice_subject_score[0]);
        printf("%s: %d点\n", choicesubjects_1[choicenumber_2 - 1], choice_subject_score[1]);

        int ok;
        printf("1 を入力すると登録、2 で氏名入力に戻ります ");
        if (scanf("%d", &ok) != 1) {
            fprintf(stderr, "確認の入力に失敗しました。\n");
            return 1;
        }
        if (ok == 1) {
            printf("登録中です\n");
        }
        else if (ok == 2) {
            sqlite3_close(db);
            //continue;
        }
        else {
            printf("対象外の番号が選択されました。メインメニューに戻ります");
            sqlite3_close(db);
        }
    
    // INSERT 処理(名前、ふりがな登録)
    snprintf(sql, sizeof(sql),
        "INSERT INTO examinee (examinee_name, furigana) VALUES ('%s', '%s');", name, furigana);
    ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "examinee の挿入エラー: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }
    int examinee_id = (int)sqlite3_last_insert_rowid(db);

    snprintf(sql, sizeof(sql),
        "INSERT INTO exam (examinee_id, exam_date) VALUES (%d, %d);", examinee_id, exam_date);
    ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "exam の挿入エラー: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }
    int exam_id = (int)sqlite3_last_insert_rowid(db);

    // 必須科目登録
    for (int i = 0; i < 3; i++) {
        snprintf(sql, sizeof(sql),
            "INSERT INTO score (exam_id, examinee_id,subject_id, score) VALUES (%d, %d, %d, %d);",
            exam_id, examinee_id, i + 1, basicscore[i]);
        ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
        if (ret != SQLITE_OK) {
            fprintf(stderr, "score (必須) の挿入エラー: %s\n", err_msg);
            sqlite3_free(err_msg);
            sqlite3_close(db);
            return 1;
        }
    }

    // 文系選択科目選登録
    snprintf(sql, sizeof(sql),
        "INSERT INTO score (exam_id, examinee_id,subject_id, score) VALUES (%d, %d, %d, %d);",
        exam_id, examinee_id, choicenumber_1 + 3, choice_subject_score[0]);
    sqlite3_exec(db, sql, 0, 0, NULL);

    // 理系選択科目登録
    snprintf(sql, sizeof(sql),
        "INSERT INTO score (exam_id, examinee_id, subject_id, score) VALUES (%d, %d, %d, %d);",
        exam_id, examinee_id, choicenumber_2 + 3, choice_subject_score[1]);
    sqlite3_exec(db, sql, 0, 0, NULL);

    printf("登録が完了しました\n");
    sqlite3_close(db);

    return 0;
    }



    void Add_register() { //既受験者の試験結果登録
        char* err_msg = NULL;
        int ret;
        char sql[1024];
        char name[59];
        char furigana[59];
        char date_input[16];
        SetConsoleOutputCP(CP_UTF8);  // 出力をUTF-8に設定
        SetConsoleCP(CP_UTF8);

        sqlite3* db = DB_connect();
        if (!db) {
            fprintf(stderr, "データベース接続に失敗しました。\n");
            return;
        }

        printf("ふりがなを入力してください（すべてひらがな）: ");
        if (scanf("%58s", furigana) != 1) {
            fprintf(stderr, "ふりがなの入力に失敗しました。\n");
            sqlite3_close(db);
            return;
        }
        // ふりがなが存在するか確認→確認取れたら試験情報の追加登録画面へ
        snprintf(sql, sizeof(sql),
            // テーブル結合
            "SELECT e.examinee_name, e.furigana, s.subject_name, sc.score, ex.exam_date "
            "FROM examinee e "
            "JOIN exam ex ON e.examinee_id = ex.examinee_id "
            "JOIN score sc ON ex.exam_id = sc.exam_id "
            "JOIN subjects s ON sc.subject_id = s.subject_id "
            "WHERE e.furigana = '%s';", furigana);


        CallbackData cb_data = { 0, 0 };

        ret = sqlite3_exec(db, sql, print_callback, &cb_data, &err_msg);

        if (ret != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", err_msg);
            sqlite3_free(err_msg);
        }
        else if (cb_data.count == 0) {
            printf("該当するデータが見つかりませんでした。\n");
            return;
        }
        /*if (!is_hiragana(furigana)) {
            printf("ふりがなはすべてひらがなで入力してください。\n");
            continue;
        }*/

        int exam_date = 0;

        // 試験日入力
        printf("試験日を半角8桁で入力してください（例: 20250501）: ");
        if (scanf("%15s", date_input) != 1) {
            clear_stdin();
            fprintf(stderr, "試験日入力失敗。\n");
            return 0;
        }
        //8桁以外の数字を弾く
        if (strlen(date_input) != 8 || strspn(date_input, "0123456789") != 8) {
            fprintf(stderr, "試験日は8桁の半角数字で入力してください。\n");
            return 0;
        }
        exam_date = atoi(date_input);
        int year = exam_date / 10000;
        int month = (exam_date / 100) % 100;
        int day = exam_date % 100;
        const char* subjects[] = { "国語", "数学", "英語" };
        const char* choicesubjects_1[] = { "日本史", "世界史", "地理", "物理", "化学", "生物" };
        int basicscore[3];
        int choice_subject_score[2];
        int choicenumber_1 = -1, choicenumber_2 = -1;

        // 科目毎の点数入力処理
        for (int i = 0; i < 3; i++) {
            while (1) {
                printf("%sの点数を入力してください (0〜100): ", subjects[i]);
                if (scanf("%d", &basicscore[i]) != 1) {
                    fprintf(stderr, "数値入力失敗。\n");
                    return 1;
                }
                if (basicscore[i] >= 0 && basicscore[i] <= 100) break;
                printf("0〜100の範囲で入力してください。\n");
            }
        }
        // 文系選択科目入力
        printf("選択科目（文系）を1つ選んでください:\n");
        for (int i = 0; i < 3; i++) {
            printf("%d: %s\n", i + 1, choicesubjects_1[i]);
        }
        while (1) {
            printf("番号を入力 (1〜3): ");
            if (scanf("%d", &choicenumber_1) != 1) {
                fprintf(stderr, "番号の入力に失敗しました。\n");
                return 1;
            }
            if (choicenumber_1 >= 1 && choicenumber_1 <= 3)
                break;
            printf("正しい番号を入力してください。\n");
        }
        // 理系選択科目入力
        printf("選択科目（理系）を1つ選んでください:\n");
        for (int i = 3; i < 6; i++) {
            printf("%d: %s\n", i + 1, choicesubjects_1[i]);
        }
        while (1) {
            printf("番号を入力 (4〜6): ");
            if (scanf("%d", &choicenumber_2) != 1) {
                fprintf(stderr, "番号の入力に失敗しました。\n");
                return 1;
            }
            if (choicenumber_2 >= 4 && choicenumber_2 <= 6)
                break;
            printf("正しい番号を入力してください。\n");
        }

        // 文系選択科目の点数入力
        printf("%sの点数を入力してください (0〜100): ", choicesubjects_1[choicenumber_1 - 1]);
        while (1) {
            if (scanf("%d", &choice_subject_score[0]) != 1) {
                fprintf(stderr, "点数入力失敗\n");
                return 1;
            }
            if (choice_subject_score[0] >= 0 && choice_subject_score[0] <= 100)
                break;
            printf("正しい点数を入力してください。\n");
        }
        // 理系選択科目の点数入力
        printf("%sの点数を入力してください (0〜100): ", choicesubjects_1[choicenumber_2 - 1]);
        while (1) {
            if (scanf("%d", &choice_subject_score[1]) != 1) {
                fprintf(stderr, "点数入力失敗\n");
                return 1;
            }
            // 0点以上100点以外を入力→再入力
            if (choice_subject_score[1] >= 0 && choice_subject_score[1] <= 100) break;
            printf("正しい点数を入力してください。\n");
        }
        //入力内容確認画面(help:名前とフリガナ反映されないです)
        SetConsoleOutputCP(CP_UTF8);  // 出力をUTF-8に設定
        SetConsoleCP(CP_UTF8);
        printf("\n入力内容の確認:\n");
        printf("名前:%s\n", name);
        printf("ふりがな:%s\n", furigana);
        printf("受験日: %d年%02d月%02d日\n", year, month, day);

        for (int i = 0; i < 3; i++) {
            printf("%s: %d点\n", subjects[i], basicscore[i]);
        }
        printf("%s: %d点\n", choicesubjects_1[choicenumber_1 - 1], choice_subject_score[0]);
        printf("%s: %d点\n", choicesubjects_1[choicenumber_2 - 1], choice_subject_score[1]);

        int ok;
        printf("1 を入力すると登録、2 でキャンセル: ");
        if (scanf("%d", &ok) != 1) {
            fprintf(stderr, "確認の入力に失敗しました。\n");
            return 1;
        }
        if (ok == 1) {
            printf("登録中です\n");
        }
        else if (ok == 2) {
            sqlite3_close(db);
            return;
        }
        else {
            sqlite3_close(db);
        }

        // INSERT 処理
        snprintf(sql, sizeof(sql),
            "SELECT examinee_id FROM examinee WHERE furigana = '%s';", furigana);
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (ret != SQLITE_OK) {
            fprintf(stderr, "SELECT文の準備に失敗しました: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return 1;
        }

        int examinee_id = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            // 既存の examinee_id を取得
            examinee_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        // 存在しない場合のみ INSERT
        if (examinee_id == -1) {
            snprintf(sql, sizeof(sql),
                "INSERT INTO examinee (examinee_name, furigana) VALUES ('%s', '%s');", name, furigana);
            ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
            if (ret != SQLITE_OK) {
                fprintf(stderr, "examinee の挿入エラー: %s\n", err_msg);
                sqlite3_free(err_msg);
                sqlite3_close(db);
                return 1;
            }
            examinee_id = (int)sqlite3_last_insert_rowid(db);
        }

        snprintf(sql, sizeof(sql),
            "INSERT INTO exam (examinee_id, exam_date) VALUES (%d, %d);", examinee_id, exam_date);
        ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
        if (ret != SQLITE_OK) {
            fprintf(stderr, "exam の挿入エラー: %s\n", err_msg);
            sqlite3_free(err_msg);
            sqlite3_close(db);
            return 1;
        }
        int exam_id = (int)sqlite3_last_insert_rowid(db);

        // 必須科目登録
        for (int i = 0; i < 3; i++) {
            snprintf(sql, sizeof(sql),
                "INSERT INTO score (exam_id, examinee_id,subject_id, score) VALUES (%d, %d, %d, %d);",
                exam_id, examinee_id, i + 1, basicscore[i]);
            ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
            if (ret != SQLITE_OK) {
                fprintf(stderr, "score (必須) の挿入エラー: %s\n", err_msg);
                sqlite3_free(err_msg);
                sqlite3_close(db);
                return 1;
            }
        }

        // 文系選択科目登録
        snprintf(sql, sizeof(sql),
            "INSERT INTO score (exam_id, examinee_id,subject_id, score) VALUES (%d, %d, %d, %d);",
            exam_id, examinee_id, choicenumber_1 + 3, choice_subject_score[0]);
        sqlite3_exec(db, sql, 0, 0, NULL);

        // 理系選択科目登録
        snprintf(sql, sizeof(sql),
            "INSERT INTO score (exam_id, examinee_id, subject_id, score) VALUES (%d, %d, %d, %d);",
            exam_id, examinee_id, choicenumber_2 + 3, choice_subject_score[1]);
        sqlite3_exec(db, sql, 0, 0, NULL);

        printf("登録が完了しました\n");
        sqlite3_close(db);

        return 0;
    }






// 変更用
    void Update() {
        char* err_msg = NULL;
        int ret;
        int select;
        char furigana[60];
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        sqlite3* db = DB_connect();
        if (!db) {
            fprintf(stderr, "データベース接続に失敗しました。\n");
            return;
        }

        printf("ふりがなをひらがなで入力してください\n");
        if (scanf("%59s", furigana) != 1) {
            fprintf(stderr, "ふりがなの入力に失敗しました。\n");
            return;
        }

        char sql[1024];  // バッファを少し大きくしました
            snprintf(sql, sizeof(sql),
                // テーブル結合
                "SELECT e.examinee_name, e.furigana, s.subject_name, sc.score, ex.exam_date "
                "FROM examinee e "
                "JOIN exam ex ON e.examinee_id = ex.examinee_id "
                "JOIN score sc ON ex.exam_id = sc.exam_id "
                "JOIN subjects s ON sc.subject_id = s.subject_id "
                "WHERE e.furigana = '%s';", furigana);
            

        CallbackData cb_data = { 0, 0 };

        ret = sqlite3_exec(db, sql, print_callback, &cb_data, &err_msg);

        if (ret != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", err_msg);
            sqlite3_free(err_msg);
        }
        else if (cb_data.count == 0) {
            printf("該当するデータが見つかりませんでした。\n");
            return;
        }

        int choose = 0;
        while (1) {
            printf("1:名前の変更\n");
            printf("2:ふりがなの変更\n");
            printf("3:受験日の変更\n");
            printf("4:点数の変更\n");
            printf("5:メインメニューに戻る\n");
            if (scanf("%d", &choose) != 1) {
                fprintf(stderr, "数値の入力に失敗しました。\n");
                return;
            }
            
            if (choose == 1) {
                char new_name[60];
                printf("新しい名前を入力してください: ");
                if (scanf("%59s", new_name) != 1) {
                    fprintf(stderr, "入力に失敗しました。\n");
                    return;
                }

                snprintf(sql, sizeof(sql),
                    "UPDATE examinee SET examinee_name = '%s' WHERE furigana = '%s';",
                    new_name, furigana);

                ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
                if (ret != SQLITE_OK) {
                    fprintf(stderr, "名前の更新に失敗: %s\n", err_msg);
                    sqlite3_free(err_msg);
                }
                else {
                    printf("氏名を更新しました。\n");
                }
            }
            else if (choose == 2) {
                char new_furigana[60];
                printf("新しいふりがなを入力してください: ");
                if (scanf("%59s", new_furigana) != 1) {
                    fprintf(stderr, "入力に失敗しました。\n");
                    return;
                }

                // 新しいふりがなと重複チェック
                int count = 0;
                snprintf(sql, sizeof(sql),
                    "SELECT COUNT(*) FROM examinee WHERE furigana = '%s';", new_furigana);
                ret = sqlite3_exec(db, sql, count_callback, &count, &err_msg);
                if (ret != SQLITE_OK) {
                    fprintf(stderr, "確認に失敗: %s\n", err_msg);
                    sqlite3_free(err_msg);
                }
                else if (count > 0) {
                    printf("すでに同じふりがなが登録されています。\n");
                }
                else {
                    snprintf(sql, sizeof(sql),
                        "UPDATE examinee SET furigana = '%s' WHERE furigana = '%s';",
                        new_furigana, furigana);

                    ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
                    if (ret != SQLITE_OK) {
                        fprintf(stderr, "ふりがなの更新に失敗: %s\n", err_msg);
                        sqlite3_free(err_msg);
                    }
                    else {
                        printf("ふりがなを更新しました。\n");
                        strcpy(furigana, new_furigana);  // 現在のふりがなを更新
                    }
                }
            }
            // 受験日の登録
            else if (choose == 3) {
                int new_exam_date;
                printf("新しい受験日（例: 20250501）を入力してください: ");
                if (scanf("%d", &new_exam_date) != 1) {
                    fprintf(stderr, "入力に失敗しました。\n");
                    return;
                }


                snprintf(sql, sizeof(sql),
                    "UPDATE exam SET exam_date = %d "
                    "WHERE examinee_id = (SELECT examinee_id FROM examinee WHERE furigana = '%s');",
                    new_exam_date, furigana);

                ret = sqlite3_exec(db, sql, 0, 0, &err_msg);
                if (ret != SQLITE_OK) {
                    fprintf(stderr, "受験日の更新失敗: %s\n", err_msg);
                    sqlite3_free(err_msg);
                }
                else {
                    printf("受験日を更新しました。\n");
                }

            }
            // 科目毎の点数変更(help:科目名が見つかりませんと表示されます)
            else if (choose == 4) {
                char subject_name[64];
                int new_score;

                printf("変更したい科目名を入力してください（例: 数学）: ");
                if (scanf("%60s", subject_name) != 1) {
                    fprintf(stderr, "入力に失敗しました。\n");
                    return;
                }

                // subject_id を取得
                int subject_id = -1;
                snprintf(sql, sizeof(sql),
                    "SELECT subject_id FROM Subjects WHERE subject_name = '%s';", subject_name);
                ret = sqlite3_exec(db, sql, count_callback, &subject_id, &err_msg);
                if (ret != SQLITE_OK || subject_id == -1) {
                    fprintf(stderr, "科目名が見つかりません: %s\n", err_msg ? err_msg : "");
                    if (err_msg) sqlite3_free(err_msg);
                    continue;
                }

                // examinee_id を取得
                int examinee_id = -1;
                snprintf(sql, sizeof(sql),
                    "SELECT examinee_id FROM examinee WHERE furigana = '%s';", furigana);
                ret = sqlite3_exec(db, sql, count_callback, &examinee_id, &err_msg);
                if (ret != SQLITE_OK || examinee_id == -1) {
                    fprintf(stderr, "ふりがなから受験者が見つかりません: %s\n", err_msg ? err_msg : "");
                    if (err_msg) sqlite3_free(err_msg);
                    continue;
                }

                // exam_id を取得
                int exam_id = -1;
                snprintf(sql, sizeof(sql),
                    "SELECT exam_id FROM exam WHERE examinee_id = %d;", examinee_id);
                ret = sqlite3_exec(db, sql, count_callback, &exam_id, &err_msg);
                if (ret != SQLITE_OK || exam_id == -1) {
                    fprintf(stderr, "exam_id の取得に失敗しました: %s\n", err_msg ? err_msg : "");
                    if (err_msg) sqlite3_free(err_msg);
                    continue;
                }

                // 新しい点数を入力
                printf("新しい点数を入力してください (0〜100): ");
                if (scanf("%d", &new_score) != 1 || new_score < 0 || new_score > 100) {
                    fprintf(stderr, "正しい点数を入力してください。\n");
                    continue;
                }

                // 点数を更新
                snprintf(sql, sizeof(sql),
                    "UPDATE score SET score = %d WHERE exam_id = %d AND subject_id = %d;",
                    new_score, exam_id, subject_id);
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
                printf("該当番号を入力してください。(1～5) \n");
            }
        }

        sqlite3_close(db);

        return 0;
    }
    int register_select_menu() {
        int select;

        while (1) {
            printf("受験者の登録方法を選択してください\n");
            printf("1: 新規登録　　2: 試験結果登録(既受験者用)　　3:メインメニューに戻る\n");
            if (scanf("%d", &select) != 1) {
                fprintf(stderr, "入力エラー。\n");
                return 1;
            }

            if (select == 1) {
                New_register();
                break;
            }
            else if (select == 2) {
                Add_register();
                break;
            }
            else if (select == 3) {
                break;
            }
            else {
                printf("該当番号を入力してください。(1～2) \n");
            }
        }

        return 0;
    }




 



