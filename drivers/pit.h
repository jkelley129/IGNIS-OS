#ifndef PIT_H
#define PIT_H

#include "../libc/stdint.h"

#define PIT_FREQUENCY 1193182  // Base frequency of PIT in Hz
#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_COMMAND 0x43

// PIT modes
#define PIT_MODE_INTERRUPT_ON_TERMINAL_COUNT 0x00
#define PIT_MODE_HARDWARE_RETRIGGERABLE_ONE_SHOT 0x02
#define PIT_MODE_RATE_GENERATOR 0x04
#define PIT_MODE_SQUARE_WAVE_GENERATOR 0x06
#define PIT_MODE_SOFTWARE_TRIGGERED_STROBE 0x08
#define PIT_MODE_HARDWARE_TRIGGERED_STROBE 0x0A

// Access modes
#define PIT_ACCESS_LATCH 0x00
#define PIT_ACCESS_LOBYTE 0x10
#define PIT_ACCESS_HIBYTE 0x20
#define PIT_ACCESS_LOHIBYTE 0x30

// Channel select
#define PIT_CHANNEL_0 0x00
#define PIT_CHANNEL_1 0x40
#define PIT_CHANNEL_2 0x80

typedef void (*pit_callback_t)(void);

void pit_init(uint32_t frequency);
void pit_set_callback(pit_callback_t callback);
uint64_t pit_get_ticks(void);
void pit_handler(void);

#endif