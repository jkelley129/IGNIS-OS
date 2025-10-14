#include "ramfs.h"
#include "../../mm/memory.h"
#include "../../libc/string.h"

// Forward declarations of operations
static kerr_t ramfs_open(vfs_node_t* node);
static kerr_t ramfs_close(vfs_node_t* node);
static kerr_t ramfs_read(vfs_node_t* node, void* buffer, size_t size, size_t* bytes_read);
static kerr_t ramfs_write(vfs_node_t* node, const void* buffer, size_t size, size_t* bytes_written);
static kerr_t ramfs_node_create(vfs_node_t* parent, const char* name, file_type_t type, vfs_node_t** result);
static kerr_t ramfs_node_delete(vfs_node_t* node);
static kerr_t ramfs_readdir(vfs_node_t* node, uint32_t index, vfs_node_t** result);

static kerr_t ramfs_mount(filesystem_t* fs, const char* device);
static kerr_t ramfs_unmount(filesystem_t* fs);

// Operations table
static const vfs_operations_t ramfs_ops = {
        .open = ramfs_open,
        .close = ramfs_close,
        .read = ramfs_read,
        .write = ramfs_write,
        .create = ramfs_node_create,
        .delete = ramfs_node_delete,
        .readdir = ramfs_readdir
};

// Mount operation - create root directory
static kerr_t ramfs_mount(filesystem_t* fs, const char* device) {
    // Create root vfs_node
    vfs_node_t* root = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!root) return E_NOMEM;

    strcpy(root->name, "/");
    root->type = FILE_TYPE_DIRECTORY;
    root->size = 0;
    root->flags = 0;
    root->parent = NULL;
    root->fs = fs;
    root->ops = &ramfs_ops;

    // Create ramfs-specific data
    ramfs_node_t* ramfs_data = (ramfs_node_t*)kmalloc(sizeof(ramfs_node_t));
    if (!ramfs_data) {
        kfree(root);
        return E_NOMEM;
    }

    ramfs_data->vfs_node = root;
    ramfs_data->data = NULL;
    ramfs_data->first_child = NULL;
    ramfs_data->next_sibling = NULL;

    root->fs_data = ramfs_data;
    fs->root = root;

    return E_OK;
}

// Helper function to recursively free a node and all its children
static void ramfs_free_node_recursive(ramfs_node_t* node) {
    if (!node) return;

    // If it's a directory, free all children first
    if (node->vfs_node && node->vfs_node->type == FILE_TYPE_DIRECTORY) {
        ramfs_node_t* child = node->first_child;
        while (child) {
            ramfs_node_t* next = child->next_sibling;
            ramfs_free_node_recursive(child);
            child = next;
        }
    }

    // Free the data if it exists
    if (node->data) {
        kfree(node->data);
    }

    // Free the vfs_node
    if (node->vfs_node) {
        kfree(node->vfs_node);
    }

    // Free the ramfs_node itself
    kfree(node);
}

static kerr_t ramfs_unmount(filesystem_t* fs) {
    if (!fs || !fs->root) return E_INVALID;

    // Get the root's ramfs data
    ramfs_node_t* root_data = (ramfs_node_t*)fs->root->fs_data;

    // Recursively free all nodes
    if (root_data) {
        ramfs_free_node_recursive(root_data);
    }

    fs->root = NULL;
    return E_OK;
}

static kerr_t ramfs_open(vfs_node_t* node) {
    // Nothing special needed for ramfs
    return E_OK;
}

static kerr_t ramfs_close(vfs_node_t* node) {
    // Nothing special needed for ramfs
    return E_OK;
}

static kerr_t ramfs_read(vfs_node_t* node, void* buffer, size_t size, size_t* bytes_read) {
    if (!node || node->type != FILE_TYPE_REGULAR) return E_ISDIR;

    ramfs_node_t* ramfs_data = (ramfs_node_t*)node->fs_data;
    if (!ramfs_data || !ramfs_data->data) {
        *bytes_read = 0;
        return E_OK;
    }

    size_t to_read = (size < node->size) ? size : node->size;

    uint8_t* dst = (uint8_t*)buffer;
    for (size_t i = 0; i < to_read; i++) {
        dst[i] = ramfs_data->data[i];
    }

    *bytes_read = to_read;
    return E_OK;
}

