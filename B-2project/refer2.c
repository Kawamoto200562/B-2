/*
やりたいこと
文字の削除
void clear_screen() {
    // ANSIエスケープシーケンスで画面をクリア＆カーソルを先頭へ移動
    printf("\033[2J\033[H");
    fflush(stdout);
}

後は何処に組み込むか、fflushの後に、「エンターを押してください」って表示するのがいいかも！
細かいコメント？
選択式で選べるようになったらいいよね　➡１
確認画面もあった方が良いかも（表示しますか？）

Getsubject
全体の流れの理解
とくに文字列のとことか
関数のプロトタイプ宣言
関数化出来ない？
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>   //memmove
#include <wchar.h>
#include "subjectEnum.h"
#include "GetSubjectTopNames.h"
#include "refer2.h"

/*
#ifdef _WIN32
#include <windows.h> 
#endif
*/

#define MIN_SUBJECT 1
#define MAX_SUBJECT 11
#define INPUT_BUFFER_SIZE 128


//関数プロトタイプの宣言
Subject getSubjectFromInput(int subjectId);    //入力された数値を元に、科目情報を返す関数
Subject SelectSubject(Subject subject);        //選択科目を入力する関数
bool Choice(const char* msg);                   //1か2で選ばせるときに使う関数
bool CheckConfirmingExamDate(const int countExamDates,const char* inputExamDate,ExamData* printExamDates);       //ユーザーに入力された試験日が、一致しているかどうか判定する関数
int IntegerInput(const char* prompt);           //整数入力を安全に受け取る関数（値が条件に正しいかは関係ない）
int setLocaleUTF_JP();                          //SQL文の文字出力の際、文字ずれが発生しないように、Localeをセットする関数
void GetSubjectTopNames(const Subject subject); //これでGetSubjectTopNameに飛ぶ
void clear_screen();                            //残っている字を消す
void trim(char* str);                           //機能5,6　入力文字の前後空白を削除する関数(strcomに使う）
void printExamDate(int countExamDates, ExamData* printExamDates);    //機能5,6試験日一覧を表示する関数
void SelectExamDateByUser(char* inputExamDate);

/*
#ifdef _WIN32
    // Windows環境の場合、コンソールのエンコーディングをUTF-8に設定
    SetConsoleOutputCP(CP_UTF8);
#endif
*/


