#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>   //memmove
#include <wchar.h>
#include "subjectEnum.h"
#include "reffer2db.h"
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
Subject getSubjectFromInput(int subjectId);                                                                     //入力された数値を元に、科目情報を返す関数
Subject SelectSubject(Subject subject, const char* prompt);                                                     //選択科目を入力する関数
bool Choice(const char* msg);                                                                                   //1か2で選ばせるときに使う関数
bool CheckConfirmingExamDate(const int countExamDates,const int inputExamDate,const int* printExamDates);       //ユーザーに入力された試験日が、一致しているかどうか判定する関数
bool CheckFinalizeExamDate(const int inputExamDate,const int* printExamDates);                                  //機能2,4,5,6用　ユーザーが入力した日付の結果を出力するかどうかの意思決定を聞く関数
bool printExamDate(int countExamDates, int* printExamDates);                                                    //機能2,4,5,6試験日一覧を表示する関数
bool isExistData(void);                                                                                         //データベースにデータが格納されているか調べる関数
int IntegerInput(const char* prompt);                                                                           //整数入力を安全に受け取る関数（値が条件に正しいかは関係ない）
int setLocaleUTF_JP(void);                                                                                      //SQL文の文字出力の際、文字ずれが発生しないように、Localeをセットする関数
int selectExamDate(const char* msg);                                                                            //機能2,4,5,6用　ユーザーに試験日を選んでもらう関数
void GetSubjectTopNames(const Subject subject);                                                                 //これでGetSubjectTopNameに飛ぶ
void clear_screen(void);                                                                                            //残っている字を消す
void printNoExistDataInDB(void);                                                                                    //データベースにデータが一件も登録されていない時の処理
void enterToReturnMenu(void);


/*
#ifdef _WIN32
    // Windows環境の場合、コンソールのエンコーディングをUTF-8に設定
    SetConsoleOutputCP(CP_UTF8);
#endif
*/


//機能2(試験実施日毎の全科目トップ5)
int show_date_top_name() {
    setLocaleUTF_JP();

    int decidedExamDate;            //ユーザーから入力された、表示する日付を保持する変数
    bool existData = true;          //データベースにデータが一件でも存在するかを調べる変数
    bool end = true;                //各科目合計トップ10出力後に、続けるかどうかのフラグ(初期はtrue)

    if ((existData = isExistData()) == true) {
        do {
            decidedExamDate = selectExamDate("試験実施日毎の全科目トップ5を表示します");
            if (decidedExamDate == 0) {
                //enterToReturnMenu();
                return 0;       //selectExamDateで「参照機能選択一覧に戻る」が選択された場合、参照機能選択一覧へ戻る
            }

            GetTopScoresByDate(decidedExamDate);

            //表示し終えたら、続けて表示するかどうかを確認
            end = Choice("\n続けて出力しますか？ 1:終了する、2:続行");

        } while (end == false);
    }
    else {
        printNoExistDataInDB();
    }

    //enterToReturnMenu();


    return 0;
}


 //機能4(試験実施日毎の全科目平均点数)
int show_avarage_score_by_exam_date() {

    setLocaleUTF_JP();

    int decidedExamDate;            //ユーザーから入力された、表示する日付を保持する変数
    bool existData = true;          //データベースにデータが一件でも存在するかを調べる変数
    bool end = true;                //各科目合計トップ10出力後に、続けるかどうかのフラグ(初期はtrue)

    if ((existData = isExistData()) == true) {
        do {
            decidedExamDate = selectExamDate("試験実施日毎の全科目平均点数を表示します");
            if (decidedExamDate == 0) {
                //enterToReturnMenu();
                return 0;       //selectExamDateで「参照機能選択一覧に戻る」が選択された場合、参照機能選択一覧へ戻る
            }

            GetAverageScoreByDate(decidedExamDate);

            //表示し終えたら、続けて表示するかどうかを確認
            end = Choice("\n続けて出力しますか？ 1:終了する、2:続行");

        } while (end == false);
    }
    else {
        printNoExistDataInDB();
    }

    //enterToReturnMenu();


    return 0;
}


