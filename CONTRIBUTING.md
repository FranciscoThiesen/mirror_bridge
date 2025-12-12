# Contributing to Mirror Bridge

> **Note**: This document has moved to [docs/guides/contributing.md](docs/guides/contributing.md).
> This file is kept for backward compatibility.

This guide covers everything you need to develop, test, and contribute to Mirror Bridge.

## Development Environment

### Quick Setup

```bash
# One command to get started
./start_dev_container.sh
```

First run takes ~30-60 minutes to build clang-p2996. Subsequent runs are instant.

This creates a persistent Docker container with:
- Bloomberg clang-p2996 with C++26 reflection support
- libc++ with reflection enabled (`<meta>` header)
- Python 3.10+ development headers
- All your changes preserved between sessions

### Daily Workflow

```bash
# Attach to container (creates if needed, starts if stopped)
./start_dev_container.sh

# Inside container - run tests
cd /workspace
./tests/run_all_tests.sh

# Try examples
cd examples/option2
../../mirror_bridge_auto src/ --module math_module
python3 test_option2.py

# Exit (container stays running)
exit
```

### What's Persisted?

**✅ Inside Container:**
- Compiled bindings (`/workspace/build/`)
- Installed packages
- Shell history

**✅ On Host (via volume mount):**
- All `/workspace` files (source code, build artifacts, tests)

**❌ Lost on container restart:**
- Running processes
- `/tmp` contents

### Common Issues

**"libc++.so.1: cannot open shared object file"**
```bash
export LD_LIBRARY_PATH=/usr/local/lib/aarch64-unknown-linux-gnu:$LD_LIBRARY_PATH
```
The test runner (`run_all_tests.sh`) sets this automatically.

**Container management:**
```bash
docker stop mirror_bridge_dev     # Stop container
docker rm mirror_bridge_dev       # Remove container
docker rmi mirror_bridge:latest   # Remove image (forces rebuild)
```

## Testing

### Run All Tests

```bash
# Inside container
cd /workspace
./tests/run_all_tests.sh
```

Expected output: **✓ ALL TESTS PASSED! (12/12)**

### Test Structure

```
tests/e2e/
├── basic/           # Point2D, Vector3, Calculator
├── containers/      # std::vector, std::array
├── nesting/         # Nested classes, cross-file dependencies
└── advanced/        # New features
    ├── variadic/    # Methods with 3-6 parameters
    ├── constructors/# Parameterized constructors
    ├── overloading/ # Method overloading via name mangling
    └── smart_ptrs/  # unique_ptr, shared_ptr conversion
```

### Writing Tests

Each test consists of three files:

**1. C++ Header** (`my_class.hpp`):
```cpp
struct MyClass {
    double value;
    double compute(double x, double y);
};
```

**2. Binding File** (`my_class_binding.cpp`):
```cpp
#include "mirror_bridge.hpp"
#include "my_class.hpp"

MIRROR_BRIDGE_MODULE(my_module,
    mirror_bridge::bind_class<MyClass>(m, "MyClass");
)
```

**3. Python Test** (`test_my_class.py`):
```python
import sys
sys.path.insert(0, '../../build')
import my_module

obj = my_module.MyClass()
obj.value = 10.0
assert obj.compute(5, 3) == 8.0
print("✓ Test passed!")
```

Tests are automatically discovered and run by `run_all_tests.sh`.

### Advanced Test Features

See `tests/e2e/advanced/` for examples of:
- **Variadic parameters**: `add3(a, b, c)`, `sum5(a, b, c, d, e)`
- **Constructor matching**: Default, 2-param, 3-param constructors
- **Method overloading**: `print(int)`, `print(double)`, `print(string)`
- **Smart pointers**: `unique_ptr<T>`, `shared_ptr<T>` conversion to/from dicts

## CLI Tools Reference

Three tools for different use cases:

### 1. mirror_bridge_auto (Auto-Discovery)

**Best for:** Quick prototyping, binding entire libraries

```bash
mirror_bridge_auto src/ --module my_module [options]

Options:
  --module NAME       Python module name (required)
  -o, --output DIR    Output directory (default: build/)
  -k, --keep-generated Keep generated .cpp file
  -v, --verbose       Show compilation output
```

**Example:**
```bash
# Binds ALL classes in src/
mirror_bridge_auto src/ --module math
```

**Opt-out:**
```cpp
// MIRROR_BRIDGE_SKIP
struct InternalClass { /* won't be bound */ };
```

