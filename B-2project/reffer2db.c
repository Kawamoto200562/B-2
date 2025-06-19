#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <ctype.h>
#include <stdbool.h>
#include "subjectEnum.h"
#include "reffer2db.h"
#include "wcwidth.h" 	//wcwidth()用


//定数でフィールド幅を決める(データベースから持ってきた受験者名、ふりがなの文字数)
#define NAME_FIELD_WIDTH		30		//名前の長さ
#define FURIGANA_FIELD_WIDTH	30		//ふりがなの長さ
#define ELECTIVE_FIELD_WIDTH	6		//選択科目の科目名の文字サイズ
#define PAGE_SIZE				1		//１ページに表示するレコード数
#define NoExistData				-1		//データが存在しない時
#define defSub					1		//get_user_command関数で使用する引数(機能7 Subject=ALLの時)
#define defNum					2		//get_user_command関数で使用する引数(機能6、機能7 Subject!=ALLの時)
#define defSubAndNum			3		//get_user_command関数で使用する引数(機能5 Subject=ALLの時)
#define MaxSubject				9		//最大教科数
#define MinSubject				1		//最低教科数
#define INPUT_BUFFER_SIZE		128		//バッファサイズ

//Subject型の各値に対する表示名の配列
const char* g_subjectNames[] = {
	"未選択",		//0
	"国語",			//1
	"数学",			//2
	"英語",			//3
	"日本史",		//4
	"世界史",		//5
	"地理",			//6
	"物理",			//7
	"化学",			//8
	"生物",			//9
	"各科目",		//10
	"戻る",			//11
};


int* getExamData(int* countExamDate);																//機能5,6 データベースに保存されている日付の種類をカウントする変数
sqlite3* openDatabase(const char* filename);														//データベース接続を行い、ハンドルを返す
int get_display_width(const wchar_t* ws);															//各文字の表示幅を計算する関数(全角なら通常2,半角なら1)
int getTotalCountUnderAveByAll(sqlite3* db, const int decidedExamDate);								//機能5,最大ページ表示用関数
int getUnderAveBySubjectCount(sqlite3* db, const int decidedExamDate, const Subject subject);		//機能6,最大ページ表示用関数
int prepareSQL(sqlite3* db, const char* sql, sqlite3_stmt** stmt);									//stmtにsql文をコンパイルしたものを格納し、実行に備える
int bindExamParameters(sqlite3_stmt* stmt, const int decidedExamDate, const int page , Subject subject, sqlite3* db);		//機能5 決定された試験日と科目をSQL文内の？へバインド
int bindSingleSubject(sqlite3_stmt* stmt, Subject subject, sqlite3* db);							//機能7 単一科目用SQL文のプレースホルダにSubject型の値をバインドする
int getAverageScoreBySubject(sqlite3* db, int decidedExamDate, Subject subject);					//機能５科目毎の平均点を取得する関数
void print_aligned(const wchar_t* ws, int field_width);												//指定したフィールド幅になるように左寄席で出力する関数 出力後に足りない部分の空白を出力する
void prepareTopNames(void);																			//機能7,8の、最初の表示文
void displayTopNamesByDate(sqlite3_stmt* stmt, const int decidedExamDate);							//機能2表示文
void displayFunq4(sqlite3_stmt* stmt,const int decidedExamDate);									//機能4表示文
int displayNamesUnderAve(sqlite3_stmt* stmt, int decidedExamDate, const int page, const int maxPage, const char* subjectName, const int avgScore); //機能5表示関数
int displayNamesUnderAveByAll(sqlite3_stmt* stmt,const int page,const int maxPage, const int decidedExamDate);			//機能6　表示関数
void printTitleSubject(Subject subject);															//機能7 「[科目名]の上位10位以内のテスト結果」を順に表示する関数
int displayTopScores(sqlite3_stmt* stmt);															//機能7,8 sqlite3_step()で結果を取得して標準出力に表示する
void displayTotalTopScores(sqlite3_stmt* stmt);														//機能7,8 合計得点の結果を表示する関数（受験者名・ふりがなはそれぞれ30文字固定）
void printNoExistData(void);																		//全機能、取得データが存在しない時の処理
const char* CountDates(void);																		//機能5,6用、試験日の日数をカウントするSQL文を返す
const char* LoadExamDates(void);																	//機能5,6用、試験日を表示する関数
const char* SQLTopNamesByDate(void);																	//機能2 SQL文
const char* funq4Sql(void);
const char* GetUnderAveBySubject(void);																//機能5 SQL文
const char* GetUnderAveBySubjectCount(void);
const char* GetUnderAveByAll(void);																	//機能6 SQL文
const char* GetUnderAveByAllCount(void);
const char* buildSingleSubjectSQL(void);															//機能7 SQL文 単一科目用のSQL文を返す(ALLの場合は、これを各教科ごとに繰り返す)
const char* SumAllSubjectScore(void);																//機能8 SQL文
char get_user_command(const int def,const int currentpage, const int maxPage, const int funq5SubNum);
char* trim(char* str );



//データベースにデータが１件以上登録されているか確認する変数
int CheckExistData(void) {
	sqlite3* db;
	int rc = 0;
	int exist = 0;
	sqlite3_stmt* stmt = NULL;

	db = openDatabase("test.db");
	if (db == NULL) {
		rc = 1;
		goto cleanup;
	}
	
	const char* sql = "SELECT EXISTS(SELECT 1 FROM score LIMIT 1)";
	rc = prepareSQL(db, sql, &stmt);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL準備に失敗しました: %s\n", sqlite3_errmsg(db));
		goto cleanup;
	}

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		exist = sqlite3_column_int(stmt, 0);
	}

cleanup:
	if (stmt) {
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if (db) {
		sqlite3_close(db);
		db = NULL;
	}

	return exist;

}


//機能2
int GetTopScoresByDate(const int decidedExamDate) {
	sqlite3* db = NULL;
	sqlite3_stmt* stmt = NULL;
	int rc = 0;

	db = openDatabase("test.db");
	if (db == NULL) {
		return 1;
	}

	const char* sql = SQLTopNamesByDate();

	rc = prepareSQL(db, sql, &stmt);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL準備に失敗しました: %s\n", sqlite3_errmsg(db));
		goto cleanup;
	}

	rc = sqlite3_bind_int(stmt, 1, decidedExamDate);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "日付のバインドに失敗しました: %s\n", sqlite3_errmsg(db));
		goto cleanup;
	}

	//準備に成功していれば、結果を表示する
	displayTopNamesByDate(stmt, decidedExamDate);

cleanup:
	if (stmt) {
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if (db) {
		sqlite3_close(db);
		db = NULL;
	}
	return rc;
}


//機能4
int GetAverageScoreByDate(const int decidedExamDate) {
	sqlite3* db = NULL;
	sqlite3_stmt* stmt = NULL;
	int rc = 0;

	db = openDatabase("test.db");
	if (db == NULL) {
		return 1;
	}

	const char* sql = funq4Sql();

	rc = prepareSQL(db, sql, &stmt);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL準備に失敗しました: %s\n", sqlite3_errmsg(db));
		goto cleanup;
	}

	rc = sqlite3_bind_int(stmt, 1, decidedExamDate);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "日付のバインドに失敗しました: %s\n", sqlite3_errmsg(db));
		goto cleanup;
	}

	//準備に成功していれば、結果を表示する
	displayFunq4(stmt,decidedExamDate);



cleanup:
	if (stmt) {
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if (db) {
		sqlite3_close(db);
		db = NULL;
	}
	return rc;
}