//機能5(試験実施日毎の各科目平均点数以下の受験者一覧)
int show_names_below_ave_by_subject() {

    setLocaleUTF_JP();

    int countExamDates = 0;         //試験日を数える変数
    char inputExamDate[128];        //ユーザーが日付を入力された関数
    int decidedExamDate;            //ユーザーから入力された、表示する日付を保持する変数
    int year, month, day;           //ユーザーから入力された「YYYY-MM-DD」型の文字列をint型へ変換する為の変数
    bool matchExamDate=false;       //入力された日付が、一致しているかどうかのフラグ
    bool end = true;                //各科目合計トップ10出力後に、続けるかどうかのフラグ(初期はtrue)
    Subject subject = Unselected;   //教科情報を保持するSubject型の変数


    //機能通り平均点数以下の受験者一覧を表示した後、「続ける」を選択した場合、繰り返す
    do {
        printf("試験実施日毎の各科目平均点数以下の受験者一覧を表示します\n\n");


        //ExamData型へ、試験日をYYYY-MM-DD型で保存
        ExamData* printExamDates = getExamData(&countExamDates);
        if (printExamDates == NULL) {
            fprintf(stderr, "試験日の取得に失敗しました\n");
            return 1;
        }
        do {
            matchExamDate = false;

            //試験日一覧を表示
            printExamDate(countExamDates, printExamDates);


            //ユーザーから日付を入力してもらう関数
            SelectExamDateByUser(inputExamDate);

            // 改行を除去
            inputExamDate[strcspn(inputExamDate, "\n")] = '\0';

            //文字の前後の空白を処理する関数
            trim(inputExamDate);

            //入力された文字が、一致しているか判定
            matchExamDate = CheckConfirmingExamDate(countExamDates, inputExamDate, printExamDates);
        } while (matchExamDate == false);

        //一致した場合、printExamDatesを解放し、入力情報のみを管理
        free(printExamDates);


        //inputExamDateを,「2025-01-01」形式から、「20250101」形式へ変換、decidedExamDayへ代入する処理
        if (sscanf_s(inputExamDate, "%4d-%2d-%2d", &year, &month, &day) != 3) {
            fprintf(stderr, "日付の形式が正しくありません\n");
            return 1;
        }

        decidedExamDate = year * 10000 + month * 100 + day;

        //-------ここまでがユーザーに試験日を選択してもらう処理--------


        //続けて、教科を選択する
        printf("平均点数以下の受験者一覧を表示したい科目を選択\n");
        subject = SelectSubject(subject);
        int tmp = subject;


        //メニュー画面へ戻る、または指定された試験日かつ該当科目の平均点数以下の受験者一覧を表示
        switch (subject) {
        case Unselected: printf("科目が選ばれていません！"); //安全策の為、一応載せておく
            break;
        case BackSelectMenu: printf("参照機能選択一覧に戻ります\n");
            break;
        default: GetNamesUnderAveBySubject(subject, decidedExamDate);    // 入力された日と、教科を引数にして、各科目平均点数以下の受験者一覧を表示
            break;
        }
        
        //もし、GetNamesUnderAveBySubject()が起動した場合、続けて科目選択をするか確認
        if (subject != BackSelectMenu) {
            end = Choice("続けて出力しますか？ 1:終了する、2:続行");
        }

    } while (subject != BackSelectMenu && end == false);

    return 0;
}


//機能6(試験実施日毎の全科目平均点数以下の受験者一覧)
int show_names_below_ave_by_all() {

    setLocaleUTF_JP();

    int countExamDates = 0;     //試験日を数える変数
    char inputExamDate[128];    //ユーザーが日付を入力された関数
    int decidedExamDate;         //ユーザーから入力された、表示する日付を保持する変数
    int year, month, day;   //ユーザーから入力された「YYYY-MM-DD」型の文字列をint型へ変換する為の変数
    bool matchExamDate = false;    //入力された日付が、一致しているかどうかのフラグ
    bool end = true;                         //各科目合計トップ10出力後に、続けるかどうかのフラグ(初期はtrue)


    printf("試験実施日毎の全科目平均点数以下の受験者一覧を表示します\n\n");


    //ExamData型へ、試験日をYYYY-MM-DD型で保存
    ExamData* printExamDates = getExamData(&countExamDates);
    if (printExamDates == NULL) {
            fprintf(stderr, "試験日の取得に失敗しました\n");
            return 1;
        }
        do {
            matchExamDate = false;

            //試験日一覧を表示
            printExamDate(countExamDates, printExamDates);


            //ユーザーから日付を入力してもらう関数
            SelectExamDateByUser(inputExamDate);

            // 改行を除去
            inputExamDate[strcspn(inputExamDate, "\n")] = '\0';

            //文字の前後の空白を処理する関数
            trim(inputExamDate);

            //入力された文字が、一致しているか判定
            matchExamDate = CheckConfirmingExamDate(countExamDates, inputExamDate, printExamDates);
        } while (matchExamDate == false);

        //一致した場合、printExamDatesを解放し、入力情報のみを管理
        free(printExamDates);


        //inputExamDateを,「2025-01-01」形式から、「20250101」形式へ変換、decidedExamDayへ代入する処理
        if (sscanf_s(inputExamDate, "%4d-%2d-%2d", &year, &month, &day) != 3) {
            fprintf(stderr, "日付の形式が正しくありません\n");
            return 1;
        }

        decidedExamDate = year * 10000 + month * 100 + day;

    /*
    * 入力された日を引数にして、全科目平均点数以下の受験者一覧を表示
    * 受験者ごとに、選択した科目の合計平均点を算出する方が良いらしいソース（質問スレッド）
    */

    getchar();

    return 0;
}

