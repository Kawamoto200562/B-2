#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include "subjectEnum.h"
#include "GetSubjectTopNames.h"
#include "wcwidth.h" 	//wcwidth()用


//定数でフィールド幅を決める(データベースから持ってきた受験者名、ふりがなの文字数)
#define NAME_FIELD_WIDTH		30
#define FURIGANA_FIELD_WIDTH	30


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

ExamData* getExamData(int* countExamDates);															//機能5,6 データベースに保存されている日付の種類をカウントする変数
sqlite3* openDatabase(const char* filename);														//データベース接続を行い、ハンドルを返す
int get_display_width(const wchar_t* ws);															//各文字の表示幅を計算する関数(全角なら通常2,半角なら1)
int prepareSQL(sqlite3* db, const char* sql, sqlite3_stmt** stmt);									//stmtにsql文をコンパイルしたものを格納し、実行に備える
int bindExamParameters(sqlite3_stmt* stmt, int decidedExamDate, Subject subject, sqlite3* db);		//機能5 決定された試験日と科目をSQL文内の？へバインド
int bindSingleSubject(sqlite3_stmt* stmt, Subject subject, sqlite3* db);							//機能7 単一科目用SQL文のプレースホルダにSubject型の値をバインドする
double getAverageScore(sqlite3* db, int decidedExamDate, Subject subject);
void print_aligned(const wchar_t* ws, int field_width);												//指定したフィールド幅になるように左寄席で出力する関数 出力後に足りない部分の空白を出力する
void displayNamesUnderAve(sqlite3_stmt* stmt, int decidedExamDate, const char* subjectName, double avgScore);
void printTitleSubject(Subject subject);															//機能7 「[科目名]の上位10位以内のテスト結果」を順に表示する関数
void displayTopScores(sqlite3_stmt* stmt);															//機能7,8 sqlite3_step()で結果を取得して標準出力に表示する
void displayTotalTopScores(sqlite3_stmt* stmt);														//機能7,8 合計得点の結果を表示する関数（受験者名・ふりがなはそれぞれ30文字固定）
const char* CountDates(void);																		//機能5,6用、試験日の日数をカウントするSQL文を返す
const char* LoadExamDates(void);																	//機能5,6用、試験日を表示する関数
const char* GetUnderAveBYSubject(void);																//機能5 SQL文
const char* buildSingleSubjectSQL(void);															//機能7 SQL文 単一科目用のSQL文を返す(ALLの場合は、これを各教科ごとに繰り返す)
const char* SumAllSubjectScore(void);																//機能8 SQL文



//機能5
int GetNamesUnderAveBySubject(const Subject subject, const int decidedExamDate) {

	sqlite3* db;
	sqlite3_stmt* stmt = NULL;
	int rc = 0;

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

	const char* sql = GetUnderAveBYSubject();

	//subjectがAll(=10)であれば、個別科目（国語~生物: 1～10）の各科目で結果を表示
	if (subject == All) {
		for (int subjectInt = Japanese; subjectInt <= Biology; subjectInt++) {
			rc = prepareSQL(db, sql, &stmt);
			if (rc != SQLITE_OK) {
				fprintf(stderr, "SQL文の準備に失敗しました: %s\n", sqlite3_errmsg(db));
				//完全に失敗とせず、該当科目はスキップし次へ
				continue;
			}
			rc = bindExamParameters(stmt, decidedExamDate, (Subject)subjectInt, db);
			if (rc != SQLITE_OK) {
				sqlite3_finalize(stmt);
				continue;
			}
			//科目毎の平均点を取得
			double avgScore = getAverageScore(db, decidedExamDate, (Subject)subjectInt);
			//ヘルパー関数で結果を表示
			displayNamesUnderAve(stmt, decidedExamDate, g_subjectNames[subjectInt],avgScore);
			sqlite3_finalize(stmt);
		}
		goto cleanup;
	}
	else {
		//単一科目の場合
		rc = prepareSQL(db, sql, &stmt);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "SQL文の準備に失敗しました: %s\n", sqlite3_errmsg(db));
			goto cleanup;
		}
			rc = bindExamParameters(stmt, decidedExamDate, subject, db);
			if (rc != SQLITE_OK) {
				goto cleanup;
			}
			//対象科目の平均点を取得
			double avgScore = getAverageScore(db, decidedExamDate, subject);
			displayNamesUnderAve(stmt, decidedExamDate, g_subjectNames[subject], avgScore);
			goto cleanup;

		//科目、日付が引数で渡されたので、それらを元に、平均点数以下の受験者を表示
		//全科目の場合
		//単一科目の場合
		//指定された日付、教科がなかった場合
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
}