//機能5
int GetNamesUnderAveBySubject(const Subject subject, const int decidedExamDate) {
	sqlite3* db = NULL;
	sqlite3_stmt* stmt = NULL;
	int rc = 0;
	int page = 0;
	int found = 0;
	char command;
	int totalCount = 0;
	int maxPage = 0;
	int dummy = 0;	//All以外の時、get_user_commandに渡す第四引数
	int avgScore = 0;
	const char* sql = GetUnderAveBySubject();

	db = openDatabase("test.db");
	if (db == NULL) {
		rc = 1;
		goto cleanup;
	}

	if (subject == BackSelectMenu) {
		rc = 0;
		goto cleanup;
	}

	// subjectがAllの場合、教科別にページ切替を実装する
	if (subject == All) {
		// 外側ループ：件数取得と教科切替
		int currentSubject = Japanese;
		while (1) {
			// ① 現在の教科に合わせて総件数と最大ページ数を再計算
			totalCount = getUnderAveBySubjectCount(db, decidedExamDate, (Subject)currentSubject);
			if (totalCount < 0) {
				fprintf(stderr, "教科 %s の総件数取得に失敗しました。\n", g_subjectNames[currentSubject]);
				// エラー時は次の教科へスキップする（または適宜処理）
			}
			maxPage = (totalCount + PAGE_SIZE - 1) / PAGE_SIZE;
			if (maxPage == 0) {
				maxPage = 1;
			}

			// ② 教科が切り替わったら、ページ番号はリセット
			page = 0;

			// ③ 内側ループ：現在の教科内でページ切替処理を行う
			while (1) {
				rc = prepareSQL(db, sql, &stmt);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "SQL文の準備に失敗しました: %s\n", sqlite3_errmsg(db));
					goto cleanup;
				}
				rc = bindExamParameters(stmt, decidedExamDate, page, (Subject)currentSubject, db);
				if (rc != SQLITE_OK) {
					sqlite3_finalize(stmt);
					// エラー発生時は内側ループ中断して次の教科に移行（または適宜処理）
					break;
				}
				avgScore = getAverageScoreBySubject(db, decidedExamDate, (Subject)currentSubject);

				printf("\n%d/%d教科\n", currentSubject, MaxSubject);

				found = displayNamesUnderAve(stmt, decidedExamDate, page, maxPage, g_subjectNames[currentSubject], avgScore);
				sqlite3_finalize(stmt);
				stmt = NULL;

				//  現在の教科内のページ操作のコマンドを取得
				//'1':次ページ, '2':前ページ, '3':次の教科, '4':前の教科, '0':終了 を返す
				command = get_user_command(defSubAndNum,page,maxPage,currentSubject);
				if (command == '1') {
					if (page + 1 < maxPage) {
						page++;
						clear_screen();
						continue;
					}
					else {
						clear_screen();
						printf("既に最終ページです\n");
						continue;
					}
				}
				else if (command == '2') {
					if (page > 0) {
						page--;
						clear_screen();
						continue;
					}
					else {
						clear_screen();
						printf("これが先頭のページです\n");
						continue;
					}
				}
				else if (command == '3') {
					clear_screen();
					// 次の教科へ切り替え
					currentSubject++;
					if (currentSubject > Biology) { // 最後の教科の場合
						printf("これ以上、次の科目はありません\n");
						currentSubject = Biology;
					}
					break;  // 内側ループ終了→外側ループで次教科に
				}
				else if (command == '4') {
					clear_screen();
					// 前の教科へ切り替え
					currentSubject--;
					if (currentSubject < Japanese) { // 最初の教科の場合
						printf("これ以上、前の科目はありません\n");
						currentSubject = Japanese;
					}
					break;  // 内側ループ終了し、外側ループで前教科に切替
				}
				else if (command == '0') {
					goto cleanup;
				}
			} // end inner while (ページ切替)

			// 外側ループの終了条件（例：全教科を回り終えた場合）をここで検討する
			// たとえば、currentSubject > Biology なら全体終了するか、またはループを継続するか
			if (currentSubject > Biology) {
				break; // 全教科の表示を終了
			}
		} // end outer while (教科切替)
	}
	else {
		// subjectが単一の場合の従来のページ切替処理
		totalCount = getUnderAveBySubjectCount(db, decidedExamDate, subject);
		if (totalCount < 0) {
			fprintf(stderr, "総件数の取得に失敗しました。\n");
			rc = 1;
			goto cleanup;
		}
		maxPage = (totalCount + PAGE_SIZE - 1) / PAGE_SIZE;
		if (maxPage == 0) {
			maxPage = 1;
		}
		do {
			rc = prepareSQL(db, sql, &stmt);
			if (rc != SQLITE_OK) {
				fprintf(stderr, "SQL文の準備に失敗しました: %s\n", sqlite3_errmsg(db));
				goto cleanup;
			}
			rc = bindExamParameters(stmt, decidedExamDate, page, subject, db);
			if (rc != SQLITE_OK) {
				goto cleanup;
			}
			avgScore = getAverageScoreBySubject(db, decidedExamDate, subject);
			found = displayNamesUnderAve(stmt, decidedExamDate, page, maxPage, g_subjectNames[subject], avgScore);
			if (found == 0) {
				if (page > 0) {
					printf("これ以上、該当の受験者はいません\n");
					getchar();
					page--;
				}
				else {
					printNoExistData();
					break;
				}
			}

			//'1'で次ページ、'2'で前ページ、'0'で終了
			command = get_user_command(defNum,page,maxPage,dummy);
			if (command == '1') {
				if (page + 1 < maxPage) {
					page++;
					clear_screen();
				}
				else {
					clear_screen();
					printf("既に最終ページです\n");
				}
				sqlite3_finalize(stmt);
				stmt = NULL;
				continue;
			}
			else if (command == '2') {
				if (page > 0) {
					page--;
					clear_screen();
				}
				else {
					clear_screen();
					printf("これが先頭のページです\n\n");
				}
				sqlite3_finalize(stmt);
				stmt = NULL;
				continue;
			}
			else if (command == '0') {
				printf("データの閲覧を終了します");
				getchar();
				break;
			}
		} while (1);
	}

cleanup:
	if (stmt) {
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if (db) {
		sqlite3_close(db);
		db = NULL;
	}
	return rc;
}



//PAGE_SIZE毎にページ分け
//機能6
int GetNamesUnderAveByAll(const int decidedExamDate) {

	sqlite3* db;
	sqlite3_stmt* stmt = NULL;
	int rc = 0;
	int page = 0;	//現在のページ番号
	int maxPage=0;
	int dummy = 0;	//All以外の時、get_user_commandに渡す第四引数
	int totalCount = 0;
	int found = 0;		//データが存在するか確認する変数
	char command;

	db = openDatabase("test.db");
	if (db == NULL) {
		rc = 1;
		goto cleanup;
	}

	totalCount = getTotalCountUnderAveByAll(db,decidedExamDate);
	if (totalCount < 0) {
		fprintf(stderr, "総件数の取得に失敗しました。\n");
		rc = 1;
		goto cleanup;
	}
	maxPage = (totalCount + PAGE_SIZE - 1) / PAGE_SIZE;
	if (maxPage == 0) {
		maxPage = 1;
	}

	//SQL文
	const char* sql = GetUnderAveByAll();

	while (1) {
		rc = prepareSQL(db, sql, &stmt);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "SQL準備に失敗しました: %s\n", sqlite3_errmsg(db));
			goto cleanup;
		}

		//decidedExamDateをバインド
		rc = sqlite3_bind_int(stmt, 1, decidedExamDate);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "試験日パラメータのバインドに失敗しました %s\n", sqlite3_errmsg(db));
			goto cleanup;
		}
		rc = sqlite3_bind_int(stmt, 2, PAGE_SIZE);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "ページ件数のバインドに失敗しました %s\n", sqlite3_errmsg(db));
			goto cleanup;
		}
		rc = sqlite3_bind_int(stmt, 3, page * PAGE_SIZE);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "オフセットパラメータのバインドに失敗しました %s\n", sqlite3_errmsg(db));
			goto cleanup;
		}


		//準備に成功していれば、結果を表示する
		found=displayNamesUnderAveByAll(stmt, page,maxPage,decidedExamDate);

		if (found==0) {
			if (page > 0) {	//データが一つ以上あり、且つページ異動により、表示するデータがない場合
				printf("これ以上、該当の受験者はいません\n");
				getchar();
				page--;	//前ページに戻す
			}
			else {	//データベースにデータが一件も登録されていなかった場合は、この機能の場合、ないので、削除しても良い？
				printNoExistData();
				break;
			}
		}

		//'1'で次ページ、'2'で前ページ、'0'で終了
		command = get_user_command(defNum,page,maxPage,dummy);

		if (command == '1') {
			if (page + 1 < maxPage) {
				page++;
				clear_screen();
			}
			else {
				clear_screen();
				printf("既に最終ページです\n");
			}
			sqlite3_finalize(stmt);
			stmt = NULL;
			continue;
		}
		else if (command == '2') {
			if (page > 0) {
				page--;
				clear_screen();
			}
			else {
				clear_screen();
				printf("これが先頭のページです\n");
			}
			sqlite3_finalize(stmt);
			stmt = NULL;
			continue;
		}
		else if (command == '0') {
			printf("データの閲覧を終了します");
			getchar();
			break;
		}
	}
