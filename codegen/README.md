# Mirror Bridge C++17 Code Generator

This directory contains a **C++17 compatibility layer** for Mirror Bridge. It allows you to generate standalone binding code that can be compiled with any C++17 compiler (GCC, Clang, MSVC) - no reflection compiler needed at build time.

> **Note**: This is a workaround for users who cannot use the reflection compiler in their build system. The recommended approach is to use Mirror Bridge directly with the reflection-enabled compiler for the best experience.

## How It Works

1. **One-time code generation** (requires reflection compiler, e.g., in Docker)
2. **Normal C++17 build** (any standard compiler)

```
┌─────────────────────────────────────────────────────────────┐
│  Your C++ Headers (C++17)                                   │
│  e.g., calculator.hpp                                       │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  Codegen Driver (uses reflection)                           │
│  #include "codegen/mirror_bridge_codegen.hpp"               │
│  mirror_bridge::codegen::bind<Calculator>("Calculator")     │
└─────────────────────────────────────────────────────────────┘
                              │
            [reflection compiler - one time]
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  Generated C++17 Code                                       │
│  calculator_bindings.cpp (pure C++17, no reflection)        │
└─────────────────────────────────────────────────────────────┘
                              │
            [any C++17 compiler - normal build]
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  Python Extension Module                                    │
│  calculator.so                                              │
└─────────────────────────────────────────────────────────────┘
```

## Quick Start

### Step 1: Create a Codegen Driver

```cpp
// my_codegen.cpp
#include "codegen/mirror_bridge_codegen.hpp"
#include "my_classes.hpp"  // Your C++ headers

int main() {
    mirror_bridge::codegen::generate_module("mymodule",
        mirror_bridge::codegen::bind<MyClass>("MyClass"),
        mirror_bridge::codegen::bind<OtherClass>("OtherClass")
    );
    return 0;
}
```

### Step 2: Generate C++17 Code (using Docker)

```bash
# Run the codegen in the Mirror Bridge Docker container
docker run --rm -v $(pwd):/workspace -w /workspace \
    ghcr.io/franciscothiesen/mirror_bridge:latest \
    bash -c "
        clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
            -I/workspace my_codegen.cpp -o /tmp/codegen && \
        /tmp/codegen > mymodule_bindings.cpp
    "
```

### Step 3: Compile with Any C++17 Compiler

```bash
# GCC
g++ -std=c++17 -shared -fPIC \
    -I/path/to/your/headers \
    mymodule_bindings.cpp \
    -o mymodule.so \
    $(python3-config --includes --ldflags)

# Or Clang
clang++ -std=c++17 -shared -fPIC \
    -I/path/to/your/headers \
    mymodule_bindings.cpp \
    -o mymodule.so \
    $(python3-config --includes --ldflags)
```

### Step 4: Use from Python

```python
import mymodule

obj = mymodule.MyClass()
obj.some_method()
```

## Supported Features

| Feature | Supported |
|---------|-----------|
| Data members (get/set) | Yes |
| Methods (instance) | Yes |
| Methods (static) | Yes |
| Constructors (default) | Yes |
| Constructors (parameterized) | Yes |
| Primitive types (int, double, etc.) | Yes |
| std::string | Yes |
| std::vector<T> | Yes |
| Const methods | Yes |
| Void methods | Yes |
| Exception handling | Yes |

## Limitations

The C++17 codegen is simpler than the full reflection-based approach:

- **No automatic nested type conversion** - Methods taking user-defined types as parameters need manual handling
- **No inheritance support** - Each class is bound independently
- **No smart pointer support** - Use raw pointers or simple types
- **No std::function callbacks** - Use the main library for callback support

For these features, use the main Mirror Bridge library with the reflection compiler.

## File Structure

```
codegen/
├── README.md                    # This file
├── mirror_bridge_codegen.hpp    # The codegen header
├── mirror_bridge_codegen        # Convenience wrapper script
├── sample_class.hpp             # Example C++ class
├── codegen_driver.cpp           # Example codegen driver
├── test_codegen.sh              # Integration test script
└── test_codegen.py              # Python test for generated module
```

## Running Tests

```bash
# Inside the Docker container or with reflection compiler available
cd codegen
./test_codegen.sh
```

This will:
1. Compile the codegen driver with the reflection compiler
2. Generate C++17 binding code
3. Compile with g++ -std=c++17
4. Run Python tests

## Integration with CI/CD

You can integrate codegen into your workflow:

```yaml
# .github/workflows/build.yml
jobs:
  generate-bindings:
    runs-on: ubuntu-latest
    container: ghcr.io/franciscothiesen/mirror_bridge:latest
    steps:
      - uses: actions/checkout@v4
      - name: Generate bindings
        run: |
          clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
              my_codegen.cpp -o codegen
          ./codegen > generated_bindings.cpp
      - uses: actions/upload-artifact@v4
        with:
          name: generated-bindings
          path: generated_bindings.cpp

  build:
    needs: generate-bindings
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/download-artifact@v4
        with:
          name: generated-bindings
      - name: Build with standard compiler
        run: |
          g++ -std=c++17 -shared -fPIC generated_bindings.cpp \
              -o mymodule.so $(python3-config --includes --ldflags)
```

## Why Use This?

- Your build system doesn't support the reflection compiler
- You want to commit generated bindings to version control
- You need to build on platforms where the reflection compiler isn't available
- You're evaluating Mirror Bridge before fully adopting it

## When NOT to Use This

- You can use the reflection compiler directly (preferred)
- You need advanced features (callbacks, smart pointers, inheritance)
- You want the best compile-time performance