//

//機能７
//mainからSubject型を受取り、科目ごとに表示する
	int ShowSubjectTopNames(const Subject subject) {

		sqlite3* db;
		sqlite3_stmt* stmt = NULL;
		int rc = 0;

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


//ALLの場合、forループでJapanese～Biology(1～9)の各科目を処理
	if (subject == All) {
		for (int i = Japanese; i <= Biology; i++) {

			//subject型「sub」へ、iを代入
			Subject sub = (Subject)i;
			printTitleSubject(sub);

			const char* sql = buildSingleSubjectSQL();
			rc = prepareSQL(db, sql, &stmt);
			if (rc != SQLITE_OK) {
				fprintf(stderr, "Error preparing SQL for subject [%s] (iteration %d)\n", g_subjectNames[sub], i);
				goto cleanup;
			}
			rc = bindSingleSubject(stmt, sub, db);
			if (rc != SQLITE_OK) {
				fprintf(stderr, "Error binding parameter for subject [%s] (iteration %d)\n", g_subjectNames[sub], i);;
				goto cleanup;
			}
			displayTopScores(stmt);
			sqlite3_finalize(stmt);
			stmt = NULL;
			printf("\n"); //科目間の区切り
		}
	}
	else if (subject >= Japanese && subject < All) {
		//単一科目の場合
		const char* sql = buildSingleSubjectSQL();
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
		displayTopScores(stmt);
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
ExamData* getExamData(int* countExamDates) {
	sqlite3* db = NULL;
	sqlite3_stmt* stmt = NULL;
	int rc = 0;
	ExamData* data = NULL;

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
	data = malloc((*countExamDates) * sizeof(ExamData));
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
		// カラム0: 試験日に保存されている整数（例えば 20250101）
		int dateValue = sqlite3_column_int(stmt, 0);
		// 整数値から年月日を抽出
		int year = dateValue / 10000;
		int month = (dateValue % 10000) / 100;
		int day = dateValue % 100;
		// "YYYY-MM-DD" の形式で書式設定して、構造体に格納
		snprintf(data[index].exam_day, sizeof(data[index].exam_day), "%04d-%02d-%02d", year, month, day);
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
	//ここをrcにすると、正しく開けなくなる
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

//stmtにsql文をコンパイルしたものを格納し、実行に備える
int prepareSQL(sqlite3* db, const char* sql, sqlite3_stmt** stmt) {
	int rc = sqlite3_prepare_v2(db, sql, -1, stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "クエリ準備エラー: %s\n", sqlite3_errmsg(db));
	}
	return rc;
}

//機能5 決定された試験日と科目をSQL文内の？へバインド
int bindExamParameters(sqlite3_stmt* stmt, int decidedExamDate, Subject subject, sqlite3* db) {
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
}

//機能7 単一科目用SQL文のプレースホルダにSubject型の値をバインドする
int bindSingleSubject(sqlite3_stmt* stmt, Subject subject, sqlite3* db) {
	int rc = sqlite3_bind_int(stmt, 1, subject);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "バインドエラー(単一科目): %s\n", sqlite3_errmsg(db));
	}
	return rc;
}