cleanup:
	if (stmt) {
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if (db) {
		sqlite3_close(db);
		db = NULL;
	}
	return rc;


}


//科目毎にページ分け（トップ10なので、PAGESIZEは関係なし）
//Allを選んだ際、「次のページへ」を「次の科目へ」に修正する
//機能７
//mainからSubject型を受取り、科目ごとに表示する
int ShowSubjectTopNames(const Subject subject) {

		sqlite3* db;
		sqlite3_stmt* stmt = NULL;
		int rc = 0;
		int page = 0;	//現在のページ番号(「subject」が「All」の時用)
		int found = 0;		//データが存在するか確認する変数
		int dummy = 0;	//All以外の時、get_user_commandに渡す第四引数
		char command;	//ページ操作の入力を受け付ける変数(「subject」が「All」の時用)
		int maxPage = MaxSubject;


		db = openDatabase("test.db");
		if (db == NULL) {
			rc = 1;
			goto cleanup;
		}

		//まずないと思うが、一応、念の為にBackSelectMenuが渡されていた時の処理
		if (subject == BackSelectMenu) {
			rc = 0;
			goto cleanup;
		}

		const char* sql = buildSingleSubjectSQL();
		printf("\n");		//原因不明の改行が起きる為、その調整

//ALLの場合、forループでJapanese～Biology(1～9)の各科目を処理
	if (subject == All) {

		while (1) {
			if (page < Japanese) {
				page = Japanese;
			}

			Subject sub = (Subject)page;


			rc = prepareSQL(db, sql, &stmt);
			if (rc != SQLITE_OK) {
				fprintf(stderr, "Error preparing SQL for subject[%s](iteration %d)\n", g_subjectNames[sub], page);
			}

			rc = bindSingleSubject(stmt, sub, db);
			if (rc != SQLITE_OK) {
				fprintf("Error binding parameter for subject [%s] (iteration %d)\n", g_subjectNames[sub], page);
			}

			printf("%d/%dページ\n\n", page, maxPage);

			printTitleSubject(sub);

			prepareTopNames();
			found = displayTopScores(stmt);

			//ここでユーザーにページ移動処理
			if (found == 0) {
				if (page > 0) {
					printf("受検者が存在しません\n");
				}
				else {
					printNoExistData();
					break;
				}
			}

			command = get_user_command(defSub,page,maxPage,dummy);


			//'1'で次科目、'2'で前科目、'0'で終了
			if (command == '1') {
				clear_screen();
				if (page < Biology) {
					page++;
				}else {
					printf("これ以上、次の教科はありません\n");
				}
				sqlite3_finalize(stmt);
				stmt = NULL;
				printf("\n");
				continue;
			}
			else if (command == '2') {
				clear_screen();
				if (page > Japanese) {
					page--;
					sqlite3_finalize(stmt);
					stmt = NULL;
					printf("\n");
					continue;
				}
				else {
					printf("これ以上、前の教科はありません\n");
					sqlite3_finalize(stmt);
					stmt = NULL;
					printf("\n");
					continue;
				}
			}
			else if (command == '0') {
				printf("データの閲覧を終了します");
				getchar();
				break;
			}

			sqlite3_finalize(stmt);
			stmt = NULL;
			printf("\n");
		}
	}
	else if (subject >= Japanese && subject < All) {
		//単一科目の場合

		rc = prepareSQL(db, sql, &stmt);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "Error preparing SQL for subject [%s]\n", g_subjectNames[subject]);
			goto cleanup;
		}
		rc = bindSingleSubject(stmt, subject, db);	
		if (rc != SQLITE_OK) {
			fprintf(stderr, "Error binding parameter for subject [%s]\n", g_subjectNames[subject]);
			goto cleanup;
		}
		printTitleSubject(subject);
		prepareTopNames();
		found=displayTopScores(stmt);
		if (found == 0) {
			printNoExistData();
		}
	}
	else {
		fprintf(stderr, "不正な科目オプションです\n");	//万が一の為のエラー処理
		rc = 1;
		goto cleanup;
	}

cleanup:
	if (stmt){
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if (db) {
		sqlite3_close(db);
		db = NULL;
	}
	return rc;
}

//機能８
int GetTotalTopName() {
	sqlite3* db=NULL;
	sqlite3_stmt* stmt = NULL;
	int rc = 0;

	db = openDatabase("test.db");
	if (db == NULL) {
		return 1;
	}

	const char* sql = SumAllSubjectScore();
	rc = prepareSQL(db, sql, &stmt);
	if (rc != SQLITE_OK) {
		fprintf(stderr,"SQL準備に失敗しました: %s\n", sqlite3_errmsg(db));
		goto cleanup;
	}

	//準備に成功していれば、結果を表示する
	prepareTopNames();

	displayTotalTopScores(stmt);

cleanup:
	if (stmt) {
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if (db) {
		sqlite3_close(db);
		db = NULL;
	}
	return rc;
}

//機能5,6 データベースに保存されている日付の種類をカウントする変数
int* getExamData(int* countExamDates) {
	sqlite3* db = NULL;
	sqlite3_stmt* stmt = NULL;
	int rc = 0;
	int* data = NULL;

	db = openDatabase("test.db");
	if (db == NULL) {
		*countExamDates = 0;
		goto cleanup;
	}

	// ① 件数を取得する
	const char* sql = CountDates();
	rc = prepareSQL(db, sql, &stmt);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL準備に失敗しました: %s\n", sqlite3_errmsg(db));
		*countExamDates = 0;
		goto cleanup;
	}

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		*countExamDates = sqlite3_column_int(stmt, 0);
	}
	else {
		*countExamDates = 0;
	}
	sqlite3_finalize(stmt);

	// ② 件数に合わせたサイズの配列を確保
	data = malloc((*countExamDates) * sizeof(int));
	if (data == NULL) {
		fprintf(stderr, "メモリ確保エラー\n");
		*countExamDates = 0;
		goto cleanup;
	}

	// ③ 日付一覧を取得し、整数値から文字列 ("YYYY-MM-DD") に変換して格納
	const char* sql_Load = LoadExamDates();
	rc = prepareSQL(db, sql_Load, &stmt);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "ステート作成エラー: %s\n", sqlite3_errmsg(db));
		free(data);
		data = NULL;    // 明示的に NULL にする
		*countExamDates = 0;
		goto cleanup;
	}

	int index = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW && index < *countExamDates) {
		data[index] = sqlite3_column_int(stmt, 0);
		index++;
	}

cleanup:
	if (stmt) {
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if (db) {
		sqlite3_close(db);
		db = NULL;
	}
	return data;
}



//データベース接続を行い、ハンドルを返す
sqlite3* openDatabase(const char* filename) {
	sqlite3* db;
	int rc = sqlite3_open(filename, &db);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "データベースを開けません:%s\n", sqlite3_errmsg(db));
		return NULL;
	}
	return db;
}

//各文字の表示幅を計算する関数(全角なら通常2,半角なら1)
int get_display_width(const wchar_t* ws) {
	int width = 0;
	while (*ws) {
		int char_width = wcwidth(*ws);
		if (char_width < 0) {
			char_width = 0;
		}
		width += char_width;
		ws++;
	}
	return width;
}

