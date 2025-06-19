#include<stdio.h>
#include<sqlite3.h>
#include<stdlib.h>
#include<stdbool.h>
#include<locale.h>
#include<string.h>
#include<ctype.h>
#include<errno.h>
#include"wcwidth.h"
#include"main_db.h"

#define PAGE_SIZE				50	//１ページに表示する人数
#define NAME_FIELD_WIDTH		30	//名前の長さ
#define FURIGANA_FIELD_WIDTH	30	//ふりがなの長さ
#define NEXT_PAGE				-1	//次のページ
#define BACK_PAGE				-2	//前のページ
#define SwitchSort				-3	//並び替えの変更（id順、またはふりがなの50音順）
#define ELECTIVE_SUB_WIDTH		6	//選択科目名の名前の長さ

int setLocaleUTF_JP_main();																						// ロケールを明示的に設定（日本語のUTF-8環境）
sqlite3* openDatabase_main(const char* filename);																//データベース接続を行い、ハンドルを返す
int prepareSQL_main(sqlite3* db, const char* sql, sqlite3_stmt** stmt);											//stmtにsql文をコンパイルしたものを格納し、実行に備える
int countExaminee_main(sqlite3* db);																			//データベースに登録されている総受験者数をカウントする関数
int bindExamineesForPagination_main(sqlite3_stmt* stmt, sqlite3* db, const int page);							//ページサイズごとに受験者を表示するよう、バインドする関数
void print_aligned_main(const wchar_t* ws, int field_width);													//指定したフィールド幅になるように左寄席で出力する関数 出力後に足りない部分の空白を出力する
int get_display_width_main(const wchar_t* ws);																	//各文字の表示幅を計算する関数(全角なら通常2,半角なら1)
void printExamineeList_main(sqlite3_stmt* stmt, const int currentPage, const int maxPage,bool sortByFuriganaAsc, int pageExamineeIds[], int *pageCount);	//データベースに登録されている受験者一覧を表示する関数
int get_user_command_main(const int currentPage, const int maxPage, const int pageCount, bool sortByFuriganaAsc);	//ユーザーのコマンド入力によって、ページ切り替え、ソートを行なう関数
char* trim_main(char* str);																						//入力された文字の前後の空白を取り除く関数
void clear_screen_main();																						//画面をクリアする関数	
bool printExamResultPickUpId(sqlite3* db, const int pickUpId, const int selectIndex);							//選択された受験生の試験結果等を表示する関数void
void printExamineeName(sqlite3_stmt* stmt, const int selectIndex);													//選択された受験者の名前、フリガナを表示する関数
int printPickUpIdExamResult(sqlite3_stmt* stmt);																//該当idの受験者のテスト結果一覧を表示する関数
const char* SqlPickUpIdExamResult(void);																		//SQL文　該当Idの受験者のテスト結果を表示する為のSQL文を代入する関数
void enterToReturnMenu_main(void);																				//enterを押して、メインメニューに戻る関数