//機能7(全試験における各科目合計トップ１０)
int show_subject_top_name() {

    setLocaleUTF_JP();

    //subjectEnum.h で定義したsubject型を宣言
    Subject subject = Unselected;
    bool end = true;                         //各科目合計トップ10出力後に、続けるかどうかのフラグ(初期はtrue)

    do {
        printf("全試験における各科目合計トップ10を出力します\n");
        subject=SelectSubject(subject);

        //メニュー画面へ戻る、または該当科目のトップ10を表示
        switch (subject) {
        case Unselected: printf("科目が選ばれていません！"); //安全策の為、一応載せておく
            break;
        case BackSelectMenu: printf("参照機能選択一覧に戻ります\n");
            break;
        default: GetSubjectTopNames(subject);
            break;
        }

        getchar();


        //もし、GetSubjectTopNames()が起動した場合、続けて科目選択をするか確認
        if (subject != BackSelectMenu ) {
            end = Choice("続けて出力しますか？ 1:終了する、2:続行");
        }

        //BackSelectMenuが選択されておらず、且つCheckRetryで「続けない」を選択した場合、終了
    } while (subject != BackSelectMenu && end == false);

    return 0;
}



//機能8(全試験における全科目合計トップ１０)
int show_total_top_name() {

    setLocaleUTF_JP();

    printf("全試験における全科目合計トップ１０を表示します\n\n");

GetTotalTopName();

printf("\n\n参照機能選択一覧へ戻ります\n");

getchar();

return 0;
}




//getSubjectを元に、subjectへ科目情報を代入する関数
Subject getSubjectFromInput(int getSubject) {
    switch (getSubject) {
    case 1: return Japanese;
    case 2: return Mathematics;
    case 3: return English;
    case 4: return JapaneseHistory;
    case 5: return WorldHistory;

    case 6: return Geography;
    case 7: return Physics;
    case 8: return Chemistry;
    case 9: return Biology;
    case 10:return All;
    case 11:return BackSelectMenu;
    default:return Unselected;          //万が一の安全策
    }
}

//どの科目を選択するか選ぶ関数(機能5,7)
Subject SelectSubject(Subject subject){

    int subjectId = 0;                           //subjectIdへ教科情報を入力に使用する変数
    bool returnToMenu = false;                  ////参照機能選択一覧に戻るかどうかのフラグ

    do {

        //入力が1～11の整数になるまでループ
        while (1) {
            printf("1:国語 2:数学 3:英語 4:日本史 5:世界史 6:地理 7:物理 8:化学 9:生物 10:各科目 11:参照機能選択一覧に戻る\n\n");

            subjectId = IntegerInput("1～11の整数を入力して下さい:");   //IntegerInput関数を通して、getSubjectに整数を格納

            if (subjectId >= MIN_SUBJECT && subjectId <= MAX_SUBJECT) {
                break;
            }
            else {
                printf("無効な入力です。1～11の整数を入力してください\n");
            }
        }

        subject = getSubjectFromInput(subjectId);

        //「参照機能選択一覧に戻る」が選択された場合、ユーザーの意思を確認(ChoiceBack関数へ)
        if (subject == BackSelectMenu) {
            returnToMenu = Choice("参照機能選択一覧へ戻りますか？ 1:戻る、2:続行");
        }

    } while (subject == BackSelectMenu && returnToMenu == false);     //BackSelectMenuが選択され、且つ参照機能選択一覧に戻らないが選択された場合

    return subject;
}


//今回は、returnToMenuとendの判定にこの関数を使用　因みに、引数が*型なのは、先頭文字列のポインタを渡しているから（単一だと、一文字しか送れないらしい）
bool Choice(const char* msg) {
    int tmp = 0;
    while (1) {
        printf("%s\n", msg);
        tmp = IntegerInput("1または2を入力してください:");

        if (tmp == 1) {
            return true;
        }
        else if (tmp == 2) {
            return false;
        }
        else {
            printf("無効な入力です。1または2を入力してください\n");
        }
    }
}

