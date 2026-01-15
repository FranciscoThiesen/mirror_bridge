# Mirror Bridge Architecture

This document describes the internal architecture of mirror_bridge for developers, contributors, and AI assistants working with the codebase.

## Overview

Mirror Bridge is a **compile-time C++ binding generator** that uses C++26 reflection (P2996) to automatically create foreign function interfaces for Python, Lua, and JavaScript. The key principle is that **all binding logic is resolved at compile-time**, resulting in zero-overhead runtime performance.

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  C++ Header     │────▶│  C++26 Compiler  │────▶│  Native Module  │
│  (your code)    │     │  with Reflection │     │  (.so/.node)    │
└─────────────────┘     └──────────────────┘     └─────────────────┘
                               │
                        Compile-time:
                        - Member discovery
                        - Type introspection
                        - Binding generation
```

## Directory Structure

```
mirror_bridge/
├── core/
│   └── mirror_bridge_core.hpp      # Language-agnostic reflection utilities
├── python/
│   └── mirror_bridge_python.hpp    # Python C API bindings
├── lua/
│   └── mirror_bridge_lua.hpp       # Lua C API bindings
├── javascript/
│   ├── mirror_bridge_javascript.hpp # Node.js N-API bindings
│   └── mirror_bridge_v8.hpp        # Direct V8 bindings
├── mirror_bridge.hpp               # Main include (defaults to Python)
├── scripts/
│   └── discover_symbols.py         # Header parsing for class discovery
├── tools/
│   └── mirror_bridge               # Unified CLI wrapper
├── tests/                          # Comprehensive test suite
├── examples/                       # Usage examples
└── docs/                           # User documentation
```

## Core Components

### 1. Reflection Layer (`core/mirror_bridge_core.hpp`)

The foundation of mirror_bridge uses C++26 reflection APIs:

```cpp
// Discover class members at compile-time (P2996R10 syntax)
std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current())  // Data members
std::meta::members_of(^^T, std::meta::access_context::current())                  // All members
std::meta::identifier_of(member)            // Member name
std::meta::type_of(member)                  // Member type
std::meta::parameters_of(func)              // Function parameters
std::meta::return_type_of(func)             // Return type

// Splicers - inject reflected code back into source
obj.[:member:]  // Access member via reflection
```

**Compile-Time Caches** avoid O(N²) complexity during reflection:

```cpp
template<typename T>
struct DataMemberCache {
    static consteval std::size_t compute_count();
    static consteval auto get_at_index(std::size_t i);
};
// Similar caches for: MemberFunctionCache, StaticMemberFunctionCache, StaticDataMemberCache
```

### 2. Type Classification (Concepts)

Types are classified using C++20 concepts for clean overload resolution:

| Concept | Types | Python Mapping |
|---------|-------|----------------|
| `Arithmetic` | int, float, double, bool | int/float |
| `StringLike` | std::string, string_view, const char* | str |
| `Container` | std::vector, array, list, deque | list |
| `SmartPointer` | unique_ptr, shared_ptr | wrapper/dict |
| `EnumType` | enum, enum class | int |
| `Bindable` | Classes with reflectable members | class wrapper |
| `NestedBindable` | Bindable types used as members | dict |

### 3. Python Bindings (`python/mirror_bridge_python.hpp`)

**PyWrapper<T>** - Holds C++ objects in Python:

```cpp
template<typename T>
struct PyWrapper {
    PyObject_HEAD
    T* cpp_object;  // Pointer to actual C++ object
    bool owns;      // True if Python owns the object (should delete)
};
```

**bind_class<T>()** - The main binding function:

1. Generate type signature for change detection
2. Count members/methods via reflection
3. Generate property getters/setters (direct member access via splicers)
4. Generate method dispatch table
5. Create `PyTypeObject` with all slots filled
6. Register in module and type registries

**Type Conversion Functions**:

```cpp
// C++ → Python
template<typename T>
PyObject* to_python(const T& value);

