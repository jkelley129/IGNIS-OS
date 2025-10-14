#ifndef RAMFS_H
#define RAMFS_H

#include "../vfs.h"
#include "../../error_handling/errno.h"

// RAM filesystem specific node structure
typedef struct ramfs_node {
    vfs_node_t* vfs_node;       // Back pointer to VFS node
    uint8_t* data;              // File data (NULL for directories)
    struct ramfs_node* first_child;  // First child (for directories)
    struct ramfs_node* next_sibling; // Next sibling
} ramfs_node_t;

// Initialize and create a RAM filesystem instance
kerr_t ramfs_create_fs(filesystem_t** fs_out);

// Destroy a RAM filesystem instance
kerr_t ramfs_destroy_fs(filesystem_t* fs);

#endif