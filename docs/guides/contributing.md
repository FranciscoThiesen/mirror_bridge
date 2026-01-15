# Contributing Guide

This guide covers everything you need to develop, test, and contribute to Mirror Bridge.

## Development Environment

### Quick Setup

```bash
# One command to get started
./start_dev_container.sh
```

First run takes ~2-60 minutes depending on whether you pull or build the image.

The Docker container includes:
- Bloomberg clang-p2996 with C++26 reflection support
- libc++ with reflection enabled (`<meta>` header)
- Python 3.10+ development headers
- Lua 5.4 development headers
- Node.js with N-API headers
- All your changes preserved between sessions

### Daily Workflow

```bash
# Attach to container
./start_dev_container.sh

# Inside container - run tests
cd /workspace
./tests/run_all_tests.sh

# Try examples
cd examples/01-hello-world
mirror_bridge generate src/ --module greeter --lang python
python3 test_greeter.py

# Exit (container stays running)
exit
```

### What's Persisted?

| Location | Persisted? |
|----------|-----------|
| `/workspace` (source code) | Yes |
| Compiled bindings | Yes |
| Installed packages | Yes |
| Shell history | Yes |
| `/tmp` contents | No |
| Running processes | No |

## Testing

### Run All Tests

```bash
./tests/run_all_tests.sh
```

Expected output: `ALL TESTS PASSED!`

### Test Structure

```
tests/
├── e2e/
│   ├── basic/           # Point2D, Vector3, Calculator
│   ├── containers/      # std::vector, std::array
│   ├── nesting/         # Nested classes, cross-file
│   └── advanced/        # New features
│       ├── variadic/    # Methods with 3-6 parameters
│       ├── constructors/# Parameterized constructors
│       ├── overloading/ # Method overloading
│       └── smart_ptrs/  # unique_ptr, shared_ptr
├── lua/                 # Lua-specific tests
├── js/                  # JavaScript-specific tests
└── run_all_tests.sh
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

**3. Test Script** (`test_my_class.py`):
```python
import sys
sys.path.insert(0, '../../build')
import my_module

obj = my_module.MyClass()
obj.value = 10.0
assert obj.compute(5, 3) == 8.0
print("Test passed!")
```

Tests are automatically discovered and run by `run_all_tests.sh`.

## Code Style & Conventions

### Modern C++ Practices

This library showcases modern C++26:

**Reflection (P2996R10 syntax):**
```cpp
auto members = std::meta::members_of(^^T, std::meta::access_context::current());
constexpr auto name = std::meta::identifier_of(member);
auto& value = obj.[:member:];  // Splicer
```

**Concepts:**
```cpp
template<Arithmetic T>
PyObject* to_python(const T& value);

template<SmartPointer T>
PyObject* to_python(const T& ptr);
```

**Variadic Templates:**
```cpp
([&] {
    process_parameter<Is>();
}(), ...);  // Fold expression
```

### Documentation Standards

- **Complex logic needs comments**: Explain WHY, not just WHAT
- **Use section headers**: Clearly delineate different parts
- **Document modern techniques**: Help readers learn from the code
- **Avoid redundant comments**: Don't comment self-documenting code

### Commit Guidelines

```bash
# Good commit messages
git commit -m "Add smart pointer support for unique_ptr and shared_ptr"
git commit -m "Fix template instantiation for nested bindable types"

# Bad commit messages
git commit -m "fix bug"
git commit -m "updates"
```

## Pull Request Process

1. Fork and create a feature branch
2. Make your changes with clear commits
3. Ensure all tests pass: `./tests/run_all_tests.sh`
4. Update documentation if needed
5. Submit PR with description of changes

### Areas Needing Work

- **Reference parameters**: Passing bound classes by reference
- **Template classes**: Automatic template instantiation detection
- **Const method overloads**: Distinguish const vs non-const methods
- **Additional language backends**: Ruby, Go, Rust
- **Python stub generation**: `.pyi` files for IDE support

## Project Structure

```
mirror_bridge/
├── core/                    # Language-agnostic reflection
├── python/                  # Python C API bindings
├── lua/                     # Lua C API bindings
├── javascript/              # Node.js N-API bindings
├── tools/                   # CLI tools
├── docs/                    # Documentation
├── examples/                # Progressive examples
├── tests/                   # Test suite
├── single_header/           # Amalgamated headers
└── benchmarks/              # Performance tests
```

## Common Issues

### "libc++.so.1: cannot open shared object file"

```bash
export LD_LIBRARY_PATH=/usr/local/lib/aarch64-unknown-linux-gnu:$LD_LIBRARY_PATH
```

The test runner sets this automatically.

### Container management

```bash
docker stop mirror_bridge_dev     # Stop container
docker rm mirror_bridge_dev       # Remove container
docker rmi mirror_bridge:latest   # Remove image
```

## License

Apache License 2.0 - See LICENSE for details.