// Python → C++
template<typename T>
bool from_python(PyObject* obj, T& out);
```

### 4. Lua Bindings (`lua/mirror_bridge_lua.hpp`)

Uses Lua userdata with metatables for property access:

```cpp
template<typename T>
struct LuaWrapper {
    T* cpp_object;
    bool owns;
};
// Metatables: __index, __newindex for property access
```

### 5. JavaScript Bindings

**Node.js N-API** (`mirror_bridge_javascript.hpp`):
- Uses `napi_define_properties` for property descriptors
- `napi_value` type conversions
- External data pointers for C++ storage

**Direct V8** (`mirror_bridge_v8.hpp`):
- For embedded V8 (non-Node.js) scenarios
- Direct V8 API usage

## Binding Generation Flow

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. DISCOVERY                                                     │
│    ├─ mirror_bridge_auto scans headers for classes              │
│    ├─ discover_symbols.py parses C++ syntax                     │
│    └─ Respects MIRROR_BRIDGE_SKIP markers                       │
├─────────────────────────────────────────────────────────────────┤
│ 2. CODE GENERATION                                               │
│    ├─ Generate binding .cpp with #includes                      │
│    ├─ MIRROR_BRIDGE_MODULE(name, bind_class<T>(...); ...)       │
│    └─ Output: module_binding.cpp                                │
├─────────────────────────────────────────────────────────────────┤
│ 3. COMPILATION (Bloomberg clang-p2996)                          │
│    ├─ -std=c++2c -freflection -freflection-latest              │
│    ├─ Compile-time: reflection metadata extraction              │
│    ├─ Compile-time: binding code generation via splicers        │
│    └─ Output: module.so                                         │
└─────────────────────────────────────────────────────────────────┘
```

## CLI Tools

| Tool | Purpose |
|------|---------|
| `mirror_bridge` | Unified CLI (generate, build, config, pch, init) |
| `mirror_bridge_auto` | Auto-discover classes in a directory |
| `mirror_bridge_generate` | Generate from .mirror config file |
| `mirror_bridge_build` | Compile pre-written binding.cpp |

## Key Design Decisions

### Header-Only Library
No build system complexity. Include and use with any build system.

### Concept-Based Type Classification
Clean overload resolution without SFINAE complexity:
```cpp
template<Arithmetic T>
PyObject* to_python(const T& val) { ... }

template<Container T>
PyObject* to_python(const T& val) { ... }
```

### Dict for Nested Objects
Nested C++ objects become Python dicts, not wrapper objects:
- Simpler mental model
- No need to pre-register nested types
- Works consistently across all languages

### Python-Based Global Type Registry
Cross-module type sharing uses Python's `sys.modules`:
```python
sys.modules['_mirror_bridge_types'][typeid_name] = PyTypeObject*
```
This works around C++ static variable per-.so limitations.

### Splicers for Zero-Overhead Access
Member access via `obj.[:member:]` compiles to direct offset access:
```cpp
// This at compile-time becomes equivalent to:
// return &(self->member_name);
return &(wrapper->cpp_object->[:get_data_member<T, Index>():]);
```

## Performance Characteristics

- **Compile time**: ~200-250ms with PCH, ~500-1700ms without
- **Runtime overhead**: Zero (direct function pointers, no vtables)
- **Binary size**: Comparable to hand-written bindings

## Extending Mirror Bridge

### Adding a New Language

1. Create `<lang>/mirror_bridge_<lang>.hpp`
2. Implement `Wrapper<T>` struct for FFI object storage
3. Implement `to_<lang>()` and `from_<lang>()` conversions
4. Create `MIRROR_BRIDGE_<LANG>_MODULE` macro
5. Add CLI variant `mirror_bridge_auto_<lang>`

### Adding New Type Support

1. Define concept in `core/mirror_bridge_core.hpp`
2. Add conversion overloads in each language binding
3. Update documentation

## Common Patterns

### Binding a Class

```cpp
#include "mirror_bridge.hpp"
#include "my_class.hpp"

MIRROR_BRIDGE_MODULE(mymodule,
    mirror_bridge::bind_class<MyClass>(m, "MyClass");
)
```

### Binding Free Functions

```cpp
mirror_bridge::bind_function<&my_function>(m, "my_function");
```

### Skipping a Class

```cpp
class MIRROR_BRIDGE_SKIP InternalClass {
    // Won't be discovered by mirror_bridge_auto
};
```

## Troubleshooting

### "No matching overload" errors
The type isn't recognized. Check if it matches one of the supported concepts.

### Cross-module type issues
Ensure both modules are loaded and using the same Python interpreter.

### Compile errors with reflection
Ensure you're using Bloomberg clang-p2996 with `-std=c++2c -freflection`.

## See Also

- `docs/getting-started/` - Quick start guides
- `docs/reference/` - CLI and API reference
- `examples/` - Working code examples
- `tests/` - Comprehensive test cases (useful as examples)
