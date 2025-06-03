#ifndef GetSubjectTopNames_H
#define GetSubjectTopNames_H

//データベースに登録されている日付を数える関数
ExamData* getExamData(int* countExamDays);

//機能5
int GetNamesUnderAveBySubject(const Subject subject, const int decidedExamDate);

//機能7
int ShowSubjectTopNames(const Subject subject);

//機能8
int GetTotalTopName(void);

#endif