//機能6 マックスページ表示用関数
int getTotalCountUnderAveByAll(sqlite3* db, const int decidedExamDate) {
	int totalCount = 0;
	const char* countSql = GetUnderAveByAllCount();
	sqlite3_stmt* stmt = NULL;

	int rc = prepareSQL(db, countSql, &stmt);
	if (rc != SQLITE_OK) {
		return -1;
	}
	rc = sqlite3_bind_int(stmt, 1, decidedExamDate);
	if (rc != SQLITE_OK) {
		sqlite3_finalize(stmt);
		return -1;
	}
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		totalCount = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return totalCount;
}

//機能５マックスページ表示用関数
int getUnderAveBySubjectCount(sqlite3* db, const int decidedExamDate, const Subject subject) {
	int totalCount = 0;
	const char* countSql = GetUnderAveBySubjectCount();
	sqlite3_stmt* stmt = NULL;

	int rc = prepareSQL(db, countSql, &stmt);
	if (rc != SQLITE_OK) {
		return -1;
	}
	rc = sqlite3_bind_int(stmt, 1, decidedExamDate);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "パラメータ１のバインドに失敗: %s\n", sqlite3_errmsg(db));
		return rc;
	}
	rc = sqlite3_bind_int(stmt, 2, (int)subject);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "パラメータ2のバインドに失敗: %s\n", sqlite3_errmsg(db));
		return rc;
	}
	rc = sqlite3_bind_int(stmt, 3, decidedExamDate);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "パラメータ3のバインドに失敗: %s\n", sqlite3_errmsg(db));
		return rc;
	}
	rc = sqlite3_bind_int(stmt, 4, (int)subject);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "パラメータ4のバインドに失敗: %s\n", sqlite3_errmsg(db));
		return rc;
	}
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		totalCount = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return totalCount;
}


//stmtにsql文をコンパイルしたものを格納し、実行に備える
int prepareSQL(sqlite3* db, const char* sql, sqlite3_stmt** stmt) {
	int rc = sqlite3_prepare_v2(db, sql, -1, stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "クエリ準備エラー: %s\n", sqlite3_errmsg(db));
	}
	return rc;
}

//機能5 決定された試験日と科目をSQL文内の？へバインド
int bindExamParameters(sqlite3_stmt* stmt,const int decidedExamDate, const int page, Subject subject, sqlite3* db) {
	int rc;
	rc = sqlite3_bind_int(stmt, 1, decidedExamDate);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "パラメータ１のバインドに失敗: %s\n", sqlite3_errmsg(db));
		return rc;
	}
	rc = sqlite3_bind_int(stmt, 2, (int)subject);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "パラメータ2のバインドに失敗: %s\n", sqlite3_errmsg(db));
		return rc;
	}
	rc = sqlite3_bind_int(stmt, 3, decidedExamDate);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "パラメータ3のバインドに失敗: %s\n", sqlite3_errmsg(db));
		return rc;
	}
	rc = sqlite3_bind_int(stmt, 4, (int)subject);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "パラメータ4のバインドに失敗: %s\n", sqlite3_errmsg(db));
		return rc;
	}
	rc = sqlite3_bind_int(stmt, 5, PAGE_SIZE);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "ページ件数のバインドに失敗しました %s\n", sqlite3_errmsg(db));
		return rc;
	}
	rc = sqlite3_bind_int(stmt, 6, page * PAGE_SIZE);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "オフセットパラメータのバインドに失敗しました %s\n", sqlite3_errmsg(db));
		return rc;
	}
	return rc;
}

//機能7 単一科目用SQL文のプレースホルダにSubject型の値をバインドする
int bindSingleSubject(sqlite3_stmt* stmt, Subject subject, sqlite3* db) {

	int rc = sqlite3_bind_int(stmt, 1, subject);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "バインドエラー(単一科目): %s\n", sqlite3_errmsg(db));
	}
	return rc;
	
}


 
// 指定された試験日(decidedExamDate)と科目(subject)に対する平均得点（scoreのAVG）をDBから取得し返す。

int getAverageScoreBySubject(sqlite3* db, int decidedExamDate, Subject subject) {
	int avgInt=0;
	sqlite3_stmt* stmt_avg = NULL;
	const char* sqlAvg = "SELECT AVG(s.score) "
		"FROM score s "
		"JOIN exam ex ON s.exam_id = ex.exam_id "
		"WHERE ex.exam_date = ? AND subject_id = ?;";
	double avg = 0.0;
	int rc = sqlite3_prepare_v2(db, sqlAvg, -1, &stmt_avg, NULL);
	if (rc == SQLITE_OK) {
		rc = sqlite3_bind_int(stmt_avg, 1, decidedExamDate);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "平均点 1番目パラメータバインド失敗: %s\n", sqlite3_errmsg(db));
		}
		rc = sqlite3_bind_int(stmt_avg, 2, (int)subject);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "平均点 2番目パラメータバインド失敗: %s\n", sqlite3_errmsg(db));
		}
		if (sqlite3_step(stmt_avg) == SQLITE_ROW) {
			avg = sqlite3_column_double(stmt_avg, 0);
		}
	}
	else {
		fprintf(stderr, "平均点取得SQL準備失敗: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt_avg);
	avgInt = avg;

	return avgInt;
}

//指定したフィールド幅になるように左寄席で出力する関数
//出力後に足りない部分の空白を出力する
void print_aligned(const wchar_t* ws, int field_width) {
	int str_width = get_display_width_main(ws);
	wchar_t buffer[256] = { 0 };
	int printed_width = 0; // バッファにコピーしたときの表示幅
	int pos = 0;
	const wchar_t* p = ws;

	// 表示幅がフィールド幅を超える場合、切り詰める
	if (str_width > field_width) {
		while (*p) {
			int cw = wcwidth(*p);
			if (cw < 0)
				cw = 0;
			if (printed_width + cw > field_width - 1) {  // 末尾に「…」を入れる場合は -1
				break;
			}
			buffer[pos++] = *p;
			printed_width += cw;
			p++;
		}
		// 末尾に省略記号を付加（フィールド幅が十分な場合）
		if (field_width >= 2) {
			buffer[pos++] = L'…';
			printed_width += 1; // 省略記号の幅として1と仮定
		}
	}
	else {
		// 表示幅がフィールド幅に収まる場合、内容をそのままコピーする
		wcscpy_s(buffer, 256, ws);
		pos = (int)wcslen(buffer);  // ここでコピーした文字列の長さを pos に反映する
		printed_width = str_width;
	}

	// buffer[pos] にヌル文字を設定
	buffer[pos] = L'\0';

	// 出力
	wprintf(L"%ls", buffer);
	// 不足分を空白で埋める
	for (int i = 0; i < field_width - printed_width; i++) {
		wprintf(L" ");
	}
}

//機能7,8の最初に表示
void prepareTopNames(void) {
	printf("--------------------------------------------------------------------------------------------\n");
	printf("|順位| 氏名		             | ふりがな		             | 得点 |   試験日   |\n");
	printf("--------------------------------------------------------------------------------------------\n");
}

//機能２表示文
void displayTopNamesByDate(sqlite3_stmt* stmt, const int decidedExamDate) {
	int found = 0;

	char examDateStr[16];
	int year = decidedExamDate / 10000;
	int month = (decidedExamDate % 10000) / 100;
	int day = decidedExamDate % 100;
	snprintf(examDateStr, sizeof(examDateStr), "%04d-%02d-%02d", year, month, day);

	printf("%sの全科目トップ5\n\n",examDateStr);

	printf("--------------------------------------------------------------------------------\n");
	printf("|順位| 氏名			      | ふりがな		       | 得点  |\n");
	printf("--------------------------------------------------------------------------------\n");


	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int rank = sqlite3_column_int(stmt, 0);
		const unsigned char* mb_examinee_name = sqlite3_column_text(stmt, 1);
		const unsigned char* mb_furigana = sqlite3_column_text(stmt, 2);
		int total_score = sqlite3_column_int(stmt, 3);

		// マルチバイト文字列をワイド文字列に変換するためのバッファ
		wchar_t w_examinee_name[256] = { 0 };
		wchar_t w_furigana[256] = { 0 };
		size_t conv_examinee = 0, conv_furigana = 0;

		// examinee_name の変換
		errno_t err = mbstowcs_s(&conv_examinee, w_examinee_name, 256, (const char*)mb_examinee_name, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting examinee_name\n");
		}
		// furigana の変換
		err = mbstowcs_s(&conv_furigana, w_furigana, 256, (const char*)mb_furigana, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting furigana\n");
		}

		// 出力：順位、受験者名、ふりがな、合計得点
		// 受験者名・ふりがなはそれぞれ固定長（30文字）で表示するため、print_aligned() を利用
		wprintf(L"%2d位 | ", rank);
		print_aligned(w_examinee_name, NAME_FIELD_WIDTH);  // NAME_FIELD_WIDTH は30に定義
		printf(" | ");
		print_aligned(w_furigana, FURIGANA_FIELD_WIDTH);     // FURIGANA_FIELD_WIDTH は30に定義
		wprintf(L" | %3d点 |\n", total_score);					//合計得点

		found++;
	}
	if (found == 0) {
		printNoExistData();
	}

}


