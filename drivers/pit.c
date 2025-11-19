#include "pit.h"
#include "io/ports.h"
#include "driver.h"
#include "error_handling/errno.h"
#include "libc/stddef.h"
#include "scheduler/task.h"

static volatile uint64_t pit_ticks = 0;
static pit_callback_t tick_callback = 0;

// Forward declaration of driver init function
static kerr_t pit_driver_init(driver_t* drv);

// Driver structure
static driver_t pit_driver = {
    .name = "PIT",
    .type = DRIVER_TYPE_TIMER,
    .version = 1,
    .priority = 20,  // Initialize after IDT (priority 10)
    .init = pit_driver_init,
    .cleanup = NULL,
    .depends_on = "IDT",  // Depends on interrupt system
    .driver_data = NULL
};

// Driver initialization function (actual PIT setup)
static kerr_t pit_driver_init(driver_t* drv) {
    // Default frequency: 100 Hz
    uint32_t frequency = 100;

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

    return E_OK;
}

// Public init function - registers the driver
kerr_t pit_register(uint32_t frequency) {
    // Note: For now we ignore the frequency parameter in registration
    // The actual init uses a default of 100 Hz
    // This could be improved by storing frequency in driver_data
    return driver_register(&pit_driver);
}

void pit_set_callback(pit_callback_t callback) {
    tick_callback = callback;
}

uint64_t pit_get_ticks(void) {
    return pit_ticks;
}

void pit_handler(void) {
    pit_ticks++;
    scheduler_tick();

    if (tick_callback) {
        tick_callback();
    }
}