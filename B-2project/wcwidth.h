// wcwidth.h
#include <locale.h>
#ifndef WCWIDTH_H
#define WCWIDTH_H



#ifdef __cplusplus
extern "C" {
#endif

	int wcwidth(wchar_t ucs);	
	int wcswidth(const wchar_t* pwcs);
	int get_dis_width(const char* str);

#ifdef __cplusplus
}
#endif

#endif  // WCWIDTH_H