/**
 * getAverageScore
 *
 * 指定された試験日(decidedExamDate)と科目(subject)に対する平均得点（scoreのAVG）をDBから取得し返す。
 *
 * @param db                SQLite データベースハンドル
 * @param decidedExamDate   試験日 (整数形式; 例: 20250101)
 * @param subject           対象科目 (Subject 型)
 * @return                  平均得点（double）。取得できなかった場合は 0.0 を返す。
 */
double getAverageScore(sqlite3* db, int decidedExamDate, Subject subject) {
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
	return avg;
}

//指定したフィールド幅になるように左寄席で出力する関数
//出力後に足りない部分の空白を出力する
void print_aligned(const wchar_t* ws, int field_width) {
	wprintf(L"%ls", ws);
	int current_width = get_display_width(ws);
	for (int i = 0; i < field_width - current_width; i++) {
		wprintf(L" ");
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
void displayNamesUnderAve(sqlite3_stmt* stmt, int decidedExamDate, const char* subjectName, double avgScore) {
	int found = 0;
	int rc = 0;
	char examDateStr[16];
	int year = decidedExamDate / 10000;
	int month = (decidedExamDate % 10000) / 100;
	int day = decidedExamDate % 100;
	snprintf(examDateStr, sizeof(examDateStr), "%04d-%02d-%02d", year, month, day);

	printf("\n--- 試験日 %s の [%s] の (平均: %.2f点) 平均点以下の受験者一覧 ---\n",
		examDateStr, subjectName, avgScore);

	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		const unsigned char* examineeName = sqlite3_column_text(stmt, 0);
		int score = sqlite3_column_int(stmt, 1);
		printf("受験者: %s, 得点: %d\n", examineeName, score);
		found++;
	}
	if (found == 0) {
		printf("該当する受験者は存在しませんでした。\n");
	}
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

//sqlite3_step()で結果を取得して標準出力に表示する
void displayTopScores(sqlite3_stmt* stmt) {
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int rank = sqlite3_column_int(stmt, 0);
		const unsigned char* mb_examinee_name = sqlite3_column_text(stmt, 1);
		const unsigned char* mb_furigana = sqlite3_column_text(stmt, 2);
		int score = sqlite3_column_int(stmt, 3);

		//マルチバイト文字列からワイド文字列へ変換するバッファ
		wchar_t w_examinee_name[256] = { 0 };
		wchar_t w_furigana[256] = { 0 };
		size_t conv_examinee = 0;
		size_t conv_furigana = 0;
		errno_t err;

		// mbstowcs_s の呼び出し（個別の変数を使用）
		err = mbstowcs_s(&conv_examinee, w_examinee_name, 256, (const char*)mb_examinee_name, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting examinee_name\n");
		}
		err = mbstowcs_s(&conv_furigana, w_furigana, 256, (const char*)mb_furigana, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting furigana\n");
		}

		//表示:【rank位 名前】は通常のフォーマット関数で　
		wprintf(L"%2d位 名前: ", rank);
		//各フィールドについて、実際の表示幅を計算して余白で埋める
		print_aligned(w_examinee_name, NAME_FIELD_WIDTH);
		print_aligned(w_furigana, FURIGANA_FIELD_WIDTH);
		wprintf(L"%3d点\n", score);
	}
}


// 合計得点の結果を表示する関数（受験者名・ふりがなはそれぞれ30文字固定）
void displayTotalTopScores(sqlite3_stmt* stmt) {
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		// SQL文のカラム順に沿って値を取得
		int rank = sqlite3_column_int(stmt, 0);
		const unsigned char* mb_examinee_name = sqlite3_column_text(stmt, 1);
		const unsigned char* mb_furigana = sqlite3_column_text(stmt, 2);
		int total_score = sqlite3_column_int(stmt, 3);

		// マルチバイト文字列をワイド文字列に変換するためのバッファ
		wchar_t w_examinee_name[256] = { 0 };
		wchar_t w_furigana[256] = { 0 };
		size_t conv_examinee = 0, conv_furigana = 0;
		errno_t err;

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
		wprintf(L"%2d位 ", rank);
		print_aligned(w_examinee_name, NAME_FIELD_WIDTH);  // NAME_FIELD_WIDTH は30に定義
		print_aligned(w_furigana, FURIGANA_FIELD_WIDTH);     // FURIGANA_FIELD_WIDTH は30に定義
		// 合計得点は十分な幅（ここでは6桁）を確保して表示
		wprintf(L"%6d点\n", total_score);
	}
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

//機能5 SQL文
const char* GetUnderAveBYSubject(void) {
	return
		" SELECT e.examinee_name, s.score "
		" FROM score s "
		" JOIN examinee e ON s.examinee_id = e.examinee_id "
		" JOIN exam ex ON s.exam_id = ex.exam_id "
		" WHERE ex.exam_date = ? AND s.subject_id = ? "
		" AND s.score < ( "
		"	SELECT AVG(s.score) "
		" FROM score s "
		" JOIN exam ex ON s.exam_id = ex.exam_id "
		" WHERE ex.exam_date = ? AND s.subject_id = ? "
		" );";
}

//機能7 SQL文
//単一科目用のSQL文を返す
const char* buildSingleSubjectSQL(void) {
	return
		"WITH RankedScores AS (	"
		"	SELECT "
		"		e.examinee_name,"
		"		e.furigana,		"
		"		s.score,		"
		"		sb.Subject_name,		"
		"		DENSE_RANK() OVER (PARTITION BY s.Subject_id ORDER BY s.score DESC) AS rank "
		"	FROM score s		"
		"	JOIN examinee e ON s.examinee_id = e.examinee_id "
		"	JOIN Subjects sb ON s.Subject_id = sb.Subject_id	"
		"	WHERE s.Subject_id=? "
		")	"
		"SELECT rank, examinee_name, furigana, score "
		"FROM RankedScores		"
		"WHERE rank <= 10		"
		"ORDER BY Subject_name,rank,score DESC;";
}

//機能8 SQL文
const char* SumAllSubjectScore(void) {
	return
		"WITH ScoreCalcs AS ( "
		"  SELECT e.examinee_id, e.examinee_name, e.furigana, "
		"         (COALESCE(comp.japanese, 0) + COALESCE(comp.mathematics, 0) + COALESCE(comp.english, 0) "
		"          + COALESCE(eleA.max_score, 0) + COALESCE(eleB.max_score, 0)) AS total_score "
		"  FROM examinee e "
		"  LEFT JOIN ( "
		"     SELECT examinee_id, "
		"            MAX(CASE WHEN Subject_id = 1 THEN score ELSE 0 END) AS japanese, "
		"            MAX(CASE WHEN Subject_id = 2 THEN score ELSE 0 END) AS mathematics, "
		"            MAX(CASE WHEN Subject_id = 3 THEN score ELSE 0 END) AS english "
		"     FROM score "
		"     GROUP BY examinee_id "
		"  ) comp ON e.examinee_id = comp.examinee_id "
		"  LEFT JOIN ( "
		"     SELECT examinee_id, MAX(score) AS max_score "
		"     FROM score "
		"     WHERE Subject_id IN (4, 5, 6) "
		"     GROUP BY examinee_id "
		"  ) eleA ON e.examinee_id = eleA.examinee_id "
		"  LEFT JOIN ( "
		"     SELECT examinee_id, MAX(score) AS max_score "
		"     FROM score "
		"     WHERE Subject_id IN (7, 8, 9) "
		"     GROUP BY examinee_id "
		"  ) eleB ON e.examinee_id = eleB.examinee_id "
		"), Ranked AS ( "
		"  SELECT CAST(DENSE_RANK() OVER (ORDER BY total_score DESC) AS INTEGER) AS rnk, "
		"         examinee_name, furigana, total_score "
		"  FROM ScoreCalcs "
		") "
		"SELECT rnk, examinee_name, furigana, total_score "
		"FROM Ranked "
		"WHERE rnk <= 10 "
		"ORDER BY total_score DESC;";
}