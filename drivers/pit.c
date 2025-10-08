#include "pit.h"
#include "../io/ports.h"

static volatile uint64_t pit_ticks = 0;
static pit_callback_t tick_callback = 0;

void pit_init(uint32_t frequency) {
    // Calculate the divisor for the desired frequency
    uint32_t divisor = PIT_FREQUENCY / frequency;

    // Ensure divisor is within valid range
    if (divisor < 1) divisor = 1;
    if (divisor > 65535) divisor = 65535;

    // Send command byte:
    // Channel 0, Access mode lohibyte, Rate generator mode
    uint8_t command = PIT_CHANNEL_0 | PIT_ACCESS_LOHIBYTE | PIT_MODE_RATE_GENERATOR;
    outb(PIT_COMMAND, command);

    // Send divisor (low byte, then high byte)
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    pit_ticks = 0;
}

void pit_set_callback(pit_callback_t callback) {
    tick_callback = callback;
}

uint64_t pit_get_ticks(void) {
    return pit_ticks;
}

void pit_handler(void) {
    pit_ticks++;

    if (tick_callback) {
        tick_callback();
    }
}