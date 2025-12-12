# Architecture Overview

This document describes the internal architecture of Mirror Bridge, intended for contributors and those who want to understand how the system works.

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         User Interface Layer                            │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────┐  │
│  │  CLI Tool       │  │  Config Files   │  │  Direct C++ API         │  │
│  │  (mirror_bridge)│  │  (.mirror)      │  │  (bind_class<T>)        │  │
│  └────────┬────────┘  └────────┬────────┘  └───────────┬─────────────┘  │
└───────────┼────────────────────┼───────────────────────┼────────────────┘
            │                    │                       │
            ▼                    ▼                       ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         Binding Generation Layer                        │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Auto-Discovery Engine                         │   │
│  │  • Header scanning                                               │   │
│  │  • Class detection (struct/class)                                │   │
│  │  • Skip comment handling (MIRROR_BRIDGE_SKIP)                    │   │
│  │  • Binding code generation                                       │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         Core Reflection Layer                           │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                   mirror_bridge_core.hpp                         │   │
│  │                                                                   │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │   │
│  │  │  Concepts   │  │  Member     │  │  Type Signature         │  │   │
│  │  │  Arithmetic │  │  Caches     │  │  Generation             │  │   │
│  │  │  StringLike │  │  (compile-  │  │  (change detection)     │  │   │
│  │  │  Container  │  │   time)     │  │                         │  │   │
│  │  │  SmartPtr   │  │             │  │                         │  │   │
│  │  │  Bindable   │  │             │  │                         │  │   │
│  │  └─────────────┘  └─────────────┘  └─────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │
        ┌────────────────────────┼────────────────────────┐
        │                        │                        │
        ▼                        ▼                        ▼
┌───────────────────┐  ┌───────────────────┐  ┌───────────────────┐
│  Python Bindings  │  │   Lua Bindings    │  │   JS Bindings     │
│  ───────────────  │  │   ─────────────   │  │   ───────────     │
│                   │  │                   │  │                   │
│  Python C API     │  │  Lua C API        │  │  Node.js N-API    │
│  ─────────────    │  │  ──────────       │  │  ────────────     │
│  • PyWrapper<T>   │  │  • LuaWrapper<T>  │  │  • JsWrapper<T>   │
│  • to_python()    │  │  • to_lua()       │  │  • to_javascript()│
│  • from_python()  │  │  • from_lua()     │  │  • from_js()      │
│  • PyTypeObject   │  │  • Metatables     │  │  • napi_value     │
│                   │  │                   │  │                   │
└───────────────────┘  └───────────────────┘  └───────────────────┘
        │                        │                        │
        ▼                        ▼                        ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         C++26 Reflection (P2996)                        │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │  std::meta::members_of(^^T)        - Discover class members      │   │
│  │  std::meta::nonstatic_data_members_of(^^T) - Data members only   │   │
│  │  std::meta::identifier_of(member)  - Get member name             │   │
│  │  std::meta::type_of(member)        - Get member type             │   │
│  │  std::meta::parameters_of(func)    - Get function parameters     │   │
│  │  std::meta::return_type_of(func)   - Get return type             │   │
│  │  obj.[:member:]                    - Splicer (access member)     │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

## Component Details

### 1. Core Layer (`core/mirror_bridge_core.hpp`)

The language-agnostic foundation providing:

#### Type Concepts

```cpp
// Type classification at compile-time
template<typename T> concept Arithmetic;   // int, float, double...
template<typename T> concept StringLike;   // std::string, string_view...
template<typename T> concept Container;    // vector, array, list...
template<typename T> concept SmartPointer; // unique_ptr, shared_ptr
template<typename T> concept EnumType;     // enum, enum class
template<typename T> concept Bindable;     // Reflectable classes
```

#### Member Caches

Compile-time caches for O(1) member access:

```cpp
template<typename T>
struct MemberFunctionCache {
    static consteval std::size_t compute_count();
    static consteval auto get_at_index(std::size_t Index);
    static constexpr std::size_t count = compute_count();
};
```

This avoids repeated O(N) scans of `std::meta::members_of()` during binding generation.

#### Registry

Runtime registry for tracking bound classes:

```cpp
class Registry {
    void register_class(name, signature, type_obj);
    const ClassMetadata* get_class(name);
    bool is_registered(name);
};
```

### 2. Python Bindings (`python/mirror_bridge_python.hpp`)

Python-specific implementation (~1,984 lines):

#### PyWrapper<T>

Template class wrapping C++ objects for Python:

```cpp
template<typename T>
struct PyWrapper {
    PyObject_HEAD
    T* obj;
    bool owned;
};
```

#### Type Conversion

Overloaded functions for bidirectional conversion:

```cpp
// C++ → Python
template<Arithmetic T> PyObject* to_python(const T& value);
template<Container T>  PyObject* to_python(const T& container);

// Python → C++
template<typename T> T from_python(PyObject* obj);
```

#### Binding Generation

Compile-time generation of PyGetSetDef and PyMethodDef arrays:

```cpp
template<typename T>
static consteval auto generate_getset_defs();

template<typename T>
static consteval auto generate_method_defs();
```