//機能5(試験実施日毎の各科目平均点数以下の受験者一覧)
int show_names_below_ave_by_subject() {

    int decidedExamDate;            //ユーザーから入力された、表示する日付を保持する変数
    bool end = true;                //各科目合計トップ10出力後に、続けるかどうかのフラグ(初期はtrue)
    bool existData = true;          //データベースにデータが一件でも存在するかを調べる変数
    Subject subject = Unselected;   //教科情報を保持するSubject型の変数

    setLocaleUTF_JP();

    if ((existData = isExistData()) == true) {

        //機能通り平均点数以下の受験者一覧を表示した後、「続ける」を選択した場合、繰り返す
        do {

            decidedExamDate = selectExamDate("試験実施日毎の各科目平均点数以下の受験者一覧を表示します");
            if (decidedExamDate == 0) {
                //enterToReturnMenu();
                return 0;       //selectExamDateで「参照機能選択一覧に戻る」が選択された場合、参照機能選択一覧へ戻る
            }

            //続けて、教科を選択する
            subject = SelectSubject(subject, "平均点数以下の受験者一覧を表示したい科目を選択\n");

            //メニュー画面へ戻る、または指定された試験日かつ該当科目の平均点数以下の受験者一覧を表示
            switch (subject) {
            case Unselected: printf("科目が選ばれていません！"); //安全策の為、一応載せておく
                break;
            case BackSelectMenu: printf("参照機能選択一覧に戻ります\n");
                break;
            default: clear_screen();
                GetNamesUnderAveBySubject(subject, decidedExamDate);    // 入力された日と、教科を引数にして、各科目平均点数以下の受験者一覧を表示
                break;
            }

            //もし、GetNamesUnderAveBySubject()が起動した場合、続けて科目選択をするか確認
            if (subject != BackSelectMenu) {
                end = Choice("\n続けて出力しますか？ 1:終了する、2:続行");
            }

        } while (subject != BackSelectMenu && end == false);
    }
    else {
        printNoExistDataInDB();
    }

    //enterToReturnMenu();

    return 0;

}


//機能6(試験実施日毎の全科目平均点数以下の受験者一覧)
int show_names_below_ave_by_all() {

    int decidedExamDate;            //ユーザーから入力された、表示する日付を保持する変数
    bool end = true;                //各科目合計トップ10出力後に、続けるかどうかのフラグ(初期はtrue)
    bool existData = true;          //データベースにデータが一件でも存在するかを調べる変数  
    Subject subject = Unselected;   //教科情報を保持するSubject型の変数

    setLocaleUTF_JP();

    if ((existData = isExistData()) == true) {

        //機能通り平均点数以下の受験者一覧を表示した後、「続ける」を選択した場合、繰り返す
        do {

            decidedExamDate = selectExamDate("試験実施日毎の全科目平均点数以下の受験者一覧を表示します");
            if (decidedExamDate == 0) {
                //enterToReturnMenu();
                return 0;       //selectExamDateで「参照機能選択一覧に戻る」が選択された場合、参照機能選択一覧へ戻る
            }

            //入力された日を引数にして、全科目平均点数以下の受験者一覧を表示

            GetNamesUnderAveByAll(decidedExamDate);

            //表示し終えたら、続けて表示するかどうかを確認
            end = Choice("\n続けて出力しますか？ 1:終了する、2:続行");

        } while (end == false);
    }
    else {
        printNoExistDataInDB();
    }

    //enterToReturnMenu();

    return 0;

}


