#include "string.h"
#include "stdint.h"

uint32_t strlen(const char* str){
    uint32_t len = 0;
    while(str[len]){
        len++;
    }
    return len;
}

int strcmp(const char* s1, const char* s2){
    while(*s1 && (*s1 == *s2)){
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// Compare n characters of two strings
int strncmp(const char* s1, const char* s2, uint32_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }

    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* strcpy(char* dest, const char* src) {
    char* orig_dest = dest;
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
    return orig_dest;
}

// Copy n characters
char* strncpy(char* dest, const char* src, uint32_t n) {
    char* orig_dest = dest;
    while (n && *src) {
        *dest++ = *src++;
        n--;
    }
    while (n) {
        *dest++ = '\0';
        n--;
    }
    return orig_dest;
}

void* memset(void* s, int c, uint32_t n) {
    unsigned char* p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}