### 3. Lua Bindings (`lua/mirror_bridge_lua.hpp`)

Lua-specific implementation (~667 lines):

- Metatable-based property access (`__index`, `__newindex`)
- Stack-based type conversion
- Userdata with custom metatables

### 4. JavaScript Bindings (`javascript/mirror_bridge_javascript.hpp`)

Node.js N-API implementation (~646 lines):

- N-API wrapper objects
- Property descriptors via `napi_define_properties`
- External data pointers for C++ object storage

## Data Flow

### Binding Generation Flow

```
1. User runs: mirror_bridge generate src/ --module my_mod --lang python

2. Auto-Discovery:
   ├── Scan *.hpp and *.h files in src/
   ├── Parse for struct/class definitions
   ├── Filter out MIRROR_BRIDGE_SKIP marked classes
   └── Generate binding code

3. Generated Code:
   ┌────────────────────────────────────────┐
   │ #include "mirror_bridge.hpp"           │
   │ #include "my_class.hpp"                │
   │                                        │
   │ MIRROR_BRIDGE_MODULE(my_mod,           │
   │     mirror_bridge::bind_class<MyClass> │
   │         (m, "MyClass");                │
   │ )                                      │
   └────────────────────────────────────────┘

4. Compilation:
   clang++ -std=c++2c -freflection ... → my_mod.so

5. At compile-time (inside bind_class<T>):
   ├── std::meta::nonstatic_data_members_of(^^T)
   │   → Discover data members
   ├── std::meta::members_of(^^T)
   │   → Discover methods
   ├── Generate PyGetSetDef[] for properties
   ├── Generate PyMethodDef[] for methods
   └── Create PyTypeObject and register
```

### Runtime Flow

```
Python code: obj.value = 42

1. Python calls setter (from PyGetSetDef)

2. Setter function:
   ├── Extract PyWrapper<T>* from Python object
   ├── Get T* from wrapper
   ├── Convert PyObject* (42) to C++ type
   └── Assign: cpp_obj->value = from_python<int>(py_value)

3. Return to Python
```

## Key Design Decisions

### 1. Header-Only Library

**Decision**: Single-header distribution for easy integration.

**Rationale**:
- Zero build system complexity for users
- Works with any build system
- Inspired by simdjson's approach

**Trade-off**: Larger compile times (mitigated by PCH).

### 2. Concept-Based Type System

**Decision**: Use C++20 concepts for type classification.

**Rationale**:
- Clean, readable constraints
- Better error messages than SFINAE
- Extensible (users can add specializations)

```cpp
template<Arithmetic T>
PyObject* to_python(const T& value) { ... }

template<Container T>
PyObject* to_python(const T& container) { ... }
```

### 3. Compile-Time Caching

**Decision**: Cache reflection results in consteval functions.

**Rationale**:
- `std::meta::members_of()` is O(N)
- Multiple passes would be O(N²)
- Caching gives O(1) access after first computation

### 4. Language-Specific Thin Layers

**Decision**: Core reflection logic shared; only FFI code differs.

**Rationale**:
- ~77% code reuse across languages
- Consistent behavior
- Easier maintenance

## Zero-Overhead Guarantee

All binding code is resolved at compile-time:

| Aspect | Implementation |
|--------|---------------|
| No vtables | Direct function pointers in PyMethodDef |
| No RTTI | Type info from reflection metadata |
| No indirection | Direct member offset access via splicers |
| No runtime reflection | All via compile-time metaprogramming |

## Extension Points

### Adding a New Language

1. Create `<lang>/mirror_bridge_<lang>.hpp`
2. Implement `to_<lang>()` and `from_<lang>()` conversions
3. Implement `Wrapper<T>` template
4. Create `MIRROR_BRIDGE_<LANG>_MODULE` macro
5. Add to amalgamation script

### Adding a New Type Category

1. Define new concept in `mirror_bridge_core.hpp`:
   ```cpp
   template<typename T>
   concept MyNewType = /* constraints */;
   ```

2. Add conversion overloads in each language binding:
   ```cpp
   template<MyNewType T>
   PyObject* to_python(const T& value);
   ```

## File Structure

```
mirror_bridge/
├── core/
│   └── mirror_bridge_core.hpp     # Language-agnostic core (476 lines)
├── python/
│   └── mirror_bridge_python.hpp   # Python bindings (1,984 lines)
├── lua/
│   └── mirror_bridge_lua.hpp      # Lua bindings (667 lines)
├── javascript/
│   └── mirror_bridge_javascript.hpp # JS bindings (646 lines)
├── mirror_bridge.hpp              # Default include (Python)
├── mirror_bridge_pch.hpp          # PCH wrapper
├── tools/
│   └── mirror_bridge              # Unified CLI
├── single_header/                 # Amalgamated headers
├── docs/                          # Documentation
├── examples/                      # Progressive examples
└── tests/                         # Test suite
```

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Member discovery | O(N) once | Cached at compile-time |
| Member access | O(1) | Direct offset |
| Method call | O(1) | Direct function pointer |
| Type conversion | O(1) - O(N) | Depends on type (containers are O(N)) |
| Module init | O(M) | M = number of bound classes |
