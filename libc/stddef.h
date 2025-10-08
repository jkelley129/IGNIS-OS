#ifndef _STDDEF_H
#define _STDDEF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Define NULL */
#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif

/* Define size_t and ptrdiff_t */
typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;

/* Define wchar_t if not built in */
#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif

/* Define offsetof macro */
#define offsetof(type, member) ((size_t)&(((type*)0)->member))

#ifdef __cplusplus
}
#endif

#endif /* _STDDEF_H */