//機能5,6　ユーザーに入力された文字が、試験日と一致しているか判定
bool CheckConfirmingExamDate(const int countExamDates, const char* inputExamDate, ExamData* printExamDates) {
    bool matchExamDate = false;
        for (int i = 0; i < countExamDates; i++) {
            if (strcmp(inputExamDate, printExamDates[i].exam_day) == 0) {
                matchExamDate = true;
                break;
            }
        }

        if (matchExamDate == true) {
            //画面クリアするならココ
            printf("%sの各科目平均点数以下の受験者一覧を表示します\n", inputExamDate);
            return true;
        }
        else {
            printf("入力エラーです。もう一度入力してください\n");
            return false;
        }
}


//データを文字型で受け取り、整数型へ変換して値を返す関数（エラー表示が２回出るバグあり,エラーという表示も冗長？　後で直す）機能7,8用
int IntegerInput(const char* prompt) {
    char buffer[128];
    int input;
    char extra; //数字以外の余分な文字を受取る変数

    //入力が正しく整数として読み取られるまでループ
    while (1) {
        printf("%s", prompt);
        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {     //ユーザーに入力してもらう処理

            //入力された文字データを整数データへ変換、変換できない場合はエラー処理。また、(unsigned int)sizeof(extra)で、「3d」のような入力をエラーに出来るよう処理
            if (sscanf_s(buffer, "%d %c", &input, &extra, (unsigned int)sizeof(extra)) == 1) {
                return input;
            }
        }
        printf("エラー!%s", prompt);
    }
}

int setLocaleUTF_JP() {
    // ロケールを明示的に設定（日本語のUTF-8環境）
    if (setlocale(LC_ALL, "ja_JP.UTF-8") == NULL) {
        fprintf(stderr, "ロケールの設定に失敗しました。\n");
        return 1;
    }
}

//Subject型を値渡しする
void GetSubjectTopNames(const Subject subject) {
    ShowSubjectTopNames(subject);
}

//画面をクリアする関数
void clear_screen() {
    // ANSIエスケープシーケンスで画面をクリア＆カーソルを先頭へ移動
    printf("clear関数起動！\n");
    printf("\033[2J\033[H");
    fflush(stdout);
}

//機能5,機能6専用(入力された文字の調整（前後の空白処理）)
void trim(char* str) {
    char* start = str;  //引数の文字列の先頭を表すポインタ
    char* end;          //文字列の末尾の位置

    //先頭の空白文字をスキップ
    while (*start && isspace((unsigned char) * start)){ 
        start++;    //文字列の最初から調べ、空白文字である限り、startポインタを進める
    }

    //文字列全体が空白の場合の処理
    if (*start == '\0') {
        str[0] = '\0';
        return;
    }

    //末尾の空白文字を見つける
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }

    //新しい終端位置の設定
    *(end + 1) = '\0';

    //先頭の空白を除いた部分を元の文字列領域にコピー
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

void printExamDate(int countExamDates, ExamData* printExamDates) {
    //結果の表示
    printf("%d日の試験日があります\n", countExamDates);
        printf("各試験日の日程\n");

        for (int i = 0; i < countExamDates; i++) {
            printf("%s\n", printExamDates[i].exam_day);
        }
}

void SelectExamDateByUser(char* inputExamDate){
    //ユーザーから日付を入力してもらい、それが一致するかどうかを判定する(全角２０２５－０１－０１は未対応、余力があればやる)
    printf("受験者一覧を表示する試験実施日を指定してください(例: 2025-01-01 )\n\n");
    if (fgets(inputExamDate, INPUT_BUFFER_SIZE, stdin) == NULL) {
        fprintf(stderr, "入力エラーです。\n");
    }
}