//機能4　表示文
void displayFunq4(sqlite3_stmt* stmt, const int decidedExamDate) {

	char examDateStr[16];
	int year = decidedExamDate / 10000;
	int month = (decidedExamDate % 10000) / 100;
	int day = decidedExamDate % 100;
	snprintf(examDateStr, sizeof(examDateStr), "%04d-%02d-%02d", year, month, day);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		int totalScore = sqlite3_column_int(stmt, 0);
		printf("\n%sの全科目の平均点数は、 %d 点です\n", examDateStr,totalScore);
	}
	else {
		printf("平均点数の表示に失敗しました\n");
	}

}



/**
 * displayExamResults
 *
 * バインド済みの SQLite のステートメントから結果行を取得し表示する。
 * ヘッダには試験日 (decidedExamDate) を "YYYY-MM-DD" 形式に変換して出力するとともに、
 * 渡された科目名(subjectName)と、その試験日の平均点(avgScore)を出力する。
 *
 * @param stmt              結果がセットされた SQLite ステートメント
 * @param decidedExamDate   試験日 (整数形式)
 * @param subjectName       表示用科目名の文字列（例: g_subjectNames[sub]）
 * @param avgScore          試験日・科目に対して取得した平均点（double）
 */
//機能5表示関数
int displayNamesUnderAve(sqlite3_stmt* stmt, int decidedExamDate, const int page, const int maxPage, const char* subjectName,const int avgScore) {
	int found = 0;		//該当科目に受験者データが存在するか確認する変数
	int rc = 0;
	char examDateStr[16];
	int year = decidedExamDate / 10000;
	int month = (decidedExamDate % 10000) / 100;
	int day = decidedExamDate % 100;
	snprintf(examDateStr, sizeof(examDateStr), "%04d-%02d-%02d", year, month, day);

	printf("\n%d/%dページ目\n\n", page+1,maxPage);

	printf("\n--- 試験日 %s の [%s] の平均: %d点 以下の受験者一覧 ---\n\n",
		examDateStr, subjectName, avgScore);

	printf("--------------------------------------------------------------------------\n");
	printf("| 氏名			         | ふりがな			 | 得点 |\n");
	printf("--------------------------------------------------------------------------\n");

	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		const unsigned char* mb_examinee_name = sqlite3_column_text(stmt, 0);
		const unsigned char* mb_furigana = sqlite3_column_text(stmt, 1);
		int score = sqlite3_column_int(stmt, 2);

		//名前とふりがなをワイド文字列に変換
		wchar_t w_examinee_name[256] = { 0 };
		wchar_t w_furigana[256] = { 0 };
		size_t conv_examinee = 0;
		size_t conv_furigana = 0;

		errno_t err = mbstowcs_s(&conv_examinee, w_examinee_name, 256, (const char*)mb_examinee_name, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting examinee_name\n");
		}

		err = mbstowcs_s(&conv_furigana, w_furigana, 256, (const char*)mb_furigana, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting furigana\n");
		}

		printf("| ");
		print_aligned(w_examinee_name,NAME_FIELD_WIDTH);
		printf(" | ");
		print_aligned(w_furigana,FURIGANA_FIELD_WIDTH);
		printf("| %3d  |\n", score);
		found++;
	}
	return found;
}

//機能６表示用
int displayNamesUnderAveByAll(sqlite3_stmt* stmt,const int page,const int maxPage, const int decidedExamDate) {

	int found = 0;		//該当科目に受験者データが存在するか確認する変数
	char examDateStr[16];
	int year = decidedExamDate / 10000;
	int month = (decidedExamDate % 10000) / 100;
	int day = decidedExamDate % 100;
	snprintf(examDateStr, sizeof(examDateStr), "%04d-%02d-%02d", year, month, day);


	printf("\n%d/%dページ目\n\n",page+1,maxPage);

	printf("--- 試験日 %s の全科目平均点数以下の受験者一覧 ---\n\n",examDateStr);

	printf("------------------------------------------------------------------------------------------------------------------\n");
	printf("| 氏名			         | ふりがな			  | 選択科目	    | 個人平均点 | 全体平均点 |\n");
	printf("------------------------------------------------------------------------------------------------------------------\n");

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		// 各列のテキスト取得
		const unsigned char* mb_examinee_name = sqlite3_column_text(stmt, 0);
		const unsigned char* mb_furigana = sqlite3_column_text(stmt, 1);
		const unsigned char* mb_electiveSub1 = sqlite3_column_text(stmt, 2);
		const unsigned char* mb_electiveSub2 = sqlite3_column_text(stmt, 3);
		int total_score = sqlite3_column_int(stmt, 4);
		int ave_total_score = sqlite3_column_int(stmt, 5);

		// マルチバイト文字列からワイド文字列へ変換するためのバッファ
		wchar_t w_examinee_name[256] = { 0 };
		wchar_t w_furigana[256] = { 0 };
		wchar_t w_electiveSub1[256] = { 0 };
		wchar_t w_electiveSub2[256] = { 0 };
		size_t conv_examinee = 0;
		size_t conv_furigana = 0;
		size_t conv_electiveSub1 = 0;
		size_t conv_electiveSub2 = 0;

		errno_t err = mbstowcs_s(&conv_examinee, w_examinee_name, 256, (const char*)mb_examinee_name, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting examinee_name\n");
		}
		err = mbstowcs_s(&conv_furigana, w_furigana, 256, (const char*)mb_furigana, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting furigana\n");
		}
		err = mbstowcs_s(&conv_electiveSub1, w_electiveSub1, 256, (const char*)mb_electiveSub1, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting electiveSub1\n");
		}
		err = mbstowcs_s(&conv_electiveSub2, w_electiveSub2, 256, (const char*)mb_electiveSub2, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting electiveSub2\n");
		}

		// 各列の出力（print_aligned 関数で整列表示）
		printf("| ");
		print_aligned(w_examinee_name, NAME_FIELD_WIDTH);
		printf(" | ");
		print_aligned(w_furigana, FURIGANA_FIELD_WIDTH);
		printf(" | ");
		print_aligned(w_electiveSub1, ELECTIVE_FIELD_WIDTH);
		wprintf(L" + ");
		print_aligned(w_electiveSub2, ELECTIVE_FIELD_WIDTH);
		printf(" | ");
		wprintf(L"%8d点 | %8d点 |\n", total_score, ave_total_score);
		found++;
	}
		return found;
}



//機能7 「[科目名]の上位10位以内のテスト結果」を順に表示する関数
void printTitleSubject(Subject subject) {
	if (subject >= Japanese && subject <= Biology) {
		//個別科目の場合
		printf("[%s]の上位10位以内のテスト結果:\n", g_subjectNames[subject]);
	}
	else if (subject == All) {
		printf("[各科目]の上位10位以内のテスト結果\n");
	}
}

