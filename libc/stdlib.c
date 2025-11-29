#include "stdlib.h"

int atoi(const char *str) {
    int result = 0, sign = 1, found = 0;
    while (*str == ' ' || *str == '\t' || *str == '\n' ||
           *str == '\v' || *str == '\f' || *str == '\r') str++;
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') str++;
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
        found = 1;
    }
    return found ? sign * result : -1;
}