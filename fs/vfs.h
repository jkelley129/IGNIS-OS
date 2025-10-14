#ifndef VFS_H
#define VFS_H

#include "../libc/stdint.h"
#include "../libc/string.h"
#include "../error_handling/errno.h"

#define MAX_FILENAME 64
#define MAX_PATH 256
#define MAX_MOUNTS 8

// File types
typedef enum {
    FILE_TYPE_REGULAR,
    FILE_TYPE_DIRECTORY
} file_type_t;

// Forward declarations
struct vfs_node;
struct filesystem;

// File operations structure (function pointers for filesystem implementations)
typedef struct {
    kerr_t (*open)(struct vfs_node* node);
    kerr_t (*close)(struct vfs_node* node);
    kerr_t (*read)(struct vfs_node* node, void* buffer, size_t size, size_t* bytes_read);
    kerr_t (*write)(struct vfs_node* node, const void* buffer, size_t size, size_t* bytes_written);
    kerr_t (*create)(struct vfs_node* parent, const char* name, file_type_t type, struct vfs_node** result);
    kerr_t (*delete)(struct vfs_node* node);
    kerr_t (*readdir)(struct vfs_node* node, uint32_t index, struct vfs_node** result);
} vfs_operations_t;

// VFS node (represents a file or directory)
typedef struct vfs_node {
    char name[MAX_FILENAME];
    file_type_t type;
    size_t size;
    uint32_t flags;

    struct vfs_node* parent;
    struct filesystem* fs;      // Which filesystem owns this node
    void* fs_data;              // Filesystem-specific data

    const vfs_operations_t* ops; // Operations for this node
} vfs_node_t;

// Filesystem type structure
typedef struct filesystem {
    char name[32];              // e.g., "ramfs", "ext4"
    void* fs_private;           // Filesystem-specific data
    vfs_node_t* root;           // Root of this filesystem

    // Filesystem-level operations
    kerr_t (*mount)(struct filesystem* fs, const char* device);
    kerr_t (*unmount)(struct filesystem* fs);
} filesystem_t;

// Mount point structure
typedef struct {
    char path[MAX_PATH];        // Where it's mounted (e.g., "/", "/mnt")
    filesystem_t* fs;           // The filesystem mounted here
    uint8_t in_use;
} mount_point_t;

// VFS public interface
kerr_t vfs_init(void);
kerr_t vfs_mount(filesystem_t* fs, const char* path);
kerr_t vfs_unmount(const char* path);

// File operations (these dispatch to the appropriate filesystem)
vfs_node_t* vfs_open(const char* path);
kerr_t vfs_close(vfs_node_t* node);
kerr_t vfs_read(vfs_node_t* node, void* buffer, size_t size, size_t* bytes_read);
kerr_t vfs_write(vfs_node_t* node, const void* buffer, size_t size, size_t* bytes_written);
kerr_t vfs_create_file(const char* path);
kerr_t vfs_create_directory(const char* path);
kerr_t vfs_delete(const char* path);

// Directory operations
kerr_t vfs_list(const char* path);
void vfs_print_tree(vfs_node_t* node, int depth);

// Helper functions
vfs_node_t* vfs_resolve_path(const char* path);
const char* vfs_basename(const char* path);
char* vfs_dirname(const char* path);

// Copy operation
kerr_t vfs_copy_file(const char* dest, const char* source);

#endif