//sqlite3_step()で結果を取得して標準出力に表示する（機能７表示関数）
int displayTopScores(sqlite3_stmt* stmt) {

	int found = 0;		//該当科目に受験者データが存在するか確認する変数

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int rank = sqlite3_column_int(stmt, 0);
		const unsigned char* mb_examinee_name = sqlite3_column_text(stmt, 1);
		const unsigned char* mb_furigana = sqlite3_column_text(stmt, 2);
		int score = sqlite3_column_int(stmt, 3);
		int int_exam_date = sqlite3_column_int(stmt, 4); // 数値型の日付

		// 日付をフォーマット (char型)
		char formatted_exam_date[11] = { 0 };
		snprintf(formatted_exam_date, sizeof(formatted_exam_date), "%04d-%02d-%02d",
			int_exam_date / 10000,         // 年
			(int_exam_date / 100) % 100,  // 月
			int_exam_date % 100);         // 日

		// formatted_exam_dateをワイド文字列に変換
		wchar_t w_formatted_exam_date[11] = { 0 };
		size_t conv_date = 0;
		errno_t err = mbstowcs_s(&conv_date, w_formatted_exam_date, sizeof(w_formatted_exam_date) / sizeof(wchar_t),
			formatted_exam_date, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting exam_date\n");
		}

		// 名前とふりがなをワイド文字列に変換
		wchar_t w_examinee_name[256] = { 0 };
		wchar_t w_furigana[256] = { 0 };
		size_t conv_examinee = 0;
		size_t conv_furigana = 0;

		err = mbstowcs_s(&conv_examinee, w_examinee_name, 256, (const char*)mb_examinee_name, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting examinee_name\n");
		}
		err = mbstowcs_s(&conv_furigana, w_furigana, 256, (const char*)mb_furigana, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting furigana\n");
		}

		// 表示:【rank位 名前 ふりがな 点数 日付】
		wprintf(L"%2d位 | ", rank);
		print_aligned(w_examinee_name, NAME_FIELD_WIDTH); // 名前を整列して表示
		printf("| ");
		print_aligned(w_furigana, FURIGANA_FIELD_WIDTH);  // ふりがなを整列して表示
		printf("| ");
		wprintf(L"%3d点| ", score);							//得点
		wprintf(L"%ls |\n", w_formatted_exam_date);          // ワイド文字列の日付を表示
		found++;
	}
	return found;
}

// 機能8合計得点の結果を表示する関数（受験者名・ふりがなはそれぞれ30文字固定）機能８表示関数
void displayTotalTopScores(sqlite3_stmt* stmt) {

	int found = 0;		//該当科目に受験者データが存在するか確認する変数

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		// SQL文のカラム順に沿って値を取得
		int rank = sqlite3_column_int(stmt, 0);
		const unsigned char* mb_examinee_name = sqlite3_column_text(stmt, 1);
		const unsigned char* mb_furigana = sqlite3_column_text(stmt, 2);
		int total_score = sqlite3_column_int(stmt, 3);
		int int_exam_date = sqlite3_column_int(stmt, 4); // 数値型の日付

		// 日付をフォーマット (char型)
		char formatted_exam_date[11] = { 0 };
		snprintf(formatted_exam_date, sizeof(formatted_exam_date), "%04d-%02d-%02d",
			int_exam_date / 10000,         // 年
			(int_exam_date / 100) % 100,  // 月
			int_exam_date % 100);         // 日

		// formatted_exam_dateをワイド文字列に変換
		wchar_t w_formatted_exam_date[11] = { 0 };
		size_t conv_date = 0;
		errno_t err = mbstowcs_s(&conv_date, w_formatted_exam_date, sizeof(w_formatted_exam_date) / sizeof(wchar_t),
			formatted_exam_date, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting exam_date\n");
		}

		// マルチバイト文字列をワイド文字列に変換するためのバッファ
		wchar_t w_examinee_name[256] = { 0 };
		wchar_t w_furigana[256] = { 0 };
		size_t conv_examinee = 0, conv_furigana = 0;

		// examinee_name の変換
		err = mbstowcs_s(&conv_examinee, w_examinee_name, 256, (const char*)mb_examinee_name, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting examinee_name\n");
		}
		// furigana の変換
		err = mbstowcs_s(&conv_furigana, w_furigana, 256, (const char*)mb_furigana, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting furigana\n");
		}

		// 出力：順位、受験者名、ふりがな、合計得点
		// 受験者名・ふりがなはそれぞれ固定長（30文字）で表示するため、print_aligned() を利用
		wprintf(L"%2d位 | ", rank);
		print_aligned(w_examinee_name, NAME_FIELD_WIDTH);  // NAME_FIELD_WIDTH は30に定義
		printf("| ");
		print_aligned(w_furigana, FURIGANA_FIELD_WIDTH);     // FURIGANA_FIELD_WIDTH は30に定義
		printf("| ");
		wprintf(L"%3d点| ", total_score);					//合計得点
		wprintf(L"%ls |\n", w_formatted_exam_date);          // ワイド文字列の日付を表示
		found++;
	} 
	printf("\n");
	if (found == 0) {
		printNoExistData();
	}
}

//データ取得時、データが存在しなかったときの処理
void printNoExistData(void) {
	printf("該当受験者はいません\n");
}


//機能5,6用、試験日の日数をカウントするSQL文を返す
const char* CountDates() {
	return
		"SELECT Count(DISTINCT exam_date) FROM exam;";
}

//機能5,6用、試験日を表示する関数
const char* LoadExamDates() {
	return
		"SELECT DISTINCT exam_date FROM exam;";
}


//ユーザー入力を文字で受取り、入力された文字に応じてページ移動を行う関数
char get_user_command(const int def, const int currentPage, const int maxPage, const int funq5SubNum) {
	char input[INPUT_BUFFER_SIZE];
	char command;
	char* trimmedInput;

	while (1) {
		// メニュー表示（常に「終了」オプション）
		printf("\n0: 終了");

		if (def == defNum) {
			if (currentPage + 1 < maxPage) {
				printf(" 1: 次のページ");
			}
			if (currentPage > 0) {
				printf(" 2: 前のページ");
			}
		}

		// defSub の場合：科目を切り替える選択肢
		if (def == defSub) {
			if (currentPage < maxPage) {
				printf(" 1: 次の科目");
			}
			if (currentPage > 1) {
				printf(" 2: 前の科目");
			}
		}

		// defSubAndNum の場合：ページと教科の両方の切替えが可能
		if (def == defSubAndNum) {
			if (currentPage + 1 < maxPage) {
				printf(" 1: 次のページ");
			}
			if (currentPage > 0) {
				printf(" 2: 前のページ");
			}
			if (funq5SubNum < MaxSubject) {
				printf(" 3: 次の教科");
			}
			if (funq5SubNum > MinSubject) {
				printf(" 4: 前の教科");
			}
		}

		printf("\n\n入力: ");
		if (fgets(input, sizeof(input), stdin) == NULL) {
			fprintf(stderr, "入力エラーが発生しました、再度入力してください\n");
			continue;
		}

		// 入力バッファに改行が含まれているかチェック
		if (strchr(input, '\n') == NULL) {
			int ch;
			while ((ch = getchar()) != '\n' && ch != EOF) {}
		}
		// 改行文字を除去
		input[strcspn(input, "\n")] = '\0';

		// 前後の空白を除去する（trim関数実装済みとして）
		trimmedInput = trim(input);
		if (trimmedInput == NULL || trimmedInput[0] == '\0') {
			printf("入力されていません。再度入力してください\n");
			continue;
		}

		command = trimmedInput[0];

		// def に応じたコマンドの検証
		switch (def) {
		case defSubAndNum:
			if (command == '0' || command == '1' || command == '2' ||
				command == '3' || command == '4') {
				return command;
			}
			break;
		case defNum:
		case defSub:
		default:
			if (command == '0' || command == '1' || command == '2') {
				return command;
			}
			break;
		}
		printf("不正な入力です。正しいコマンド (");
		if (def == defSubAndNum) {
			printf("0～4");
		}
		else {
			printf("0～2");
		}
		printf(") を半角整数で入力してください。\n");
	}
}

