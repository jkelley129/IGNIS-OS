#ifndef STRING_H
#define STRING_H

#include "stdint.h"

uint32_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, uint32_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, uint32_t n);
void* memset(void* s, int c, uint32_t n);

#endif