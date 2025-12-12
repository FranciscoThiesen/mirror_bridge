# Mirror Bridge Multi-Language Support

> **Note**: This document has moved to [docs/guides/multi-language.md](docs/guides/multi-language.md).
> This file is kept for backward compatibility.

## Overview

Mirror Bridge now supports **three languages** using a single, elegant reflection-based architecture:

- **Python** (Python C API)
- **JavaScript** (Node.js N-API)
- **Lua** (Lua C API)

All three share the same core reflection infrastructure, ensuring consistent behavior and minimal code duplication.

## Architecture

```
mirror_bridge/
├── core/
│   └── mirror_bridge_core.hpp          # Language-agnostic reflection engine
│       ├── Type traits & concepts (Arithmetic, Container, StringLike, etc.)
│       ├── Reflection utilities (member/method discovery)
│       ├── Metadata registry
│       └── Member/method caches (compile-time optimization)
│
├── python/
│   └── mirror_bridge_python.hpp        # Python C API bindings
│       ├── to_python() / from_python() conversions
│       ├── PyWrapper<T> template
│       ├── Getter/setter/method binding
│       └── MIRROR_BRIDGE_MODULE macro
│
├── javascript/
│   └── mirror_bridge_javascript.hpp    # Node.js N-API bindings
│       ├── to_javascript() / from_javascript() conversions
│       ├── JsWrapper<T> template
│       ├── Property descriptors (napi_define_properties)
│       └── MIRROR_BRIDGE_JS_MODULE macro
│
├── lua/
│   └── mirror_bridge_lua.hpp           # Lua C API bindings
│       ├── to_lua() / from_lua() conversions
│       ├── LuaWrapper<T> template
│       ├── Metatable-based access (__index, __newindex)
│       └── MIRROR_BRIDGE_LUA_MODULE macro
│
└── mirror_bridge.hpp                   # Convenience header (includes Python by default)
```

## Key Design Principles

### 1. **Separation of Concerns**

- **Core**: Language-agnostic C++26 reflection logic
  - Type introspection
  - Member/method discovery
  - Signature generation
  - Compile-time caching

- **Language Bindings**: Language-specific FFI layer
  - Type conversion (C++ ↔ Language)
  - Object wrapping
  - Property/method access
  - Memory management

### 2. **Zero Code Duplication**

The core reflection logic is shared across all languages. Each language binding only implements:
- Type conversion functions
- FFI-specific wrapper templates
- Language-specific module macros

### 3. **Consistent API Surface**

All three languages expose the same C++ API:
- Data members as properties
- Methods with full type conversion
- Constructors (default + parameterized)
- Nested objects (as dicts/objects/tables)
- Containers (as lists/arrays/tables)

## Usage Examples

### Python

```cpp
#include "python/mirror_bridge_python.hpp"
#include "point.hpp"

MIRROR_BRIDGE_MODULE(point,
    mirror_bridge::bind_class<Point2D>(m, "Point2D");
)
```

```python
import point
p = point.Point2D()
p.x = 3.0
```

### JavaScript

```cpp
#include "javascript/mirror_bridge_javascript.hpp"
#include "point.hpp"

MIRROR_BRIDGE_JS_MODULE(point,
    mirror_bridge::javascript::bind_class<Point2D>(env, m, "Point2D");
)
```

```javascript
const point = require('./build/Release/point');
const p = new point.Point2D();
p.x = 3.0;
```

### Lua

```cpp
#include "lua/mirror_bridge_lua.hpp"
#include "point.hpp"

MIRROR_BRIDGE_LUA_MODULE(point,
    mirror_bridge::lua::bind_class<Point2D>(L, "Point2D");
)
```

```lua
local point = require("point")
local p = point.Point2D()
p.x = 3.0
```

## Build Instructions

### Python
```bash
clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -I. -fPIC -shared $(python3-config --includes --ldflags) \
    binding.cpp -o module.so
```

### JavaScript
```bash
# Requires node-gyp and binding.gyp configuration
node-gyp configure build
```

### Lua
```bash
clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -I. -I/usr/include/lua5.4 -fPIC -shared \
    binding.cpp -o module.so -llua5.4
```

## Type Conversion Matrix

| C++ Type | Python | JavaScript | Lua |
|----------|--------|------------|-----|
| `int`, `double` | `int`, `float` | `number` | `number` |
| `std::string` | `str` | `string` | `string` |
| `std::vector<T>` | `list` | `Array` | `table` (numeric) |
| `struct {...}` | `dict` | `Object` | `table` |
| `std::unique_ptr<T>` | Converted value | Converted value | Converted value |
| `enum` | `int` | `number` | `number` |

## Testing

All three languages use identical test structures:

```
tests/
├── python/
│   └── e2e/basic/point2d/
│       ├── point2d.hpp
│       ├── point2d_binding.cpp
│       └── test_point2d.py
│
├── js/
│   └── basic/point2d/
│       ├── point2d.hpp
│       ├── point2d_binding.cpp
│       ├── binding.gyp
│       └── test_point2d.js
│
└── lua/
    └── basic/point2d/
        ├── point2d.hpp
        ├── point2d_binding.cpp
        └── test_point2d.lua
```

## Performance

All three bindings achieve **zero runtime overhead** through compile-time code generation:

- **Python**: Direct C API calls (no pybind11 template overhead)
- **JavaScript**: N-API with minimal wrapper objects
- **Lua**: Lightweight metatable-based access

Compile-time performance is optimized via:
- Member/method caches (O(1) lookups instead of O(N) scans)
- Shared reflection infrastructure
- Template instantiation control

## Future Enhancements

Planned features:
1. **Amalgamation script** - Single-header versions for each language (Python-only, JS-only, Lua-only)
2. **More languages** - Rust FFI, R bindings, etc.
3. **Advanced features** - Async methods (JavaScript), coroutines (Lua), pickling (Python)
4. **Performance benchmarks** - Compare vs pybind11, node-addon-api, Sol2

## Credits

Inspired by:
- **pybind11** / **nanobind** - Python binding design patterns
- **Sol2** - Lua metatable-based property access
- **Node-addon-api** - Modern N-API wrapper patterns
- **simdjson** - Single-header library architecture with amalgamation

Powered by:
- **Bloomberg clang-p2996** - C++26 reflection implementation
- **C++26 Reflection (P2996)** - Compile-time introspection
