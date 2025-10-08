#ifndef VFS_H
#define VFS_H

#include "../libc/stdint.h"
#include "../libc/string.h"

#define MAX_FILENAME 64
#define MAX_PATH 256

// File types
typedef enum {
    FILE_TYPE_REGULAR,
    FILE_TYPE_DIRECTORY
} file_type_t;

// File structure
typedef struct file {
    char name[MAX_FILENAME];
    file_type_t type;
    uint8_t* data;           // File contents (NULL for directories)
    size_t size;             // File size in bytes
    struct file* next;       // Next file in same directory
    struct file* first_child; // First child (for directories)
    struct file* parent;     // Parent directory
} file_t;

// VFS operations
void vfs_init();
file_t* vfs_create_file(const char* path);
file_t* vfs_create_directory(const char* path);
file_t* vfs_open(const char* path);
int vfs_write(file_t* file, const void* data, size_t size);
int vfs_read(file_t* file, void* buffer, size_t size);
int vfs_delete(const char* path);
void vfs_list(const char* path);
void vfs_print_tree(file_t* dir, int depth);
int vfs_copy_file(const char* dest, file_t* source, size_t size);

// Helper functions
file_t* vfs_resolve_path(const char* path);
const char* vfs_basename(const char* path);
char* vfs_dirname(const char* path);

#endif