//機能7(全試験における各科目合計トップ１０)
int show_subject_top_name() {

    bool existData = true;      //データベースにデータが一件でも存在するかを調べる変数

    setLocaleUTF_JP();

    if ((existData = isExistData()) == true) {

        //subjectEnum.h で定義したsubject型を宣言
        Subject subject = Unselected;
        bool end = true;                         //各科目合計トップ10出力後に、続けるかどうかのフラグ(初期はtrue)

        do {
            clear_screen();

            subject = SelectSubject(subject, "全試験における各科目合計トップ10を出力します\n");

            clear_screen();

            //メニュー画面へ戻る、または該当科目のトップ10を表示
            switch (subject) {
            case Unselected: printf("科目が選ばれていません！"); //安全策の為、一応載せておく
                break;
            case BackSelectMenu: printf("参照機能選択一覧に戻ります\n");
                break;
            default: GetSubjectTopNames(subject);
                break;
            }


            //もし、GetSubjectTopNames()が起動した場合、続けて科目選択をするか確認
            if (subject != BackSelectMenu) {
                end = Choice("\n続けて出力しますか？ 1:終了する、2:続行");
            }

            //BackSelectMenuが選択されておらず、且つCheckRetryで「続けない」を選択した場合、終了
        } while (subject != BackSelectMenu && end == false);
    }
    else {
        printNoExistDataInDB();
    }

    //enterToReturnMenu();
    return 0;
}


//機能8(全試験における全科目合計トップ１０)
int show_total_top_name() {

    bool existData = true;      //データベースにデータが一件でも存在するかを調べる変数

    if ((existData = isExistData()) == true) {

        setLocaleUTF_JP();

        printf("全試験における全科目合計トップ１０を表示します\n\n");

        GetTotalTopName();
    }

    else {
        printNoExistDataInDB();
    }

    //enterToReturnMenu();

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
Subject SelectSubject(Subject subject, const char* prompt){

    int subjectId = 0;                           //subjectIdへ教科情報を入力に使用する変数
    bool returnToMenu = false;                  //参照機能選択一覧に戻るかどうかのフラグ

    do {

        //入力が1～11の整数になるまでループ
        while (1) {
            printf("%s",prompt);
            printf("1:国語 2:数学 3:英語 4:日本史 5:世界史 6:地理 7:物理 8:化学 9:生物 10:各科目 11:参照機能選択一覧に戻る\n\n");

            subjectId = IntegerInput("1～11の整数を入力して下さい:");   //IntegerInput関数を通して、getSubjectに整数を格納

            if (subjectId >= MIN_SUBJECT && subjectId <= MAX_SUBJECT) {
                break;
            }
            else {
                clear_screen();
                printf("無効な入力です。1～11の整数を入力してください\n");
            }
        }

        subject = getSubjectFromInput(subjectId);

        //「参照機能選択一覧に戻る」が選択された場合、ユーザーの意思を確認(ChoiceBack関数へ)
        if (subject == BackSelectMenu) {
            returnToMenu = Choice("参照機能選択一覧へ戻りますか？ 1:戻る、2:続行");
            if (returnToMenu == false) {
                clear_screen();
                printf("参照機能選択一覧へ戻らないが選択されました。\n\n");
            }
        }

    } while (subject == BackSelectMenu && returnToMenu == false);     //BackSelectMenuが選択され、且つ参ユーザーが「参照機能選択一覧に戻らない」を選択した時のみ、ループを続ける

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
            clear_screen();
            printf("無効な入力です。正しい選択は 1 または 2 です\n");
        }
        tmp++;
    }
}


//機能2,4,5,6　ユーザーに入力された文字が、試験日と一致しているか判定
bool CheckConfirmingExamDate(const int countExamDates,const int inputExamDate, const int* printExamDates) {
    bool matchExamDate = false;
    if (inputExamDate > 0 && inputExamDate <= countExamDates) {
        return true;
    }
    //参照機能一覧へ戻るかどうかに対し、参照機能へ戻らないを選択した場合の処理
    else if (inputExamDate==0) {
        clear_screen();
        printf("参照機能一覧へ戻らないが選択されました。\n\n");
        return false;
    }
    else {
        clear_screen();
        printf("「1」から「%d」または「0」を半角整数で入力してください\n\n", countExamDates);
        return false;
    }
}