char* trim(char* str) {
	char* start, * end;

	if (str == NULL) {
		return NULL;
	}

	//先頭の空白をスキップ
	start = str;
	while ((*start != '\0') && isspace((unsigned char)*start)) {
		start++;
	}

	//全部が空白の場合は、空文字を返す
	if (*start == '\0') {
		*start = '\0';
		return start;
	}

	//末尾の空白の位置を求める
	end = start + strlen(start) - 1;
	while (end > start && isspace((unsigned char)*end)) {
		end--;
	}
	//終了位置の次にヌル文字をセット
	*(end + 1) = '\0';

	//入力バッファの先頭に結果をコピー
	//*ここではstartを返すだけでよいので、元のポインタはfree()しない

	return start;
}

//機能2　SQL文
const char* SQLTopNamesByDate(void) {
	return 
	"WITH ScoreCalcs AS ("
		"  SELECT "
		"    e.examinee_id, "
		"    e.examinee_name, "
		"    e.furigana, "
		"    comp.exam_date, "
		"    (COALESCE(comp.japanese, 0) + "
		"     COALESCE(comp.mathematics, 0) + "
		"     COALESCE(comp.english, 0) + "
		"     COALESCE(eleA.max_score, 0) + "
		"     COALESCE(eleB.max_score, 0)) AS total_score "
		"  FROM examinee e "
		"  JOIN ("
		"    SELECT "
		"      s.examinee_id, "
		"      ex.exam_date, "
		"      MAX(CASE WHEN Subject_id = 1 THEN score ELSE 0 END) AS japanese, "
		"      MAX(CASE WHEN Subject_id = 2 THEN score ELSE 0 END) AS mathematics, "
		"      MAX(CASE WHEN Subject_id = 3 THEN score ELSE 0 END) AS english "
		"    FROM score s "
		"    JOIN exam ex ON s.exam_id = ex.exam_id "
		"    GROUP BY s.examinee_id, ex.exam_date "
		"  ) comp ON e.examinee_id = comp.examinee_id "
		"  LEFT JOIN ("
		"    SELECT "
		"      s.examinee_id, "
		"      ex.exam_date, "
		"      MAX(s.score) AS max_score "
		"    FROM score s "
		"    JOIN exam ex ON s.exam_id = ex.exam_id "
		"    WHERE Subject_id IN (4, 5, 6) "
		"    GROUP BY s.examinee_id, ex.exam_date "
		"  ) eleA ON e.examinee_id = eleA.examinee_id AND comp.exam_date = eleA.exam_date "
		"  LEFT JOIN ("
		"    SELECT "
		"      s.examinee_id, "
		"      ex.exam_date, "
		"      MAX(s.score) AS max_score "
		"    FROM score s "
		"    JOIN exam ex ON s.exam_id = ex.exam_id "
		"    WHERE Subject_id IN (7, 8, 9) "
		"    GROUP BY s.examinee_id, ex.exam_date "
		"  ) eleB ON e.examinee_id = eleB.examinee_id AND comp.exam_date = eleB.exam_date "
		"), "
		"Ranked AS ("
		"  SELECT "
		"    exam_date, "
		"    examinee_name, "
		"    furigana, "
		"    total_score, "
		"    CAST(DENSE_RANK() OVER ("
		"      PARTITION BY exam_date "
		"      ORDER BY total_score DESC, furigana ASC "
		"    ) AS INTEGER) AS rnk "
		"  FROM ScoreCalcs "
		"  WHERE exam_date = ? "
		") "
		"SELECT "
		"  rnk, "
		"  examinee_name, "
		"  furigana, "
		"  total_score "
		"FROM Ranked "
		"WHERE rnk <= 5 "
		"ORDER BY total_score DESC;";
}

//機能4　SQL文
const char* funq4Sql(void) {
	return 
		"WITH ExamAverage AS ("
		"  SELECT "
		"    e.exam_id, "
		"    ("
		"      MAX(CASE WHEN s.Subject_id = 1 THEN s.score ELSE 0 END) + "   /* 国語 */
		"      MAX(CASE WHEN s.Subject_id = 2 THEN s.score ELSE 0 END) + "   /* 数学 */
		"      MAX(CASE WHEN s.Subject_id = 3 THEN s.score ELSE 0 END) + "   /* 英語 */
		"      MAX(CASE WHEN s.Subject_id IN (4, 5, 6) THEN s.score ELSE 0 END) + "  /* 選択科目１ */
		"      MAX(CASE WHEN s.Subject_id IN (7, 8, 9) THEN s.score ELSE 0 END) "   /* 選択科目２ */
		"    ) / 5.0 AS exam_avg "
		"  FROM exam e "
		"  JOIN score s ON e.exam_id = s.exam_id "
		"  WHERE e.exam_date = ? "       /* ユーザー入力の試験日 */
		"  GROUP BY e.exam_id "
		") "
		"SELECT AVG(exam_avg) AS full_subject_average "
		"FROM ExamAverage;";
}

//機能5 SQL文
const char* GetUnderAveBySubject(void) {
	return
		" SELECT e.examinee_name, e.furigana, s.score "
		" FROM score s "
		" JOIN examinee e ON s.examinee_id = e.examinee_id "
		" JOIN exam ex ON s.exam_id = ex.exam_id "
		" WHERE ex.exam_date = ? AND s.subject_id = ? "
		" AND s.score < ( "
		"	SELECT AVG(s.score) "
		" FROM score s "
		" JOIN exam ex ON s.exam_id = ex.exam_id "
		" WHERE ex.exam_date = ? AND s.subject_id = ? "
		" ) "
		" ORDER BY s.score DESC, e.furigana ASC "
		" LIMIT ? OFFSET ?; ";
}

//機能5 カウント用SQL文
const char* GetUnderAveBySubjectCount() {
	return
		" SELECT COUNT(examinee_name) "
		" FROM score s "
		" JOIN examinee e ON s.examinee_id = e.examinee_id "
		" JOIN exam ex ON s.exam_id = ex.exam_id "
		" WHERE ex.exam_date = ? AND s.subject_id = ? "
		" AND s.score < ( "
		"	SELECT AVG(s.score) "
		" FROM score s "
		" JOIN exam ex ON s.exam_id = ex.exam_id "
		" WHERE ex.exam_date = ? AND s.subject_id = ? "
		" ); ";
}


//機能6 SQL文
const char* GetUnderAveByAll(void) {
	return
		"WITH ExamScores AS ( "
		"  SELECT "
		"    e.examinee_id, "
		"    e.examinee_name, "
		"    e.furigana, "
		"    ex.exam_date, "
		"    MAX(CASE WHEN s.Subject_id = 1 THEN s.score ELSE 0 END) AS score1, "
		"    MAX(CASE WHEN s.Subject_id = 2 THEN s.score ELSE 0 END) AS score2, "
		"    MAX(CASE WHEN s.Subject_id = 3 THEN s.score ELSE 0 END) AS score3, "
		"    MAX(CASE WHEN s.Subject_id IN (4, 5, 6) THEN s.score ELSE 0 END) AS scoreA, "
		"    MAX(CASE WHEN s.Subject_id IN (7, 8, 9) THEN s.score ELSE 0 END) AS scoreB, "
		"    MAX(CASE WHEN s.Subject_id IN (4, 5, 6) THEN s.Subject_id ELSE 0 END) AS elective1, "
		"    MAX(CASE WHEN s.Subject_id IN (7, 8, 9) THEN s.Subject_id ELSE 0 END) AS elective2 "
		"  FROM exam ex "
		"  JOIN score s ON ex.exam_id = s.exam_id "
		"  JOIN examinee e ON e.examinee_id = ex.examinee_id "
		"  WHERE ex.exam_date = ? "
		"  GROUP BY e.examinee_id, ex.exam_date, e.examinee_name, e.furigana "
		"), "
		"ExamTotals AS ( "
		"  SELECT *, (score1 + score2 + score3 + scoreA + scoreB) AS total_score "
		"  FROM ExamScores "
		"), "
		"ElectiveGroupAverage AS ( "
		"  SELECT exam_date, elective1, elective2, AVG(total_score) AS avg_total_score "
		"  FROM ExamTotals "
		"  GROUP BY exam_date, elective1, elective2 "
		") "
		"SELECT "
		"  et.examinee_name, "
		"  et.furigana, "
		"  subjA.subject_name AS \"選択科目1\", "
		"  subjB.subject_name AS \"選択科目2\", "
		"  (et.total_score / 5.0) AS \"受験者の平均点\", "       /* 受験者の合計点を5で割って平均点に変更 */
		"  (ega.avg_total_score / 5.0) AS \"全科目平均点数\" "   /* 全体グループの平均も同様に算出 */
		"FROM ExamTotals et "
		"JOIN ElectiveGroupAverage ega ON et.exam_date = ega.exam_date "
		"    AND et.elective1 = ega.elective1 "
		"    AND et.elective2 = ega.elective2 "
		"LEFT JOIN Subjects subjA ON et.elective1 = subjA.subject_id "
		"LEFT JOIN Subjects subjB ON et.elective2 = subjB.subject_id "
		"WHERE et.total_score < ega.avg_total_score "
		"ORDER BY et.total_score ASC, et.furigana ASC "
		" LIMIT ? OFFSET ?; ";
}

