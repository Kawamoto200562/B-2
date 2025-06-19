#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include "register.h"
#include "refer1.h"
#include "refer2.h"
#include "refer3.h"
#include "delete.h"
#include "main_db.h"

void clear_stdin_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// 登録機能のメニュー
void show_register_menu(sqlite3* db) {
    char input[32];
    int register_choice;

    while (1) {
        system("cls");
        printf("\n== 登録メニュー ==\n");
        printf("1:新規受験者情報の登録\n");
        printf("2:既登録受験者の試験結果登録\n");
        printf("0:メインメニューに戻る\n");

        while (1) {
            printf("番号を入力してください: ");
            fgets(input, sizeof(input), stdin);

            // 空白だけの入力を弾く
            if (input[0] == '\n') {
                printf("空白は無効です。０から２の数字で入力してください。\n");
                continue;
            }

            // 数字として正しく読めたか確認
            if (sscanf(input, "%d", &register_choice) != 1) {
                printf("０から２の数字で入力してください。\n");
                continue;
            }

            if (register_choice < 0 || register_choice > 2) {
                printf("０から２の数字で入力してください。\n");
                continue;
            }

            break;
        }

        system("cls");
        switch (register_choice) {
        case 1:
            New_register(); // 新規受験者情報の登録
            break;
        case 2:
            Add_register(); // 既登録受験者の試験結果登録
            break;
        case 0:
            printf("メインメニューに戻ります\n");
            return;
        }

        printf("\nメニューに戻ります。Enterを押してください。");
        while (getchar() != '\n');
    }
}


void show_delete_menu(sqlite3* db) {
    char input[32];
    int delete_choice;

    while (1) {
        system("cls");
        printf("\n== 削除メニュー ==\n");
        printf("1:登録受験者について受験者単位の削除\n");
        printf("2:登録受験者について受験者の試験単位の削除\n");
        printf("0:メインメニューに戻る\n");

        while (1) {
            printf("番号を入力してください: ");
            fgets(input, sizeof(input), stdin);

            if (input[0] == '\n') {
                printf("空白は無効です。０から２の数字で入力してください。\n");
                continue;
            }

            if (sscanf(input, "%d", &delete_choice) != 1) {
                printf("０から２の数字で入力してください。\n");
                continue;
            }

            if (delete_choice < 0 || delete_choice > 2) {
                printf("０から２の数字で入力してください。\n");
                continue;
            }

            break;
        }

        system("cls");
        switch (delete_choice) {
        case 1:
            delete_by_examinee(db);
            break;
        case 2:
            delete_by_exam(db);
            break;
        case 0:
            printf("メインメニューに戻ります\n");
            return;
        }

        printf("\nメニューに戻ります。Enterを押してください。");
        while (getchar() != '\n');
    }
}


// 参照機能のメニュー
void show_reference_menu(sqlite3* db) {
    char input[32];
    int reference_choice;

    while (1) {
        system("cls");
        printf("\n== 参照メニュー ==\n");
        printf("1:試験実施日毎の各科目トップ５\n");
        printf("2:試験実施日毎の全科目トップ５\n");
        printf("3:試験実施日毎の全科目平均点数\n");
        printf("4:試験実施日毎の各科目平均点数以下の受験者一覧\n");
        printf("5:試験実施日毎の全科目平均点数以下の受験者一覧\n");
        printf("6:全試験における各科目合計トップ１０\n");
        printf("7:全試験における全科目合計トップ１０\n");
        printf("8:各科目の平均点数\n");
        printf("9:全科目の平均点数\n");
        printf("10:各科目平均点以下の受験者一覧\n");
        printf("11:全科目平均点以下の受験者一覧\n");
        printf("0:メインメニューに戻る\n");

        while (1) {
            printf("番号を入力してください: ");
            fgets(input, sizeof(input), stdin);

            if (input[0] == '\n') {
                printf("空白は無効です。０から１１の数字で入力してください。\n");
                continue;
            }

            if (sscanf(input, "%d", &reference_choice) != 1) {
                printf("０から１１の数字で入力してください。\n");
                continue;
            }

            if (reference_choice < 0 || reference_choice > 11) {
                printf("０から１１の数字で入力してください。\n");
                continue;
            }

            break;
        }

        system("cls");

        switch (reference_choice) {
        case 1:
            show_top5_by_exam_subject(db);
            break;
        case 2:
            show_date_top_name();
            break;
        case 3:
            show_avarage_score_by_exam_date(); 
            break;
        case 4:
            show_names_below_ave_by_subject();
            break;
        case 5:
            show_names_below_ave_by_all();
            break;
        case 6:
            show_subject_top_name();
            break;
        case 7:
            show_total_top_name();
            break;
        case 8:
            show_average_by_subject(db);
            break;
        case 9:
            show_total_average(db);
            break;
        case 10:
            show_examinees_below_subject_avg(db);
            break;
        case 11:
            show_examinees_below_selected_avg(db);
            break;
        case 0:
            printf("メインメニューに戻ります\n");
            return;
        }

        printf("\nメニューに戻ります。Enterを押してください。");
        while (getchar() != '\n');
    }
}


int main(void) {   
    sqlite3* db;
    int main_choice;

    // データベースを開く
    if (sqlite3_open("test.db", &db) != SQLITE_OK) {
        printf("データベースを開けません: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // メインメニュー
    while (1) {
        char input[16];
        int main_choice;

        system("cls");
        printf("\n== メインメニュー ==\n");
        printf("1: 登録機能\n");
        printf("2: 更新機能\n");
        printf("3: 削除機能\n");
        printf("4: 参照機能\n");
        printf("5: 受験者一覧\n");
        printf("0: 終了\n");

        while (1) {
            printf("番号を入力してください: ");
            fgets(input, sizeof(input), stdin);

            // 入力が空（空白や改行のみ）なら再入力
            if (input[0] == '\n') {
                printf("空白は無効です。０から４の数字で入力してください。\n");
                continue;
            }

            if (sscanf(input, "%d", &main_choice) != 1) {
                printf("０から４の数字で入力してください。\n");
                continue;
            }

            break;
        }

        switch (main_choice) {
        case 1:
            system("cls");
            show_register_menu(db);
            break;
        case 2:
            system("cls");
            Update();
            break;
        case 3:
            system("cls");
            show_delete_menu(db);
            break;
        case 4:
            system("cls");
            show_reference_menu(db);
            break;
        case 5:
            system("cls");
            main_db();
            break;
        case 0:
            system("cls");
            printf("終了します。\n");
            sqlite3_close(db);
            return 0;
        default:
            system("cls");
            printf("無効な番号です。\n０から４の数字で入力してください。\n");
            break;
        }

        printf("\nメニューに戻ります。Enterを押してください。");
        fgets(input, sizeof(input), stdin);  // ここも getchar() じゃなく fgets に変更
    }


    sqlite3_close(db);
    return 0;
}