int main_db() {
	sqlite3* db;


	int rc;
	sqlite3_stmt* stmt = NULL;

	int exist =	0;			//データベースにデータが登録されているか確認する変数
	int currentPage = 0;	//現在のページデータを保持する変数
	int totalCount = 0;		//登録されている受験者データの総数（受験者idを用いる）
	int maxPage = 0;		
	int command = 0;		//ユーザーの入力に応じてページ変更、ソートを行なう変数
	int pickUpId = 0;				
	int pageExamineeIds[PAGE_SIZE] = { 0 };
	int pageCount = 0;				//現在のページに表示された受験者の件数
	int selectIndex = 0;			
	bool isErrorHappen = false;		//該当受験者のテスト結果を表示する関数の戻り値に使用
	bool sortByFuriganaAsc = false;	//ふりがなの50温順で並び替えるフラグ

	setLocaleUTF_JP_main();

	// 1 データベースオープン

	db = openDatabase_main("test.db");
	if (db == NULL) {
		goto cleanup;
	}

	// 2 データがデータベースに一件でも登録されているかどうかを確認

	const char* sql = "SELECT EXISTS(SELECT 1 FROM examinee LIMIT 1)";
	rc = prepareSQL_main(db, sql, &stmt);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL準備に失敗しました: %s\n", sqlite3_errmsg(db));
		goto cleanup;
	}

	//ここで、登録されているかを判定(登録されていた場合、existに「1」が代入される)
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		exist = sqlite3_column_int(stmt, 0);
	}

	//登録されてない場合、メインメニューへ戻る
	if (exist == 0) {
		printf("データが登録されていません\n");
		printf("データを登録してください\n\n");
		printf("メインメニューへ戻ります\n");
		goto cleanup;
	}

	//stmtをNULLにして、次のSQL文を読み込むことが出来るようにする
	sqlite3_finalize(stmt);
	stmt = NULL;

	// 3 データが１件でも登録されている場合、登録されている全ての受験者のid,氏名,ふりがな取得して表示

	//3-1 まず、totalCountに、データベースに登録されている受験者の数を代入し、maxPageの表記に利用する
	totalCount = countExaminee_main(db);
	if (totalCount < 0) {
		fprintf(stderr, "総件数の取得に失敗しました。\n");
		rc = 1;
		goto cleanup;
	}

	//maxPageに代入
	maxPage = (totalCount + PAGE_SIZE - 1) / PAGE_SIZE;
	if (maxPage == 0) {
		maxPage = 1;
	}

	//動的プログラムの為、do_while文を使用
	do {

		//3-2登録されている受験者のid,氏名,ふりがなをページ上限まで読み込むSQL文
		if (sortByFuriganaAsc == false) {
			sql = "SELECT examinee_id, examinee_name, furigana FROM examinee ORDER BY examinee_id ASC LIMIT ? OFFSET ? ; ";
		}
		else {
			sql = "SELECT examinee_id, examinee_name, furigana FROM examinee ORDER BY furigana ASC LIMIT ? OFFSET ? ; ";
		}

		//stmtにsql文を代入
		rc = prepareSQL_main(db, sql, &stmt);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "SQL準備に失敗しました:%s\n", sqlite3_errmsg(db));
			goto cleanup;
		}

		//SQL分のLIMIT、OFFSET部分の？にPAGE_SIZE,page*PAGE_SIZEをそれぞれバインドする
		rc = bindExamineesForPagination_main(stmt, db, currentPage);
		if (rc != SQLITE_OK) {
			goto cleanup;
		}


		
		//登録されている受験者のid,氏名,ふりがなをページ上限まで表示する関数
		printExamineeList_main(stmt, currentPage, maxPage,sortByFuriganaAsc,pageExamineeIds,&pageCount);


		// 4 ユーザーに、ページ移動、または特定の受験者の得点について見るか選んでもらう

		//4-1ユーザーに入力を求める関数
		command = get_user_command_main(currentPage, maxPage, pageCount, sortByFuriganaAsc);


		//4-2 入力によって、ページ遷移、または該当受験者の得点情報を表示する

		//受験者が[PAGE_SIZE]人を超えている場合、ページで遷移(0:終了、n:次のページ、b:前のページ)
		if (command == 0) {
			goto cleanup;
		}
		else if (command == NEXT_PAGE) {
			clear_screen_main();
			if (currentPage + 1 < maxPage) {
				currentPage++;
			}
			else {
				printf("既に最後のページです");
			}
		}
		else if (command == BACK_PAGE) {
			clear_screen_main();
			if (currentPage > 0) {
				currentPage--;
			}
			else {
				printf("既に最初のページです");
			}
		}
		//sで受験者id,50音順で並び替えを切り替える事が出来る
		else if (command == SwitchSort) {
			clear_screen_main();
			if (sortByFuriganaAsc == false) {
				sortByFuriganaAsc = true;
				printf("50音順に並び替えました");
			}
			else {
				sortByFuriganaAsc = false;
				printf("受験者id順に並び替えました");
			}
			currentPage = 0;
		}
		else {
			selectIndex = command - 1;	//表記の際、+1したので、-1引いて代入
			if (selectIndex >= 0 && selectIndex < pageCount) {
				pickUpId = pageExamineeIds[selectIndex];
				selectIndex++;		//再び表記に使用するので、+1
				clear_screen_main();
				isErrorHappen = printExamResultPickUpId(db, pickUpId,selectIndex);
				if (isErrorHappen == true) {
					goto cleanup;
				}
			}
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
		continue;

	} while (1);

	//5 データベースの終了処理	この関数を終了する際には、必ずここへ遷移する