//機能2,4,5,6 ユーザーが選択した日付で良いか、確認する関数
bool CheckFinalizeExamDate(const int inputExamDate,const int* printExamDates) {
    int examDate = printExamDates[(inputExamDate-1)];
    int year = examDate / 10000;
    int month = (examDate % 10000) / 100;
    int day = examDate % 100;
    clear_screen();
    printf("選択した日程 [%04d-%02d-%02d] でよろしいですか 1:「はい」2:「いいえ」\n",year,month,day);
    bool finalize = Choice("");
    if (finalize == true) {
        return true;
    }
    else {
        clear_screen();
        return false;
    }
}

//機能2,4,5,6用ユーザーに試験日を入力してもらう関数
bool printExamDate(int countExamDates, int* printExamDates) {

    int examDate, year, month, day = 0;

    //試験日が存在していれば
    if (countExamDates > 0) {
        //結果の表示
        printf("%d日の試験日があります\n", countExamDates);
        printf("各試験日の日程\n\n");

        for (int i = 0; i < countExamDates; i++) {
            examDate = printExamDates[i];
            year = examDate / 10000;
            month = (examDate % 10000) / 100;
            day = examDate % 100;
            printf("%d : %04d-%02d-%02d\n", i + 1, year, month, day);
        }
        printf("\n");
        return true;
    }
    //試験日が存在しない場合、「試験日が存在していません」と表示し、falseを返す
    else {
        printf("試験日が存在しません\n");
        printf("試験日とテスト結果を登録してください\n\n");
        //enterToReturnMenu();
    }
    return false;
}

//データベースにデータが登録されているかを、スコアデータを使って調べる関数(受検者データ（score無し）は、「データが入ってないと見なす」)
bool isExistData(void) {
    int existData = 0;
    existData = CheckExistData();
    //データが存在するなら、trueを、しなければfalseを返す
    return (existData == 1) ?  true : false;
}

//データを文字型で受け取り、整数型へ変換して値を返す関数
int IntegerInput(const char* prompt) {
    char buffer[INPUT_BUFFER_SIZE];
    long value;
    char* endptr;

    while (1) {
        printf("%s", prompt);
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            clearerr(stdin);
            printf("入力の読み取りに失敗しました。再度入力してください\n\n");
            continue;
        }

        // 改行がなければ、残りの入力を破棄する
        if (strchr(buffer, '\n') == NULL) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
        }

        errno = 0;
        value = strtol(buffer, &endptr, 10);

        // 数値変換が失敗した場合
        if (endptr == buffer) {
            printf("無効な入力です。半角整数のみを入力してください\n\n");
            continue;
        }

        // 余分な文字のチェック（ホワイトスペースはスキップ）
        while (isspace((unsigned char)*endptr)) {
            endptr++;
        }
        if (*endptr != '\0' && *endptr != '\n') {
            printf("無効な入力です。半角整数のみを入力してください\n\n");
            continue;
        }

        // オーバーフローまたは範囲外のチェック
        if ((errno == ERANGE && (value == LONG_MAX || value == LONG_MIN)) || value > INT_MAX || value < INT_MIN) {
            printf("入力された数値が大きすぎるか小さすぎるため、整数に変換できません\n\n");
            continue;
        }
        return (int)value;
    }
}

int setLocaleUTF_JP() {
    // ロケールを明示的に設定（日本語のUTF-8環境）
    if (setlocale(LC_ALL, "ja_JP.UTF-8") == NULL) {
        fprintf(stderr, "ロケールの設定に失敗しました。\n");
        return 1;
    }
    return 0;
}


