#include "driver.h"
#include "serial.h"
#include "../console/console.h"
#include "../libc/string.h"

// Driver registry
static driver_t* driver_registry[MAX_DRIVERS];
static uint8_t driver_count = 0;

// Initialize the driver registry
kerr_t driver_registry_init(void) {
    for (int i = 0; i < MAX_DRIVERS; i++) {
        driver_registry[i] = NULL;
    }
    driver_count = 0;
    return E_OK;
}

// Register a new driver
kerr_t driver_register(driver_t* driver) {
    if (!driver) return E_INVALID;

    if (driver_count >= MAX_DRIVERS) {
        return E_NOMEM;
    }

    // Check for duplicate names
    for (uint8_t i = 0; i < driver_count; i++) {
        if (strcmp(driver_registry[i]->name, driver->name) == 0) {
            return E_EXISTS;
        }
    }

    // Set initial status
    driver->status = DRIVER_STATUS_UNINITIALIZED;

    // Add to registry
    driver_registry[driver_count] = driver;
    driver_count++;

    return E_OK;
}

// Unregister a driver by name
kerr_t driver_unregister(const char* name) {
    if (!name) return E_INVALID;

    for (uint8_t i = 0; i < driver_count; i++) {
        if (strcmp(driver_registry[i]->name, name) == 0) {
            // Call cleanup if driver is initialized
            if (driver_registry[i]->status == DRIVER_STATUS_INITIALIZED ||
                driver_registry[i]->status == DRIVER_STATUS_ENABLED) {
                if (driver_registry[i]->cleanup) {
                    driver_registry[i]->cleanup(driver_registry[i]);
                }
            }

            // Shift remaining drivers down
            for (uint8_t j = i; j < driver_count - 1; j++) {
                driver_registry[j] = driver_registry[j + 1];
            }

            driver_registry[driver_count - 1] = NULL;
            driver_count--;

            return E_OK;
        }
    }

    return E_NOTFOUND;
}

// Get driver by name
driver_t* driver_get_by_name(const char* name) {
    if (!name) return NULL;

    for (uint8_t i = 0; i < driver_count; i++) {
        if (strcmp(driver_registry[i]->name, name) == 0) {
            return driver_registry[i];
        }
    }

    return NULL;
}

// Get all drivers of a specific type
uint8_t driver_get_by_type(driver_type_t type, driver_t** drivers, uint8_t max_count) {
    if (!drivers) return 0;

    uint8_t found = 0;
    for (uint8_t i = 0; i < driver_count && found < max_count; i++) {
        if (driver_registry[i]->type == type) {
            drivers[found] = driver_registry[i];
            found++;
        }
    }

    return found;
}

// Check if driver's dependencies are met
static int driver_dependencies_met(driver_t* driver) {
    // If no dependency, it's met
    if (driver->depends_on[0] == '\0') {
        return 1;
    }

    // Check if dependency exists and is initialized
    driver_t* dep = driver_get_by_name(driver->depends_on);
    if (!dep) {
        return 0;
    }

    if (dep->status != DRIVER_STATUS_INITIALIZED &&
        dep->status != DRIVER_STATUS_ENABLED) {
        return 0;
    }

    return 1;
}