cleanup:
	if (stmt) {
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if (db) {
		sqlite3_close(db);
		db = NULL;
	}

	// enterToReturnMenu_main();

	return 0;

}


int setLocaleUTF_JP_main() {
	// ロケールを明示的に設定（日本語のUTF-8環境）
	if (setlocale(LC_ALL, "ja_JP.UTF-8") == NULL) {
		fprintf(stderr, "ロケールの設定に失敗しました。\n");
		return 1;
	}
	return 0;
}

//データベース接続を行い、ハンドルを返す
sqlite3* openDatabase_main(const char* filename) {
	sqlite3* db;
	int rc = sqlite3_open(filename, &db);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "データベースを開けません:%s\n", sqlite3_errmsg(db));
		return NULL;
	}
	return db;
}

//stmtにsql文をコンパイルしたものを格納し、実行に備える
int prepareSQL_main(sqlite3* db, const char* sql, sqlite3_stmt** stmt) {
	int rc = sqlite3_prepare_v2(db, sql, -1, stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "クエリ準備エラー: %s\n", sqlite3_errmsg(db));
	}
	return rc;
}

int countExaminee_main(sqlite3* db) {
	int totalCount = 0;
	const char* countSql = "SELECT COUNT(*) FROM examinee ;";
	sqlite3_stmt* stmt = NULL;

	int rc = prepareSQL_main(db, countSql, &stmt);
	if (rc != SQLITE_OK) {
		return -1;
	}
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		totalCount = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return totalCount;
}

//ページサイズごとに受験者を表示するよう、バインドする関数
int bindExamineesForPagination_main(sqlite3_stmt* stmt, sqlite3* db, const int page) {
	int rc;

	//SQL分のLIMIT、OFFSET部分の？にPAGE_SIZE,page*PAGE_SIZEをそれぞれバインドする
	rc = sqlite3_bind_int(stmt, 1, PAGE_SIZE);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "ページ件数のバインドに失敗しました %s\n", sqlite3_errmsg(db));
		return rc;
	}
	rc = sqlite3_bind_int(stmt, 2, page * PAGE_SIZE);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "オフセットパラメータのバインドに失敗しました %s\n", sqlite3_errmsg(db));
		return rc;
	}
	return rc;
}

