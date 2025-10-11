#ifndef ERRNO_H
#define ERRNO_H

typedef enum {
    E_OK = 0,
    E_NOMEM = -1,
    E_INVALID = -2,
    E_NOTFOUND = -3,
    E_EXISTS = -4,
    E_NOTDIR = -5,
    E_ISDIR = -6,
    E_TIMEOUT = -7,
    E_PERM = -8,
    E_HARDWARE = -9,
} kerr_t;

const char* k_strerror(kerr_t err);

// Helper macros for common patterns
#define RETURN_IF_ERROR(expr) \
    do { \
        kerr_t _err = (expr); \
        if (_err != E_OK) return _err; \
    } while(0)

#define CHECK_NULL(ptr, err) \
    if (!(ptr)) return (err)

#define TRY(expr) do {          \
    kerr_t _err = (expr);        \
    if (_err != ERR_OK) return _err; \
} while (0)


#endif