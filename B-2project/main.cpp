#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include "refer3.h"

// 登録機能のメニュー
void show_register_menu(sqlite3* db) {
    int register_choice;
    while (1) {
        system("cls");
        printf("\n== 登録メニュー ==\n");
        printf("1:新規受験者情報の登録\n");
        printf("2:既登録受験者の試験結果登録\n");       
        printf("0:メインメニューに戻る\n");
        printf("番号を入力してください: ");
        if (scanf("%d", &register_choice) != 1) {
            while (getchar() != '\n'); // 入力バッファクリア
            continue;
        }

        system("cls");

        switch (register_choice) {

        case 1:
            // 新規受験者情報の登録の関数をここに
            break;
        case 2:
            // 既登録受験者の試験結果登録の関数をここに
            break;        
        case 0:
            printf("メインメニューに戻ります");
            return;
        default:
            printf("無効な番号です。\n");
            printf("０から２の数字で入力してください。");
            break;
        }
        printf("\n続けるにはEnterを押してください...");
        while (getchar() != '\n');
        getchar();
    }
}

// 削除機能のメニュー
void show_delete_menu(sqlite3* db) {
    int delete_choice;
    while (1) {
        system("cls");
        printf("\n== 削除メニュー ==\n");
        printf("1:登録受験者について受験者単位の削除\n");
        printf("2:登録受験者について受験者の試験単位の削除\n");
        printf("0:メインメニューに戻る\n");
        printf("番号を入力してください: ");
        if (scanf("%d", &delete_choice) != 1) {
            while (getchar() != '\n'); // 入力バッファクリア
            continue;
        }

        system("cls");

        switch (delete_choice) {

        case 1:
            // 新規受験者情報の登録の関数をここに
            break;
        case 2:
            // 既登録受験者の試験結果登録の関数をここに
            break;
        case 0:
            printf("メインメニューに戻ります");
            return;
        default:
            printf("無効な番号です。\n");
            printf("０から２の数字で入力してください。");
            break;
        }
        printf("\n続けるにはEnterを押してください...");
        while (getchar() != '\n');
        getchar();
    }
}

// 参照機能のメニュー
void show_reference_menu(sqlite3* db) {
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
        printf("10:各科目平均点以下の受験者\n");
        printf("11:全科目平均点以下の受験者\n");
        printf("0:メインメニューに戻る\n");
        printf("番号を入力してください: ");
        if (scanf("%d", &reference_choice) != 1) {
            while (getchar() != '\n'); // 入力バッファクリア
            continue;
        }

        system("cls");

        switch (reference_choice) {

        case 1:

            break;
        case 2:

            break;
        case 3:

            break;
        case 4:

            break;
        case 5:

            break;
        case 6:

            break;
        case 7:

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
            show_examinees_below_total_avg(db);
            break;
        case 0:            
            printf("メインメニューに戻ります");
            return;
        default:
            printf("無効な番号です。\n");
            printf("０から11の数字で入力してください。");
            break;
        }
        printf("\n続けるにはEnterを押してください...");
        while (getchar() != '\n');
        getchar();
    }      
}

int main(void) {
    sqlite3* db;
    int main_choice;

    // データベースを開く
    if (sqlite3_open("test2.db", &db) != SQLITE_OK) {
        printf("データベースを開けません: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    
    // メインメニュー
    while (1) {
        system("cls");
        printf("\n== メインメニュー ==\n");
        printf("1: 登録機能\n");
        printf("2: 更新機能\n");
        printf("3: 削除機能\n");
        printf("4: 参照機能\n");
        printf("0: 終了\n");
        printf("番号を入力してください: ");
        scanf("%d", &main_choice);

        switch (main_choice) {

        case 1:
            system("cls");            
            show_register_menu(db);
            break;
        case 2:
            system("cls");
            // 更新機能の関数をここに
            break;
        case 3:
            system("cls");            
            show_delete_menu(db);
            break;
        case 4:
            system("cls");
            show_reference_menu(db);
            break;
        case 0:
            system("cls");
            printf("終了します。\n");
            sqlite3_close(db);
            return 0;
        default:
            system("cls");
            printf("無効な番号です。\n");
            printf("０から４の数字で入力してください。");
            break;
        }
        printf("\n続けるにはEnterを押してください...");
        while (getchar() != '\n');
        getchar();
    }

    sqlite3_close(db);
    return 0;
}