// Initialize all registered drivers in priority order
kerr_t driver_init_all(void) {
    console_puts("\n=== Initializing Drivers ===\n");

    char num_str[8];
    console_puts("Total drivers registered: ");
    uitoa(driver_count, num_str);
    console_puts(num_str);
    console_puts("\n\n");

    // Create a sorted queue of drivers (insertion sort by priority)
    driver_t* sorted_queue[MAX_DRIVERS];
    uint8_t queue_size = 0;

    // Build sorted queue
    for (uint8_t i = 0; i < driver_count; i++) {
        driver_t* drv = driver_registry[i];

        // Find insertion point (insert in order of priority, lowest first)
        uint8_t insert_pos = 0;
        while (insert_pos < queue_size && sorted_queue[insert_pos]->priority <= drv->priority) {
            insert_pos++;
        }

        // Shift everything after insertion point
        for (uint8_t j = queue_size; j > insert_pos; j--) {
            sorted_queue[j] = sorted_queue[j - 1];
        }

        // Insert driver
        sorted_queue[insert_pos] = drv;
        queue_size++;
    }

    // Initialize drivers in priority order, respecting dependencies
    uint8_t initialized_count = 0;
    uint8_t max_passes = driver_count * 2; // Prevent infinite loops
    uint8_t pass = 0;

    while (initialized_count < driver_count && pass < max_passes) {
        pass++;
        int progress_made = 0;

        for (uint8_t i = 0; i < queue_size; i++) {
            driver_t* drv = sorted_queue[i];

            // Skip if already processed
            if (drv->status != DRIVER_STATUS_UNINITIALIZED) {
                continue;
            }

            // Check dependencies
            if (!driver_dependencies_met(drv)) {
                continue;
            }

            // Initialize this driver
            console_puts("  [");
            uitoa(drv->priority, num_str);
            console_puts(num_str);
            console_puts("] ");
            console_puts(drv->name);
            console_puts(" (");
            console_puts(driver_type_name(drv->type));
            console_puts(")");

            // Pad for alignment
            size_t len = strlen(drv->name) + strlen(driver_type_name(drv->type)) + strlen(num_str);
            for (size_t j = len; j < 32; j++) {
                console_putc(' ');
            }

            kerr_t err = E_OK;
            if (drv->init) {
                console_puts("\n");  // New line for driver's own output
                err = drv->init(drv);
                console_puts("    Result: ");
            }

            if (err == E_OK) {
                drv->status = DRIVER_STATUS_INITIALIZED;
                console_puts_color("[OK]\n", CONSOLE_COLOR_SUCCESS);
                initialized_count++;
                progress_made = 1;
            } else {
                drv->status = DRIVER_STATUS_FAILED;
                console_puts_color("[FAILED: ", CONSOLE_COLOR_FAILURE);
                console_puts(k_strerror(err));
                console_puts("]\n");
                initialized_count++; // Count as processed
                progress_made = 1;
            }
        }

        // If we didn't make progress, we have circular dependencies or other issues
        if (!progress_made) {
            break;
        }
    }

    // Report any uninitialized drivers
    for (uint8_t i = 0; i < driver_count; i++) {
        if (driver_registry[i]->status == DRIVER_STATUS_UNINITIALIZED) {
            serial_debug_puts("  Warning: ");
            serial_debug_puts(driver_registry[i]->name);
            serial_debug_puts(" failed to initialize (dependency issue?)\n");
        }
    }

    console_putc('\n');
    return E_OK;
}

// List all registered drivers
void driver_list(void) {
    console_puts("\n=== Registered Drivers ===\n");
    console_puts("Name                Type            Status          Priority\n");
    console_puts("------------------------------------------------------------\n");

    for (uint8_t i = 0; i < driver_count; i++) {
        driver_t* drv = driver_registry[i];

        // Name
        console_puts(drv->name);
        size_t len = strlen(drv->name);
        for (size_t j = len; j < 20; j++) console_putc(' ');

        // Type
        console_puts(driver_type_name(drv->type));
        len = strlen(driver_type_name(drv->type));
        for (size_t j = len; j < 16; j++) console_putc(' ');

        // Status (with color)
        console_color_attr_t status_color = CONSOLE_COLOR_DEFAULT;
        switch (drv->status) {
            case DRIVER_STATUS_INITIALIZED:
            case DRIVER_STATUS_ENABLED:
                status_color = CONSOLE_COLOR_SUCCESS;
                break;
            case DRIVER_STATUS_FAILED:
                status_color = CONSOLE_COLOR_FAILURE;
                break;
            case DRIVER_STATUS_DISABLED:
                status_color = CONSOLE_COLOR_WARNING;
                break;
            default:
                status_color = CONSOLE_COLOR_DEFAULT;
        }

        console_puts_color(driver_status_name(drv->status), status_color);
        len = strlen(driver_status_name(drv->status));
        for (size_t j = len; j < 16; j++) console_putc(' ');

        // Priority
        char num_str[8];
        uitoa(drv->priority, num_str);
        console_puts(num_str);
        console_putc('\n');
    }

    console_putc('\n');
    console_puts("Total drivers: ");
    char num_str[8];
    uitoa(driver_count, num_str);
    console_puts(num_str);
    console_puts("\n\n");
}

// Get total driver count
uint8_t driver_get_count(void) {
    return driver_count;
}

// Helper function to get driver type name
const char* driver_type_name(driver_type_t type) {
    switch (type) {
        case DRIVER_TYPE_FUNDAMENTAL: return "Fundamental";
        case DRIVER_TYPE_BLOCK:       return "Block";
        case DRIVER_TYPE_CHAR:        return "Character";
        case DRIVER_TYPE_NETWORK:     return "Network";
        case DRIVER_TYPE_INPUT:       return "Input";
        case DRIVER_TYPE_TIMER:       return "Timer";
        case DRIVER_TYPE_FILESYSTEM:  return "Filesystem";
        case DRIVER_TYPE_VIDEO:       return "Video";
        case DRIVER_TYPE_AUDIO:       return "Audio";
        default:                      return "Unknown";
    }
}

// Helper function to get driver status name
const char* driver_status_name(driver_status_t status) {
    switch (status) {
        case DRIVER_STATUS_UNINITIALIZED: return "Uninitialized";
        case DRIVER_STATUS_INITIALIZED:   return "Initialized";
        case DRIVER_STATUS_ENABLED:       return "Enabled";
        case DRIVER_STATUS_DISABLED:      return "Disabled";
        case DRIVER_STATUS_FAILED:        return "Failed";
        default:                          return "Unknown";
    }
}