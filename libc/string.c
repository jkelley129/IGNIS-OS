#include "string.h"
#include "stdint.h"

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
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

char* strncpy(char* dest, const char* src, size_t n) {
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

char* strcat(char* dest, const char* src) {
    char* original_dest = dest;

    while (*dest != '\0') {
        dest++;
    }

    while (*src != '\0') {
        *dest++ = *src++;
    }

    *dest = '\0';
    return original_dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

// --- Added memcpy implementation ---
void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void uitoa(uint64_t value, char* str) {
    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    char temp[32];
    int i = 0;

    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }

    int j = 0;
    while (i > 0) {
        str[j++] = temp[--i];
    }
    str[j] = '\0';
}
