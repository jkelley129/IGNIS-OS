#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../libc/stdint.h"
#include "../io/ports.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

void keyboard_init();
void keyboard_handler();

#endif