### 2. mirror_bridge_generate (Config File)

**Best for:** Production use, explicit control

```bash
mirror_bridge_generate config.mirror [options]

Options:
  -o, --output DIR    Output directory (default: build/)
  -k, --keep-generated Keep generated .cpp file
  -v, --verbose       Show compilation output
```

**Config file format** (`.mirror`):
```
module: my_module
include_dirs: src/, include/

# ClassName: header_file [as PythonName]
Calculator: calculator.hpp
Vector3: vector3.hpp
math::Complex: complex.hpp as Complex
```

### 3. mirror_bridge_build (Manual)

**Best for:** Custom binding logic, complex scenarios

```bash
mirror_bridge_build binding.cpp [options]

Options:
  -o, --output DIR    Output directory (default: build/)
  -I DIR              Add include directory
  -v, --verbose       Show compilation output
```

**Example:**
```bash
mirror_bridge_build my_bindings.cpp -I src/ -I include/
```

## Code Style & Conventions

### Modern C++ Practices

This library showcases modern C++26:

**Reflection:**
```cpp
// Use reflection for compile-time introspection
constexpr auto members = std::meta::members_of(^^T);
constexpr auto name = std::meta::identifier_of(member);

// Use splicers to inject reflected code
auto& value = obj.[:member:];
```

**Concepts:**
```cpp
// Prefer concepts over SFINAE when possible
template<Arithmetic T>
PyObject* to_python(const T& value);

template<SmartPointer T>
PyObject* to_python(const T& ptr);
```

**Variadic Templates:**
```cpp
// Use fold expressions for parameter pack expansion
([&] {
    process_parameter<Is>();
}(), ...);  // Fold expression
```

**Template Recursion:**
```cpp
// For compile-time iteration in consteval contexts
template<typename T, std::size_t Index, std::size_t CheckIndex = 0>
consteval bool is_overloaded() {
    if constexpr (CheckIndex >= count) return false;
    else if constexpr (/* condition */) return true;
    else return is_overloaded<T, Index, CheckIndex + 1>();
}
```

### Documentation

- **Complex logic needs comments**: Explain WHY, not just WHAT
- **Use section headers**: Clearly delineate different parts of the code
- **Document modern techniques**: Help readers learn from the code

### Commit Guidelines

```bash
# Good commit messages
git commit -m "Add smart pointer support for unique_ptr and shared_ptr"
git commit -m "Fix template instantiation for nested bindable types"

# Bad commit messages
git commit -m "fix bug"
git commit -m "updates"
```

## Architecture Overview

### Core Components

**1. Reflection Layer** (`mirror_bridge.hpp:680-730`)
- `std::meta::members_of(^^T)` - Discover class structure
- `std::meta::identifier_of()` - Get member names
- `std::meta::type_of()` - Get member types
- `[:member:]` splicers - Inject reflected code

**2. Type Conversion** (`mirror_bridge.hpp:75-215`)
- Concept-based function overloading
- `Arithmetic`, `StringLike`, `Container`, `SmartPointer` concepts
- Automatic dict ↔ nested class conversion

**3. Code Generation** (`mirror_bridge.hpp:900-1030`)
- Compile-time PyGetSetDef array generation
- Compile-time PyMethodDef array generation
- Method overload detection via template recursion

**4. Python C API Binding** (`mirror_bridge.hpp:1040-1100`)
- PyTypeObject creation with reflection metadata
- Automatic registration in Python module

### Zero Overhead Guarantee

All binding code is resolved at compile-time:
- **No vtables** - Direct function pointers
- **No RTTI** - No runtime type checking
- **No indirection** - Direct member offset access
- **No runtime reflection** - All via compile-time metaprogramming

## Contributing Guidelines

### Before Submitting

1. **Run all tests**: `./tests/run_all_tests.sh` must pass
2. **Add tests** for new features
3. **Document** complex reflection logic
4. **Follow** modern C++ conventions

### Pull Request Process

1. Fork and create a feature branch
2. Make your changes with clear commits
3. Ensure all tests pass
4. Update documentation if needed
5. Submit PR with description of changes

### Areas Needing Work

- **Reference parameters**: Passing bound classes by reference
- **Template classes**: Automatic template instantiation detection
- **Const method overloads**: Distinguish const vs non-const methods
- **Performance benchmarks**: Compare with pybind11, Boost.Python
- **Additional backends**: Rust, Lua support

## License

Apache License 2.0 - See LICENSE for details.
