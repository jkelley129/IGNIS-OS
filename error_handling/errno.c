#include "errno.h"
#include "serial.h"
#include "console/console.h"

const char* k_strerror(kerr_t err){
    switch(err){
        case E_OK: return "Success";
        case E_NOMEM: return "Out of Memory";
        case E_INVALID: return "Invalid argument";
        case E_NOTFOUND: return "Not found";
        case E_EXISTS: return "Already exists";
        case E_NOTDIR: return "Not a directory";
        case E_ISDIR: return "Is a directory";
        case E_TIMEOUT: return "Operation Timed Out";
        case E_PERM: return "Permission denied";
        case E_HARDWARE: return "Hardware fault";
    }
}

void k_pkerr(kerr_t err) {
    console_puts_color("[FAILED]: ",
        (console_color_attr_t) {CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
    console_puts(k_strerror(err));
    console_putc('\n');

    serial_debug_puts("[FAILED]: ");
    serial_debug_puts(k_strerror(err));
    serial_debug_putc('\n');
}

