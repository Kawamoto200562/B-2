#include "inputcansel.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

bool is_hiragana(const char* str) {
    wchar_t wstr[256];
    size_t len;

    // ロケール設定（日本語対応）
    //setlocale(LC_CTYPE, "");

    // マルチバイト→ワイド文字列に変換
    len = mbstowcs(wstr, str, sizeof(wstr) / sizeof(wstr[0]));
    if (len == (size_t)-1) return false;

    for (size_t i = 0; i < len; i++) {
        // ひらがなのUnicode範囲：0x3040〜0x309F
        if (wstr[i] < 0x3040 || wstr[i] > 0x309F) {
            return false;
        }
    }
    return true;
}
