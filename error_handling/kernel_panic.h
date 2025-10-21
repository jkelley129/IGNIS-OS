#ifndef KERNEL_PANIC_H
#define KERNEL_PANIC_H

#include "../libc/stdint.h"

// Panic with a message
void kernel_panic(const char* message);

// Panic with a message and additional context
void kernel_panic_with_context(const char* message, const char* file,
                                int line, const char* function);

// Panic with error code
void kernel_panic_with_error(const char* message, int error_code);

// Convenience macro for panic with location info
#define PANIC(msg) kernel_panic_with_context(msg, __FILE__, __LINE__, __func__)

// Assert macro that panics on failure
#define KASSERT(condition, msg) \
do { \
if (!(condition)) { \
kernel_panic_with_context("Assertion failed: " msg, \
__FILE__, __LINE__, __func__); \
} \
} while(0)

// Verify macro that returns error instead of panicking
#define KVERIFY(condition, error_code) \
do { \
if (!(condition)) { \
return (error_code); \
} \
} while(0)

// Panic if pointer is null
#define PANIC_ON_NULL(ptr, msg) \
do { \
if ((ptr) == 0) { \
PANIC(msg); \
} \
} while(0)

// Get current stack trace information
typedef struct {
    uint64_t rbp;
    uint64_t rsp;
    uint64_t rip;
    uint64_t cr2;
    uint64_t cr3;
} stack_frame_t;

void get_stack_frame(stack_frame_t* frame);

#endif