//機能6 カウント用SQL文
const char* GetUnderAveByAllCount(void) {
	return
		"WITH ExamScores AS ( "
		"  SELECT "
		"    e.examinee_id, "
		"    e.examinee_name, "
		"    e.furigana, "
		"    ex.exam_date, "
		"    MAX(CASE WHEN s.Subject_id = 1 THEN s.score ELSE 0 END) AS score1, "
		"    MAX(CASE WHEN s.Subject_id = 2 THEN s.score ELSE 0 END) AS score2, "
		"    MAX(CASE WHEN s.Subject_id = 3 THEN s.score ELSE 0 END) AS score3, "
		"    MAX(CASE WHEN s.Subject_id IN (4, 5, 6) THEN s.score ELSE 0 END) AS scoreA, "
		"    MAX(CASE WHEN s.Subject_id IN (7, 8, 9) THEN s.score ELSE 0 END) AS scoreB, "
		"    MAX(CASE WHEN s.Subject_id IN (4, 5, 6) THEN s.Subject_id ELSE 0 END) AS elective1, "
		"    MAX(CASE WHEN s.Subject_id IN (7, 8, 9) THEN s.Subject_id ELSE 0 END) AS elective2 "
		"  FROM exam ex "
		"  JOIN score s ON ex.exam_id = s.exam_id "
		"  JOIN examinee e ON e.examinee_id = ex.examinee_id "
		"  WHERE ex.exam_date = ? "
		"  GROUP BY e.examinee_id, ex.exam_date, e.examinee_name, e.furigana "
		"), "
		"ExamTotals AS ( "
		"  SELECT *, (score1 + score2 + score3 + scoreA + scoreB) AS total_score "
		"  FROM ExamScores "
		"), "
		"ElectiveGroupAverage AS ( "
		"  SELECT exam_date, elective1, elective2, AVG(total_score) AS avg_total_score "
		"  FROM ExamTotals "
		"  GROUP BY exam_date, elective1, elective2 "
		") "
		"SELECT COUNT(*) AS total_count FROM ( "
		"  SELECT "
		"    et.examinee_name, "
		"    et.furigana, "
		"    subjA.subject_name AS \"選択科目1\", "
		"    subjB.subject_name AS \"選択科目2\", "
		"    (et.total_score / 5.0) AS \"受験者の平均点\", "   /* 各試験の合計得点を5で割った個人の平均得点 */
		"    (ega.avg_total_score / 5.0) AS \"全科目平均点数\" "
		"  FROM ExamTotals et "
		"  JOIN ElectiveGroupAverage ega ON et.exam_date = ega.exam_date "
		"      AND et.elective1 = ega.elective1 "
		"      AND et.elective2 = ega.elective2 "
		"  LEFT JOIN Subjects subjA ON et.elective1 = subjA.subject_id "
		"  LEFT JOIN Subjects subjB ON et.elective2 = subjB.subject_id "
		"  WHERE et.total_score < ega.avg_total_score "
		") sub; ";
}

//機能7 SQL文
//単一科目用のSQL文を返す
const char* buildSingleSubjectSQL() {
		return
			"WITH RankedScores AS ( "
			"   SELECT "
			"       e.examinee_name, "
			"       e.furigana, "
			"       s.score, "
			"       sb.Subject_name, "
			"       ex.exam_date, "
			"       DENSE_RANK() OVER (PARTITION BY s.Subject_id ORDER BY s.score DESC) AS rank "
			"   FROM score s "
			"   JOIN examinee e ON s.examinee_id = e.examinee_id "
			"   JOIN Subjects sb ON s.Subject_id = sb.Subject_id "
			"   JOIN exam ex ON s.exam_id = ex.exam_id "
			"   WHERE s.Subject_id=? "
			") "
			" SELECT rank, examinee_name, furigana, score, exam_date  "
			" FROM RankedScores "
			" WHERE rank <= 10 "
			" ORDER BY rank ASC, score DESC, furigana ASC , exam_date DESC;";
}

//機能8 SQL文
const char* SumAllSubjectScore(void) {
	return
		"WITH ScoreCalcs AS ( "
		"  SELECT e.examinee_id, e.examinee_name, e.furigana, comp.exam_date, "
		"         (COALESCE(comp.japanese, 0) + COALESCE(comp.mathematics, 0) + COALESCE(comp.english, 0) "
		"          + COALESCE(eleA.max_score, 0) + COALESCE(eleB.max_score, 0)) AS total_score "
		"  FROM examinee e "
		"  JOIN ( "
		"     SELECT s.examinee_id AS examinee_id, "
		"            ex.exam_date, "
		"            MAX(CASE WHEN Subject_id = 1 THEN score ELSE 0 END) AS japanese, "
		"            MAX(CASE WHEN Subject_id = 2 THEN score ELSE 0 END) AS mathematics, "
		"            MAX(CASE WHEN Subject_id = 3 THEN score ELSE 0 END) AS english "
		"     FROM score s "
		"     JOIN exam ex ON s.exam_id = ex.exam_id "
		"     GROUP BY s.examinee_id, ex.exam_date "
		"  ) comp ON e.examinee_id = comp.examinee_id "
		"  LEFT JOIN ( "
		"     SELECT s.examinee_id, ex.exam_date, MAX(s.score) AS max_score "
		"     FROM score s "
		"     JOIN exam ex ON s.exam_id = ex.exam_id "
		"     WHERE Subject_id IN (4, 5, 6) "
		"     GROUP BY s.examinee_id, ex.exam_date "
		"  ) eleA ON e.examinee_id = eleA.examinee_id AND comp.exam_date = eleA.exam_date "
		"  LEFT JOIN ( "
		"     SELECT s.examinee_id, ex.exam_date, MAX(s.score) AS max_score "
		"     FROM score s "
		"     JOIN exam ex ON s.exam_id = ex.exam_id "
		"     WHERE s.Subject_id IN (7, 8, 9) "
		"     GROUP BY s.examinee_id, ex.exam_date "
		"  ) eleB ON e.examinee_id = eleB.examinee_id AND comp.exam_date = eleB.exam_date "
		"), Ranked AS ( "
		"  SELECT exam_date, examinee_name, furigana, total_score, "
		"         CAST(DENSE_RANK() OVER( ORDER BY total_score DESC) AS INTEGER) AS rnk "
		"  FROM ScoreCalcs "
		") "
		"SELECT rnk, examinee_name, furigana, total_score, exam_date "
		"FROM Ranked "
		"WHERE rnk <= 10 "
		"ORDER BY total_score DESC, furigana ASC , exam_date DESC;";
}