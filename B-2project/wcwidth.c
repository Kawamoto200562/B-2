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

int wcswidth(const wchar_t* pwcs) {
    int width = 0;
    while (*pwcs) {
        width += wcwidth(*pwcs);
        pwcs++;
    }
    return width;
}

int get_dis_width(const char* str) {
    wchar_t wstr[256];
    size_t len;

    setlocale(LC_ALL, "");  // ロケール設定（日本語）
    len = mbstowcs(wstr, str, sizeof(wstr) / sizeof(wstr[0]));
    if (len == (size_t)-1) return 0;

    int width = 0;
    for (size_t i = 0; i < len; i++) {
        width += wcwidth(wstr[i]);
    }
    return width;
}
