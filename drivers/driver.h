#ifndef DRIVER_H
#define DRIVER_H

#include "libc/stdint.h"
#include "error_handling/errno.h"

#define MAX_DRIVERS 32
#define DRIVER_NAME_MAX 32

// Driver types
typedef enum {
    DRIVER_TYPE_UNKNOWN = 0,
    DRIVER_TYPE_FUNDAMENTAL,
    DRIVER_TYPE_BLOCK,      // Block device drivers (ATA, NVME, etc.)
    DRIVER_TYPE_CHAR,       // Character device drivers
    DRIVER_TYPE_NETWORK,    // Network drivers
    DRIVER_TYPE_INPUT,      // Input drivers (keyboard, mouse)
    DRIVER_TYPE_TIMER,      // Timer drivers (PIT, APIC timer)
    DRIVER_TYPE_FILESYSTEM, // Filesystem drivers
    DRIVER_TYPE_VIDEO,      // Video/display drivers
    DRIVER_TYPE_AUDIO,      // Audio drivers
} driver_type_t;

// Driver status flags
typedef enum {
    DRIVER_STATUS_UNINITIALIZED = 0,
    DRIVER_STATUS_INITIALIZED = 1,
    DRIVER_STATUS_ENABLED = 2,
    DRIVER_STATUS_DISABLED = 3,
    DRIVER_STATUS_FAILED = 4,
} driver_status_t;

// Forward declaration
struct driver;

// Driver structure
typedef struct driver {
    char name[DRIVER_NAME_MAX];     // Driver name (e.g., "ATA", "keyboard")
    driver_type_t type;              // Driver type
    uint32_t version;                // Driver version (for future use)
    uint8_t priority;                // Initialization priority (0 = highest)
    driver_status_t status;          // Current driver status

    // Function pointers
    kerr_t (*init)(struct driver* drv);      // Initialize driver
    kerr_t (*cleanup)(struct driver* drv);   // Cleanup driver

    // Optional dependency (name of driver this depends on)
    char depends_on[DRIVER_NAME_MAX];

    // Driver-specific data
    void* driver_data;
} driver_t;

// Driver registry API
kerr_t driver_registry_init(void);
kerr_t driver_register(driver_t* driver);
kerr_t driver_unregister(const char* name);
driver_t* driver_get_by_name(const char* name);
uint8_t driver_get_by_type(driver_type_t type, driver_t** drivers, uint8_t max_count);
kerr_t driver_init_all(void);
void driver_list(void);
uint8_t driver_get_count(void);

// Helper function to get driver type name
const char* driver_type_name(driver_type_t type);
const char* driver_status_name(driver_status_t status);

#endif // DRIVER_H