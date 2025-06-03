#ifndef subjectEnum_H
#define subjectEnum_H

// exam_day を "YYYY-MM-DD" 形式（10文字＋終端文字）として保持する構造体
typedef struct {
    char exam_day[11];
} ExamData;

//各教科をenum型で管理
typedef enum {
    Unselected,     //0、何も選択されていない状態
    Japanese,       //1、国語
    Mathematics,    //2、数学
    English,        //3、英語
    JapaneseHistory,//4、日本史
    WorldHistory,   //5、世界史
    Geography,      //6、地理、
    Physics,        //7、物理
    Chemistry,      //8、化学
    Biology,        //9、生物
    All,            //10、各科目
    BackSelectMenu, //11、「参照機能選択一覧に戻る」用に定義
}Subject;

#endif