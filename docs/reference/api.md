# API Reference

This document covers the C++ API for Mirror Bridge, including the core concepts, binding functions, and macros.

## Core Concepts

Mirror Bridge uses C++20 concepts to classify types at compile-time.

### `Arithmetic`

Matches numeric types (int, float, double, etc.).

```cpp
template<typename T>
concept Arithmetic = std::is_arithmetic_v<std::remove_cvref_t<T>>;
```

### `StringLike`

Matches string types.

```cpp
template<typename T>
concept StringLike =
    std::is_same_v<std::remove_cvref_t<T>, std::string> ||
    std::is_same_v<std::remove_cvref_t<T>, std::string_view> ||
    std::is_same_v<std::remove_cvref_t<T>, const char*>;
```

### `Container`

Matches STL containers (vector, array, list, etc.).

```cpp
template<typename T>
concept Container = requires {
    { std::declval<T>().begin() } -> std::input_or_output_iterator;
    { std::declval<T>().end() } -> std::input_or_output_iterator;
    { std::declval<T>().size() } -> std::convertible_to<std::size_t>;
    typename std::remove_cvref_t<T>::value_type;
} && !StringLike<T> && !SmartPointer<T>;
```

### `SmartPointer`

Matches `unique_ptr` and `shared_ptr`.

```cpp
template<typename T>
concept SmartPointer = requires {
    typename std::remove_cvref_t<T>::element_type;
} && (/* is unique_ptr or shared_ptr */);
```

### `EnumType`

Matches enum and enum class types.

```cpp
template<typename T>
concept EnumType = std::is_enum_v<std::remove_cvref_t<T>>;
```

### `Bindable`

Matches classes that can be reflected and bound.

```cpp
template<typename T>
concept Bindable = std::is_class_v<std::remove_cvref_t<T>> && requires {
    { std::meta::nonstatic_data_members_of(^^T) };
};
```

## Python Bindings

### `MIRROR_BRIDGE_MODULE`

Main macro for creating a Python module.

```cpp
MIRROR_BRIDGE_MODULE(module_name,
    // binding code here
)
```

**Example:**
```cpp
MIRROR_BRIDGE_MODULE(my_module,
    mirror_bridge::bind_class<Calculator>(m, "Calculator");
    mirror_bridge::bind_class<Vector3>(m, "Vector3");
)
```

### `mirror_bridge::bind_class<T>`

Binds a C++ class to Python.

```cpp
template<Bindable T>
void bind_class(PyObject* module, const char* name);
```

**Parameters:**
- `module` - Python module object (provided as `m` in the macro)
- `name` - Name of the class in Python

**What it binds:**
- All public data members as properties
- All public non-static methods
- Default constructor
- Parameterized constructors (if detected)
- `__repr__` method (automatic)

**Example:**
```cpp
mirror_bridge::bind_class<Calculator>(m, "Calculator");
```

### Type Conversion Functions

#### `to_python<T>`

Converts C++ values to Python objects.

```cpp
template<Arithmetic T>
PyObject* to_python(const T& value);

template<StringLike T>
PyObject* to_python(const T& value);

template<Container T>
PyObject* to_python(const T& container);

template<SmartPointer T>
PyObject* to_python(const T& ptr);

template<EnumType T>
PyObject* to_python(const T& value);

template<Bindable T>
PyObject* to_python(const T& obj);
```

#### `from_python<T>`

Converts Python objects to C++ values.

```cpp
template<typename T>
T from_python(PyObject* obj);
```

## Lua Bindings

### `MIRROR_BRIDGE_LUA_MODULE`

Main macro for creating a Lua module.

```cpp
MIRROR_BRIDGE_LUA_MODULE(module_name,
    // binding code here
)
```

**Example:**
```cpp
MIRROR_BRIDGE_LUA_MODULE(my_module,
    mirror_bridge::lua::bind_class<Calculator>(L, "Calculator");
)
```

### `mirror_bridge::lua::bind_class<T>`

Binds a C++ class to Lua.

```cpp
template<Bindable T>
void bind_class(lua_State* L, const char* name);
```

## JavaScript Bindings

### `MIRROR_BRIDGE_JS_MODULE`

Main macro for creating a Node.js module.

```cpp
MIRROR_BRIDGE_JS_MODULE(module_name,
    // binding code here
)
```

**Example:**
```cpp
MIRROR_BRIDGE_JS_MODULE(my_module,
    mirror_bridge::javascript::bind_class<Calculator>(env, m, "Calculator");
)
```

### `mirror_bridge::javascript::bind_class<T>`

Binds a C++ class to JavaScript.

```cpp
template<Bindable T>
void bind_class(napi_env env, napi_value exports, const char* name);
```

## Reflection Utilities

### Member Discovery

```cpp
// Get number of data members
template<typename T>
consteval std::size_t get_data_member_count();

// Get data member at index
template<typename T, std::size_t I>
consteval auto get_data_member();

// Get number of methods
template<typename T>
consteval std::size_t get_member_function_count();

// Get method at index
template<typename T, std::size_t I>
consteval auto get_member_function();
```

### Method Introspection

```cpp
// Get method parameter count
template<typename T, std::size_t FuncIndex>
consteval std::size_t get_method_param_count();

// Get method parameter type
template<typename T, std::size_t FuncIndex, std::size_t ParamIndex>
consteval auto get_method_param_type();

// Get method return type
template<typename T, std::size_t FuncIndex>
consteval auto get_method_return_type();
```

### Type Signature Generation

```cpp
// Generate signature for change detection
template<Bindable T>
std::string generate_type_signature(const char* file_hash = nullptr);
```

## Registry

### `mirror_bridge::core::Registry`

Singleton registry for tracking bound classes.

```cpp
class Registry {
public:
    static Registry& instance();

    void register_class(const std::string& name,
                        const std::string& signature,
                        void* type_obj = nullptr);

    const ClassMetadata* get_class(const std::string& name) const;
    bool is_registered(const std::string& name) const;
};
```

## Version Information

```cpp
#define MIRROR_BRIDGE_VERSION_MAJOR 0
#define MIRROR_BRIDGE_VERSION_MINOR 2
#define MIRROR_BRIDGE_VERSION_PATCH 0

#define MIRROR_BRIDGE_HAS_REFLECTION 1
#define MIRROR_BRIDGE_HAS_ENUMERATORS_OF 1
#define MIRROR_BRIDGE_HAS_TYPE_SIGNATURES 1
```

## Include Headers

```cpp
// Python bindings (default)
#include "mirror_bridge.hpp"

// Or explicitly:
#include "python/mirror_bridge_python.hpp"

// Lua bindings
#include "lua/mirror_bridge_lua.hpp"

// JavaScript bindings
#include "javascript/mirror_bridge_javascript.hpp"

// Core only (no language bindings)
#include "core/mirror_bridge_core.hpp"
```