//指定したフィールド幅になるように左寄席で出力する関数
//出力後に足りない部分の空白を出力する
void print_aligned_main(const wchar_t* ws, int field_width) {
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

//各文字の表示幅を計算する関数(全角なら通常2,半角なら1)
int get_display_width_main(const wchar_t* ws) {
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

void printExamineeList_main(sqlite3_stmt* stmt, const int currentPage, const int maxPage, bool sortByFuriganaAsc, int pageExamineeIds[], int *pageCount) {

	printf("\nデータベースに登録されている受験者一覧");
	if (sortByFuriganaAsc == true) {
		printf("(50音順)\n");
	}
	else {
		printf("(受験者id順)\n");
	}
	printf("（ページ %d / %d）\n", currentPage + 1, maxPage);
	printf("----------------------------------------------------------------------------------------------------------------------\n");
	printf("| %-4s | %-8s | %-30s  | %-30s \n", "番号", "受検者id", "名前", "ふりがな");
	printf("----------------------------------------------------------------------------------------------------------------------\n");

	int index = 0;
	//stmtから受験者レコードを１行ずつ取得
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int	examinee_id = sqlite3_column_int(stmt, 0);
		const unsigned char* mb_examinee_name = sqlite3_column_text(stmt, 1);
		const unsigned char* mb_furigana = sqlite3_column_text(stmt, 2);

		//表示番号と、実際の受検者IDのマッピングを記録(配列　pageExamineeIdsのindex番目に保存)
		if (index < PAGE_SIZE) {
			pageExamineeIds[index] = examinee_id;
		}

		wchar_t w_examinee_name[256] = { 0 };
		wchar_t w_furigana[256] = { 0 };
		size_t conv_examinee_name = 0;
		size_t conv_furigana = 0;

		errno_t err = mbstowcs_s(&conv_examinee_name, w_examinee_name, 256, (const char*)mb_examinee_name, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs error converting examinee_name");
		}
		err = mbstowcs_s(&conv_furigana, w_furigana, 256, (const char*)mb_furigana, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting furigana\n");
		}

		wprintf(L"%6d |", index + 1);
		wprintf(L"%9d | ", examinee_id);
		print_aligned_main(w_examinee_name, NAME_FIELD_WIDTH);
		printf("| ");
		print_aligned_main(w_furigana, FURIGANA_FIELD_WIDTH);
		printf("\n");
		index++;
	}
	*pageCount = index;
}

//ユーザー入力を文字で受取り、入力された文字に応じてページ移動を行う関数
int get_user_command_main(const int currentPage, const int maxPage, const int pageCount, bool sortByFuriganaAsc) {
	char input[64];
	char* trimmedInput;
	char* endptr;
	int num;

	while (1) {

		//終了は常に表示
		printf("\n0: 終了");

		if (currentPage + 1 < maxPage) {
			printf(" n: 次のページ");
		}
		if (currentPage > 0) {
			printf(" b: 前のページ");
		}
		if (sortByFuriganaAsc == false) {
			printf(" s: 50音順に並び替え");
		}
		else {
			printf(" s: 受験者id順に並び替え");
		}

		printf("\n\n1から%dまでの半角整数を入力すれば、該当番号の受験者情報を見る事が出来ます", pageCount);

		printf("\n\n入力:");
		if (fgets(input, sizeof(input), stdin) == NULL) {
			//入力エラーまたはEOFの場合
			fprintf(stderr, "入力エラーが発生しました、再度入力してください\n");
			continue;
		}

		//入力行末に改行が含まれているかチェックし、除去する処理
		input[strcspn(input, "\n")] = '\0';	//strcspn()は最初の'\n'の位置を返す

		trimmedInput = trim_main(input);

		if (trimmedInput[0] == '\0') {
			printf("入力されていません。再度入力してください\n");
			continue;
		}

		if (strcmp(trimmedInput, "0") == 0) {
			return 0;
		}
		if (strcmp(trimmedInput, "n") == 0) {
			return NEXT_PAGE;
		}
		if (strcmp(trimmedInput, "b") == 0) {
			return BACK_PAGE;
		}
		if (strcmp(trimmedInput, "s") == 0) {
			return SwitchSort;
		}

		//数値変換:strtolで変換を試みる
		errno = 0;
		num = (int)strtol(trimmedInput, &endptr, 10);

		//変換エラーがあった場合
		if (errno != 0 || endptr == trimmedInput || *endptr != '\0') {
			printf("不正な入力です。0、n、b、s または番号(半角整数)を入力してください\n");
			continue;
		}

		//番号が一致しない場合、エラーメッセージを出して、continue
		if (num<1 || num>pageCount) {
			printf("入力された番号に該当する受検者が存在しません。1から%dの半角整数で入力してください\n", pageCount);
			continue;
		}

		//正常な番号が入力された場合、その値を返す
		return num;
	}
}

//入力された文字の前後の空白を取り除く処理
char* trim_main(char* str) {
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

//画面をクリアする関数
void clear_screen_main() {
	// ANSIエスケープシーケンスで画面をクリア＆カーソルを先頭へ移動
	printf("\033[2J\033[H");
	fflush(stdout);
}


//受験者IDが入力された際、該当受験者の試験結果一覧を表示
bool printExamResultPickUpId(sqlite3* db, const int pickUpId, const int selectIndex) {

	bool isErrorHappen = true;				//エラーが起こった場合かどうか、これを返り値として判断する(初期はtrue)
	sqlite3_stmt* stmt = NULL;
	int found = 0;							//該当受験者に試験結果が存在するか調べる関数

	//「[受験者名]（ふりがな）のテスト結果」と表示する部分

	const char* sql = "SELECT examinee_name, furigana FROM examinee WHERE examinee_id = ? ;";

	int rc = prepareSQL_main(db, sql, &stmt);
	if (rc != SQLITE_OK) {
		return isErrorHappen;
	}

	rc = sqlite3_bind_int(stmt, 1, pickUpId);
	if (rc != SQLITE_OK) {
		return isErrorHappen;
	}

	printExamineeName(stmt, selectIndex);


	sqlite3_finalize(stmt);
	stmt = NULL;

	//該当する受験者idの受験者の試験結果表示する為のsql文を読み込み
	sql = SqlPickUpIdExamResult();

	rc = prepareSQL_main(db, sql, &stmt);
	if (rc != SQLITE_OK) {
		return isErrorHappen;
	}

	rc = sqlite3_bind_int(stmt, 1, pickUpId);
	if (rc != SQLITE_OK) {
		return isErrorHappen;
	}

	//該当する試験idの受験者の試験結果を表示

	printf("----------------------------------------------------------------------------------------------------------------------\n");
	printf("| %10s | %-20s | %-7s | %-7s | %-7s | %-6s | %-6s | %-8s\n", "試験日", "選択科目名", "国語", "数学", "英語", "選択1", "選択2", "合計得点");
	printf("----------------------------------------------------------------------------------------------------------------------\n");

	found = printPickUpIdExamResult(stmt);
	if (found == 0) {
		printf("試験結果が存在しません\n");
		printf("試験結果を登録してください。");
	}

	printf("\n\n受験者一覧へ戻ります\n");

	enterToReturnMenu_main();

	clear_screen_main();

	// 7 ユーザー表示が終わったら、受験者id,氏名,ふりがなを表示する画面に戻る
	isErrorHappen = false;
	return isErrorHappen;
}

void printExamineeName(sqlite3_stmt* stmt, const int selectIndex) {
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		const unsigned char* examinee_name = sqlite3_column_text(stmt, 0);
		const unsigned char* furigana = sqlite3_column_text(stmt, 1);

		printf("番号 %d %s(%s)のテスト結果\n\n", selectIndex, examinee_name, furigana);
	}
	else {
		printf("テスト結果が見つかりませんでした\n");
		printf("テスト結果を登録してください。受検者一覧表示メニューにに戻ります\n");
	}
}



//試験日　選択科目　各教科の得点　合計得点　を出力する関数

int printPickUpIdExamResult(sqlite3_stmt* stmt) {

	int found = 0;		//該当受験者にデータが存在するか確認する変数

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int int_exam_date = sqlite3_column_int(stmt, 0); // 数値型の日付
		const unsigned char* mb_electiveSub1 = sqlite3_column_text(stmt, 1);	//選択科目１の名前
		const unsigned char* mb_electiveSub2 = sqlite3_column_text(stmt, 2);	//選択科目２の名前
		int scoreJapanese = sqlite3_column_int(stmt, 3);
		int scoreMath = sqlite3_column_int(stmt, 4);
		int scoreEnglish = sqlite3_column_int(stmt, 5);
		int scoreElectiveSub1 = sqlite3_column_int(stmt, 6);
		int scoreElectiveSub2 = sqlite3_column_int(stmt, 7);
		int scoreSumAll = sqlite3_column_int(stmt, 8);

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

		// 選択科目1と2の科目名をワイド文字列に変換
		wchar_t w_electiveSub1[256] = { 0 };
		wchar_t w_electiveSub2[256] = { 0 };
		size_t conv_electiveSub1 = 0;
		size_t conv_electiveSub2 = 0;

		err = mbstowcs_s(&conv_electiveSub1, w_electiveSub1, 256, (const char*)mb_electiveSub1, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting electiveSub1\n");
		}
		err = mbstowcs_s(&conv_electiveSub2, w_electiveSub2, 256, (const char*)mb_electiveSub2, _TRUNCATE);
		if (err != 0) {
			fwprintf(stderr, L"mbstowcs_s error converting electiveSub2\n");
		}

		// 表示:【日付】【選択科目名】【各科目の点数】【合計得点】

		wprintf(L"%ls", w_formatted_exam_date);          // ワイド文字列の日付を表示
		printf("｜");
		print_aligned_main(w_electiveSub1, ELECTIVE_SUB_WIDTH); // 名前を整列して表示
		wprintf(L" + ");
		print_aligned_main(w_electiveSub2, ELECTIVE_SUB_WIDTH);  // ふりがなを整列して表示
		printf("｜");
		wprintf(L"%3d点 ", scoreJapanese);							//得点　国語
		printf("｜");
		wprintf(L"%3d点 ", scoreMath);								//得点　数学
		printf("｜");
		wprintf(L"%3d点 ", scoreEnglish);							//得点　英語
		printf("｜");
		wprintf(L"%3d点 ", scoreElectiveSub1);						//得点　選択科目1
		printf("｜");
		wprintf(L"%3d点 ", scoreElectiveSub2);						//得点　選択科目２
		printf("｜");
		wprintf(L"%3d点 ", scoreSumAll);							//得点　合計得点
		found++;
		printf("\n");
	}
	return found;
}


