#ifndef INPUTCANSEL_H
#define INPUTCANSEL_H

#include <stdbool.h>

// 漢字・カタカナ以外の入力をキャンセルする関数
bool cansel_kanji_and_katakana(const char* input);

 //半角数字8桁のみの入力を受け付ける関数
bool cansel_hankaku_8number(const char* input);

 //ひらがな以外の入力をキャンセルする関数
bool cansel_hiragana_igai(const char* input);


#endif // INPUT_VALIDATION_H