static kerr_t ramfs_write(vfs_node_t* node, const void* buffer, size_t size, size_t* bytes_written) {
    if (!node || node->type != FILE_TYPE_REGULAR) return E_ISDIR;

    ramfs_node_t* ramfs_data = (ramfs_node_t*)node->fs_data;
    if (!ramfs_data) return E_INVALID;

    // Free old data if exists
    if (ramfs_data->data) {
        kfree(ramfs_data->data);
    }

    // Allocate new data
    ramfs_data->data = (uint8_t*)kmalloc(size);
    if (!ramfs_data->data) {
        node->size = 0;
        *bytes_written = 0;
        return E_NOMEM;
    }

    // Copy data
    const uint8_t* src = (const uint8_t*)buffer;
    for (size_t i = 0; i < size; i++) {
        ramfs_data->data[i] = src[i];
    }

    node->size = size;
    *bytes_written = size;
    return E_OK;
}

static kerr_t ramfs_node_create(vfs_node_t* parent, const char* name, file_type_t type, vfs_node_t** result) {
    if (!parent || parent->type != FILE_TYPE_DIRECTORY) return E_NOTDIR;

    ramfs_node_t* parent_data = (ramfs_node_t*)parent->fs_data;
    if (!parent_data) return E_INVALID;

    // Check if already exists
    ramfs_node_t* child = parent_data->first_child;
    while (child) {
        if (strcmp(child->vfs_node->name, name) == 0) {
            if (result) *result = child->vfs_node;
            return E_EXISTS;
        }
        child = child->next_sibling;
    }

    // Create new vfs_node
    vfs_node_t* new_node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!new_node) return E_NOMEM;

    strncpy(new_node->name, name, MAX_FILENAME - 1);
    new_node->name[MAX_FILENAME - 1] = '\0';
    new_node->type = type;
    new_node->size = 0;
    new_node->flags = 0;
    new_node->parent = parent;
    new_node->fs = parent->fs;
    new_node->ops = &ramfs_ops;

    // Create ramfs-specific data
    ramfs_node_t* new_ramfs_data = (ramfs_node_t*)kmalloc(sizeof(ramfs_node_t));
    if (!new_ramfs_data) {
        kfree(new_node);
        return E_NOMEM;
    }

    new_ramfs_data->vfs_node = new_node;
    new_ramfs_data->data = NULL;
    new_ramfs_data->first_child = NULL;
    new_ramfs_data->next_sibling = parent_data->first_child;

    new_node->fs_data = new_ramfs_data;
    parent_data->first_child = new_ramfs_data;

    if (result) *result = new_node;
    return E_OK;
}

static kerr_t ramfs_node_delete(vfs_node_t* node) {
    if (!node || !node->parent) return E_PERM;

    ramfs_node_t* ramfs_data = (ramfs_node_t*)node->fs_data;
    ramfs_node_t* parent_data = (ramfs_node_t*)node->parent->fs_data;

    if (!ramfs_data || !parent_data) return E_INVALID;

    // Remove from parent's child list
    if (parent_data->first_child == ramfs_data) {
        parent_data->first_child = ramfs_data->next_sibling;
    } else {
        ramfs_node_t* prev = parent_data->first_child;
        while (prev && prev->next_sibling != ramfs_data) {
            prev = prev->next_sibling;
        }
        if (prev) {
            prev->next_sibling = ramfs_data->next_sibling;
        }
    }

    // Free data
    if (ramfs_data->data) {
        kfree(ramfs_data->data);
    }

    kfree(ramfs_data);
    kfree(node);

    return E_OK;
}

static kerr_t ramfs_readdir(vfs_node_t* node, uint32_t index, vfs_node_t** result) {
    if (!node || node->type != FILE_TYPE_DIRECTORY) return E_NOTDIR;

    ramfs_node_t* ramfs_data = (ramfs_node_t*)node->fs_data;
    if (!ramfs_data) return E_INVALID;

    // Traverse to the index-th child
    ramfs_node_t* child = ramfs_data->first_child;
    uint32_t current = 0;

    while (child && current < index) {
        child = child->next_sibling;
        current++;
    }

    if (!child) return E_NOTFOUND;

    *result = child->vfs_node;
    return E_OK;
}

// Public API - Create a ramfs filesystem instance
kerr_t ramfs_create_fs(filesystem_t** fs_out) {
    filesystem_t* fs = (filesystem_t*)kmalloc(sizeof(filesystem_t));
    if (!fs) return E_NOMEM;

    strcpy(fs->name, "ramfs");
    fs->fs_private = NULL;
    fs->root = NULL;
    fs->mount = ramfs_mount;
    fs->unmount = ramfs_unmount;

    *fs_out = fs;
    return E_OK;
}

kerr_t ramfs_destroy_fs(filesystem_t* fs) {
    if (!fs) return E_INVALID;

    // Unmount will free all nodes
    if (fs->root) {
        ramfs_unmount(fs);
    }

    kfree(fs);
    return E_OK;
}