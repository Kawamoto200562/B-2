#ifndef reffer2db_H
#define reffer2db_H

//データベースにデータが格納されているかを調べる変数
int CheckExistData(void);

//データベースに登録されている日付を数える関数
int* getExamData(int* countExamDays);

//機能2
int GetTopScoresByDate(const int decidedExamDate);


//機能4
int GetAverageScoreByDate(const int decidedExamDate);

//機能5
int GetNamesUnderAveBySubject(const Subject subject, const int decidedExamDate);

//機能6
int GetNamesUnderAveByAll(const int decidedExamDate);

//機能7
int ShowSubjectTopNames(const Subject subject);

//機能8
int GetTotalTopName(void);

#endif

