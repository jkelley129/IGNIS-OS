# Contributing to IGNIS OS

Thank you for your interest in contributing to IGNIS OS! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [How to Contribute](#how-to-contribute)
- [Coding Standards](#coding-standards)
- [Commit Guidelines](#commit-guidelines)
- [Pull Request Process](#pull-request-process)
- [Testing](#testing)
- [Documentation](#documentation)
- [Areas for Contribution](#areas-for-contribution)

## Code of Conduct

### Our Pledge

IGNIS OS is a learning project where everyone should feel welcome to contribute and learn. We pledge to make participation a harassment-free experience for everyone.

### Expected Behavior

- Be respectful and constructive
- Accept feedback graciously
- Focus on what's best for the project
- Show empathy toward other contributors

## Getting Started

### Prerequisites
```bash
# Required tools
- GCC (x86_64 cross-compiler recommended)
- NASM (assembler)
- GNU Make
- GRUB (grub-mkrescue)
- QEMU (for testing)

# Optional but recommended
- GDB (debugging)
- clangd (code completion)
```

### Building IGNIS
```bash
# Clone the repository
git clone https://github.com/jkelley129/IGNIS-OS.git
cd IGNIS-OS

# Build the OS
make

# Run in QEMU
make run

# Run with disk support
make disks
make run-full
```

## Development Setup

### Recommended IDE Setup

**VS Code Extensions:**
```
- C/C++ (Microsoft)
- clangd (better code completion)
- x86 and x86_64 Assembly (for .asm files)
- Makefile Tools
```

**VS Code Settings (.vscode/settings.json):**
```json
{
  "C_Cpp.intelliSenseEngine": "Disabled",
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}",
    "--header-insertion=never"
  ]
}
```

### Directory Structure
```
IGNIS-OS/
├── boot/          # Bootloader and early initialization
├── kernel/        # Core kernel functionality
├── drivers/       # Device drivers
├── mm/            # Memory management
├── fs/            # Filesystem code
├── shell/         # Command-line interface
├── libc/          # C standard library implementations
└── error/         # Error handling
```

## How to Contribute

### Types of Contributions

1. **Bug Reports**
    - Use GitHub Issues
    - Include reproduction steps
    - Provide system information
    - Include serial.log if available

2. **Feature Requests**
    - Describe the feature
    - Explain the use case
    - Consider implementation approach

3. **Code Contributions**
    - Bug fixes
    - New features
    - Documentation improvements
    - Test additions

### Important Note for Contributors

**IGNIS is a learning project.** The maintainer (Josh Kelley) wants to implement core features personally to learn. Please:

- ✅ **DO**: Submit bug fixes, small improvements, documentation
- ✅ **DO**: Suggest features and implementation approaches
- ✅ **DO**: Contribute to testing and debugging
- ⚠️ **DISCUSS FIRST**: Large features, architectural changes
- ❌ **AVOID**: Implementing major features without prior discussion

If you want to contribute a large feature, please open an issue first to discuss the implementation approach.

## Coding Standards

### C Code Style
```c
// File header
/*
 * filename.c - Brief description
 * 
 * Detailed description if needed.
 */

// Includes - grouped and sorted
#include "kernel_headers.h"
#include "../libc/string.h"

// Constants - ALL_CAPS
#define MAX_BUFFER_SIZE 256
#define PAGE_SIZE 4096

// Types - snake_case with _t suffix
typedef struct memory_block {
    size_t size;
    uint8_t is_free;
    struct memory_block* next;
} memory_block_t;

// Functions - snake_case
kerr_t function_name(uint64_t param) {
    // Opening brace on same line
    if (condition) {
        // 4-space indentation
        do_something();
    }
    
    return E_OK;
}

// Error handling - check and return
if (!pointer) {
    return E_INVALID;
}

kerr_t err = some_function();
if (err != E_OK) {
    return err;
}
```

### Assembly Style
```nasm
; Comments start with semicolon
; Labels at column 0
global function_name
extern external_function

function_name:
    ; Instructions indented
    push rbp
    mov rbp, rsp
    
    ; Align data
    sub rsp, 16
    
    ; Call convention - System V AMD64
    ; Args: rdi, rsi, rdx, rcx, r8, r9
    
    mov rsp, rbp
    pop rbp
    ret
```

### Naming Conventions
```
Files:          lowercase_with_underscores.c
Functions:      function_name()
Types:          type_name_t
Constants:      CONSTANT_NAME
Globals:        g_variable_name (avoid if possible)
Static:         static_variable (file scope only)
Macros:         MACRO_NAME
```

### Comments
```c
// Good comments explain WHY, not WHAT
// ✓ Good
// Wait for BSY bit to clear before sending command
ata_wait_busy(base);

// ✗ Bad
// Call ata_wait_busy function
ata_wait_busy(base);

// Function documentation
/**
 * Allocate a page of physical memory
 * 
 * Returns: Physical address of allocated page, or 0 on failure
 */
uint64_t pmm_alloc_page(void);
```

## Commit Guidelines

### Commit Message Format
```
<type>(<scope>): <subject>

<body>

<footer>
```

### Types

- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `test`: Adding/modifying tests
- `chore`: Build process, tools, etc.

### Examples
```
feat(pmm): Add page allocation statistics

Added functions to track and report memory usage:
- pmm_get_used_pages()
- pmm_get_free_pages()
- pmm_print_stats()

This helps with debugging memory issues.

---

fix(ata): Fix timeout in read operation

The ATA read function was timing out on some drives.
Increased timeout from 100ms to 1000ms and added
retry logic.

Fixes #42

---

docs(readme): Update build instructions

Added section on installing dependencies for Ubuntu 22.04.
```

## Pull Request Process

### Before Submitting

1. **Test your changes**
```bash
   make clean
   make
   make run
```

2. **Check for warnings**
```bash
   make 2>&1 | grep warning
```

3. **Update documentation** if needed

4. **Add tests** if applicable

### PR Checklist

- [ ] Code follows the style guidelines
- [ ] Commit messages are descriptive
- [ ] Documentation is updated
- [ ] No compiler warnings
- [ ] Tested in QEMU
- [ ] Serial log checked for errors
- [ ] Related issue linked (if applicable)

### PR Description Template
```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Documentation update
- [ ] Refactoring
- [ ] Other (specify)

## Testing
How was this tested?

## Checklist
- [ ] Code builds without errors
- [ ] No new compiler warnings
- [ ] Tested in QEMU
- [ ] Documentation updated
- [ ] Follows coding standards

## Related Issues
Fixes #(issue number)
```

## Testing

### Manual Testing
```bash
# Basic functionality
make run

# Test with disk
make disks
make run-full

# Run specific tests
# In IGNIS shell:
ignis$ memtest
ignis$ pagetest
ignis$ blktest 0
```

### Debug Mode
```bash
# Run with debug output
make run-debug

# GDB debugging
make run-gdb
# In another terminal:
gdb iso/boot/kernel.elf
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

### Checking Logs
```bash
# Serial output
cat serial.log

# QEMU log
cat qemu.log  # (if run with -D qemu.log)
```

## Documentation

### Code Documentation
```c
/**
 * Brief function description
 * 
 * Detailed description if needed. Explain parameters,
 * return values, side effects, and any preconditions.
 * 
 * @param param1 Description of param1
 * @param param2 Description of param2
 * @return Description of return value
 * 
 * @note Any important notes
 * @warning Any warnings
 */
kerr_t function_name(uint64_t param1, void* param2);
```

### Documentation Files

- **README.md**: Project overview, quick start
- **ARCHITECTURE.md**: System design, components
- **MEMORY_LAYOUT.md**: Memory organization
- **CONTRIBUTING.md**: This file
- **COMMANDS.md**: Shell command reference

### Updating Documentation

When adding features:
1. Update relevant .md files
2. Add inline code comments
3. Update shell/COMMANDS.md if adding commands
4. Consider adding examples

## Areas for Contribution

### Beginner-Friendly

- [ ] Documentation improvements
- [ ] Code comments
- [ ] Shell command help text
- [ ] Testing and bug reports
- [ ] README improvements

### Intermediate

- [ ] Bug fixes
- [ ] New shell commands
- [ ] Driver improvements
- [ ] Test utilities
- [ ] Error handling improvements

### Advanced

- [ ] Memory allocator optimization
- [ ] New device drivers
- [ ] Filesystem implementations
- [ ] Performance improvements
- [ ] Security enhancements

### Current Priorities

See the [GitHub Issues](https://github.com/jkelley129/IGNIS-OS/issues) page for:
- Bugs to fix
- Features to implement
- Documentation needs

## Communication

### Getting Help

- **GitHub Issues**: For bugs and feature requests
- **Discussions**: For questions and general discussion
- **Pull Requests**: For code contributions

### Response Times

This is a hobby project maintained by one person (primarily). Please be patient:
- Issues: Reviewed within a week
- PRs: Reviewed within 1-2 weeks
- Questions: Answered as time permits

## License

By contributing to IGNIS OS, you agree that your contributions will be licensed under the Apache License 2.0.

## Recognition

Contributors will be acknowledged in:
- Git commit history
- Release notes (for significant contributions)
- README.md contributors section (future)

## Questions?

If you have questions about contributing:
1. Check existing documentation
2. Search closed issues
3. Open a new issue with the "question" label

---

**Thank you for contributing to IGNIS OS!** 🔥

Your contributions help make this project better and provide learning opportunities for everyone involved.

---

*Last Updated: January 2025*
*IGNIS OS Version: 0.0.01*