#ifndef STRING_H
#define STRING_H

#include "stdint.h"

// Define size_t for 64-bit
#ifdef __LP64__
typedef unsigned long size_t;
#else
typedef unsigned int size_t;
#endif

size_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* source);
void* memset(void* s, int c, size_t n);
void uitoa(uint64_t value, char* str);

#endif