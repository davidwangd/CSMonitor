# Process-Monitor

A Terminal-UI monitor for cluster running processes using FXTUI framework.

## Project Overview

This project provides a TUI monitor that displays logs from running jobs. It compiles into a static library that can be linked with custom implementations of the monitoring interface.

### Core Interface

The library requires implementation of three functions:

```cpp
std::vector<std::string> get_job_list();                          // Get list of jobs to monitor
std::string get_job_stdout_file(const std::string &job);           // Return stdout file path (empty if N/A)
std::string get_job_stderr_file(const std::string &job);           // Return stderr file path (empty if N/A)
```

## Build Commands

```bash
# Configure (from project root)
cmake -B build -S .

# Build all
cmake --build build

# Build with release optimizations
cmake --build build --config Release

# Clean build
cmake --build build --target clean && cmake --build build
```

## Testing

```bash
# Run all tests
cd build && ctest --output-on-failure

# Run a specific test (replace TestName)
cd build && ctest -R TestName --output-on-failure

# Run tests with verbose output
cd build && ctest -V

# Run single test executable directly
./build/tests/test_monitor
```

## Linting and Static Analysis

```bash
# Run clang-format (if configured)
find src include -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Run clang-tidy (if configured)
clang-tidy build/compile_commands.json

# Run cppcheck
cppcheck --enable=all src/ include/
```

## Project Structure

```
CSMonitor/
├── CMakeLists.txt          # Main CMake configuration
├── include/                # Public headers
│   └── monitor/
│       └── monitor.hpp     # Main library interface
├── src/                    # Library implementation
│   ├── monitor.cpp
│   └── file_watcher.hpp
├── tests/                  # Test files
│   ├── CMakeLists.txt
│   └── test_monitor.cpp
├── third_party/            # External dependencies
│   └── ftxui/              # FTXUI library
└── examples/               # Example implementations
    ├── CMakeLists.txt
    └── example_monitor.cpp
```

## Code Style Guidelines

### C++ Version

- Use C++17 or later
- Use modern C++ features: auto, range-based for, smart pointers, std::optional

### Naming Conventions

- **Namespaces**: lowercase with underscores (`process_monitor`)
- **Classes/Structs**: PascalCase (`JobMonitor`, `FileWatcher`)
- **Functions/Methods**: snake_case (`get_job_list`, `read_file_contents`)
- **Variables**: snake_case (`job_list`, `stdout_path`)
- **Constants**: UPPER_SNAKE_CASE (`DEFAULT_TICK_INTERVAL`, `MAX_BUFFER_SIZE`)
- **Member variables**: snake_case with trailing underscore (`job_list_`, `tick_interval_`)
- **Private methods**: snake_case with leading underscore (`_update_jobs`, `_read_files`)

### Imports and Includes

- Order: standard library → third-party → project headers
- Use include guards: `#pragma once` for headers
- Include paths relative to project root: `#include "monitor/file_watcher.hpp"`

```cpp
// Standard library
#include <string>
#include <vector>
#include <memory>

// Third-party
#include <ftxui/component/component.hpp>

// Project headers
#include "monitor/file_watcher.hpp"
```

### Formatting

- Indent: 4 spaces (no tabs)
- Max line length: 100 characters
- Braces on same line (K&R style)
- Space after keywords: `if (`, `for (`, `while (`
- Pointer/reference: attached to type: `std::string&`, `int*`

### Error Handling

- Use exceptions for exceptional conditions
- Return `std::optional<T>` for values that may not exist
- Use `std::expected<T, E>` (C++23) or result types when available
- Check file operations and system calls
- Log errors before throwing/recovering

```cpp
std::optional<std::string> read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    // ... read and return contents
}
```

### Memory Management

- Prefer stack allocation
- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`)
- Never use raw `new`/`delete`
- Use RAII for resource management

### Best Practices

- **const correctness**: Mark methods and parameters as `const` when appropriate
- **pass by reference**: Use `const std::string&` for strings
- **pass by value**: Use move semantics for transferring ownership
- **avoid global state**: Use dependency injection
- **prefer composition over inheritance**

### File Watching Implementation

- Track file modifications by comparing content hashes or modification times
- Store previous state to detect changes
- Highlight updated files in the UI
- Handle file deletion/creation gracefully

### FXTUI Framework Notes

- FXTUI is included as a third-party library in `third_party/fxtui/`
- No internet required for building (vendored dependency)
- Use FXTUI components for UI elements
- Refresh rate controlled by tick interval (default 1 second)

## Testing Guidelines

- Test each interface function independently
- Create fake job files in temporary directories
- Use test fixtures for setup/teardown
- Test edge cases: empty file lists, missing files, large files

## Tick Configuration

Default: 1 second between updates. Override via command line:

```bash
./monitor --tick 500  # 500ms tick interval
./monitor -t 2000     # 2 second tick interval
```

## Debugging

```bash
# Build with debug symbols
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Run with GDB
gdb ./build/monitor

# Enable sanitizers
cmake -B build -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"
```
