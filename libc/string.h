#ifndef STRING_H
#define STRING_H

#include "stdint.h"
#include "stddef.h"

size_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* source);
void* memset(void* s, int c, size_t n);
void uitoa(uint64_t value, char* str);

#endif