void enterToReturnMenu_main() {
	int ch;
	printf("Enterで戻ります。");
	fflush(stdout);  // プロンプトを即座に表示

	/* getchar()がEOFまたは改行を読み込むまでループ */
	while ((ch = getchar()) != '\n') {
		if (ch == EOF) {
			// 入力エラーまたはストリームの終了の場合はループを抜ける
			break;
		}
		/* 余計な入力はそのまま読み捨てる */
	}

	printf("メニューに戻ります。\n");
}

const char* SqlPickUpIdExamResult() {
	return
		"WITH ExamScores AS ("
		"    SELECT "
		"        exam_id, "
		"        MAX(CASE WHEN Subject_id = 1 THEN score END) AS score_kokugo, "
		"        MAX(CASE WHEN Subject_id = 2 THEN score END) AS score_math, "
		"        MAX(CASE WHEN Subject_id = 3 THEN score END) AS score_eigo, "
		"        MAX(CASE WHEN Subject_id IN (4, 5, 6) THEN score END) AS score_option1, "
		"        MAX(CASE WHEN Subject_id IN (4, 5, 6) THEN Subject_id END) AS subID_option1, "
		"        MAX(CASE WHEN Subject_id IN (7, 8, 9) THEN score END) AS score_option2, "
		"        MAX(CASE WHEN Subject_id IN (7, 8, 9) THEN Subject_id END) AS subID_option2 "
		"    FROM score "
		"    GROUP BY exam_id"
		") "
		"SELECT "
		"    e.exam_date AS \"試験日\", "
		"    sub1.Subject_name AS \"選択科目1名\", "
		"    sub2.Subject_name AS \"選択科目2名\", "
		"    es.score_kokugo AS \"国語の点数\", "
		"    es.score_math AS \"数学の点数\", "
		"    es.score_eigo AS \"英語の点数\", "
		"    es.score_option1 AS \"選択科目1の点数\", "
		"    es.score_option2 AS \"選択科目2の点数\", "
		"    (es.score_kokugo + es.score_math + es.score_eigo + es.score_option1 + es.score_option2) AS \"合計得点\" "
		"FROM exam e "
		"JOIN ExamScores es ON e.exam_id = es.exam_id "
		"JOIN Subjects sub1 ON sub1.Subject_id = es.subID_option1 "
		"JOIN Subjects sub2 ON sub2.Subject_id = es.subID_option2 "
		"WHERE e.examinee_id = ? "									/* ユーザーからの受験者IDをパラメータとして指定 */
		"ORDER BY e.exam_date DESC;";
}
