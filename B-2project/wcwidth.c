//wcwidth.cpp
#include"wcwidth.h"

//日本語で、与えられた文字(wchar_t型)に基づいて、その表示幅を返す
//今回は、DBで与えられた文字列をこの関数で処理して、表示（文字ずれの解消）
int wcwidth(wchar_t ucs) {
    if (ucs == 0)
        return 0;
    if (ucs < 32 || (ucs >= 0x7F && ucs < 0xA0))
        return -1;

    // 日本語専用の範囲判定
    if ((ucs >= 0x3040 && ucs <= 0x309F) || // ひらがな
        (ucs >= 0x30A0 && ucs <= 0x30FF) || // カタカナ
        (ucs >= 0x4E00 && ucs <= 0x9FFF))   // 漢字
    {
        return 2; // 全角幅
    }

    // それ以外は半角幅とする
    return 1;
}