//ユーザーに試験日を入力してもらう関数
int selectExamDate(const char* msg) {

    int countExamDates = 0;         //試験日の日数を数える変数
    int inputExamDate = 0;          //ユーザーに日付を入力してもらう変数
    int decidedExamDate = 0;        //機能5.6に確定したint型の試験日を返す変数
    bool matchExamDate = false;     //入力された試験日が存在しているかどうかを確認するフラグ
    bool BackToMenu = false;        //参照選択一覧へ戻るかを確認するフラグ
    bool finalizeExamDate = false;  //試験日を確定するかを確認するフラグ
    bool isExistExamDate = true;    //試験日が存在するかどうかを確認するフラグ

    clear_screen();

    //printExamDateへ、試験日をYYYYMMDDの形且つint型で保存
    int* printExamDates = getExamData(&countExamDates);
    if (printExamDates == NULL) {
        fprintf(stderr, "試験日の取得に失敗しました\n");
        return -1;
    }
    //選んだ試験日をfinalizeExamDateで確認し,OKならループを抜ける
    do {
        //inputExamDateが日付一覧と一致していれば、ループを抜ける（例 1:20250101 2:20250102 で、１か２が入力されている場合）
        do {
            printf("%s\n\n",msg);

            matchExamDate = false;
            //db側で、int型で取得完了
                //試験日一覧を表示
            isExistExamDate = printExamDate(countExamDates, printExamDates);

            //もし試験日が存在しないのであれば、０を返し、プログラムを終了させる
            if (isExistExamDate == false) {
                return 0;
            }

            printf("0 : 参照機能に戻る\n\n");

            printf("試験日を1から%dの半角整数で入力してください(0で参照機能一覧へ戻ります) 例:1\n\n", countExamDates);

            //ユーザーから日付を入力してもらう関数
            inputExamDate = IntegerInput("入力:");

            //0が入力された場合、参照機能一覧へ戻るか聞き、戻るが選択されると、プログラムを終了
            if (inputExamDate == 0) {
                BackToMenu = Choice("参照機能一覧へ戻りますか？ 1:はい　2:いいえ");
                if (BackToMenu == true) {
                    free(printExamDates);
                    return 0;       //もし、参照機能一覧へ戻るなら、0を返す
                }
            }

            //入力された文字が、日付一覧と一致しているかどうか判定（例 1:20250101 2:20250102 で、１か２が入力されている場合）
            matchExamDate = CheckConfirmingExamDate(countExamDates, inputExamDate, printExamDates);
        } while (!matchExamDate);

        //日付が選択された場合、その日付で良いかをユーザーに確認する関数
        finalizeExamDate = CheckFinalizeExamDate(inputExamDate, printExamDates);
    } while (!finalizeExamDate);

    //decidedExamDateに、ユーザーが選んだ日付を格納
    decidedExamDate = printExamDates[(inputExamDate - 1)];

    //printExamDatesを解放し、入力情報のみを管理
    free(printExamDates);

    clear_screen();

    return decidedExamDate;
}

//Subject型を値渡しする
void GetSubjectTopNames(const Subject subject) {
    ShowSubjectTopNames(subject);
}

//画面をクリアする関数
void clear_screen() {
    // ANSIエスケープシーケンスで画面をクリア＆カーソルを先頭へ移動
    printf("\033[2J\033[H");
    fflush(stdout);
}

//データベースにデータが一件も登録されていなかった時の処理
void printNoExistDataInDB() {
    printf("データが登録されていません\n");
    printf("データを登録してください\n\n"); 
    //reffer2のメインプログラムの方でenterToReturnMenuを呼び出し
}

void enterToReturnMenu() {
    int ch;

    printf("参照機能一覧へ戻ります。Enterを押してください...");
    fflush(stdout);  // プロンプトを即座に表示

    /* getchar()がEOFまたは改行を読み込むまでループ */
    while ((ch = getchar()) != '\n') {
        if (ch == EOF) {
            // 入力エラーまたはストリームの終了の場合はループを抜ける
            break;
        }
        /* 余計な入力はそのまま読み捨てる */
    }
}