// wcwidth.h
#include <locale.h>
#ifndef WCWIDTH_H
#define WCWIDTH_H

#ifdef __cplusplus
extern "C" {
#endif

	int wcwidth(wchar_t ucs);

#ifdef __cplusplus
}
#endif

#endif  // WCWIDTH_H
