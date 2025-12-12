# Mirror Bridge - Single-Header Distribution Guide

> **Note**: This document has moved to [docs/guides/single-header.md](docs/guides/single-header.md).
> This file is kept for backward compatibility.

## Overview

Mirror Bridge now provides **single-header versions** for all three supported languages, making it incredibly easy to integrate into your projects. Just copy one file and start binding!

---

## Quick Start

### 1. Get the single-header file you need:

```bash
# Python
cp single_header/mirror_bridge_python.hpp /your/project/

# Lua
cp single_header/mirror_bridge_lua.hpp /your/project/

# JavaScript
cp single_header/mirror_bridge_javascript.hpp /your/project/
```

### 2. Create your C++ class:

```cpp
// robot.hpp
#pragma once
#include <string>

struct Robot {
    std::string name;
    double battery = 100.0;

    void move_forward(double distance) {
        battery -= distance * 0.1;
    }

    std::string get_status() const {
        return name + " (battery: " + std::to_string(battery) + "%)";
    }
};
```

### 3. Create bindings (3 lines!):

**Python:**
```cpp
#include "mirror_bridge_python.hpp"
#include "robot.hpp"

MIRROR_BRIDGE_MODULE(robot,
    mirror_bridge::bind_class<Robot>(m, "Robot");
)
```

**Lua:**
```cpp
#include "mirror_bridge_lua.hpp"
#include "robot.hpp"

MIRROR_BRIDGE_LUA_MODULE(robot,
    mirror_bridge::lua::bind_class<Robot>(L, "Robot");
)
```

**JavaScript:**
```cpp
#include "mirror_bridge_javascript.hpp"
#include "robot.hpp"

MIRROR_BRIDGE_JS_MODULE(robot,
    mirror_bridge::javascript::bind_class<Robot>(env, m, "Robot");
)
```

### 4. Compile:

**Python:**
```bash
clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -fPIC -shared $(python3-config --includes --ldflags) \
    binding.cpp -o robot.so
```

**Lua:**
```bash
clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -I/usr/include/lua5.4 -fPIC -shared \
    binding.cpp -o robot.so -llua5.4
```

**JavaScript:**
```bash
clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -I/usr/include/node -fPIC -shared \
    binding.cpp -o robot.node
```

### 5. Use in your target language:

**Python:**
```python
import robot
r = robot.Robot()
r.name = "RoboBot"
r.move_forward(50.0)
print(r.get_status())  # "RoboBot (battery: 95.0%)"
```

**Lua:**
```lua
local robot = require("robot")
local r = robot.Robot()
r.name = "RoboBot"
r:move_forward(50.0)
print(r:get_status())  -- "RoboBot (battery: 95.0%)"
```

**JavaScript:**
```javascript
const robot = require('./robot');
const r = new robot.Robot();
r.name = "RoboBot";
r.move_forward(50.0);
console.log(r.get_status());  // "RoboBot (battery: 95.0%)"
```

---

## File Sizes

| Language | File | Lines | Size |
|----------|------|-------|------|
| Python | `mirror_bridge_python.hpp` | ~1771 | ~65 KB |
| Lua | `mirror_bridge_lua.hpp` | ~859 | ~32 KB |
| JavaScript | `mirror_bridge_javascript.hpp` | ~875 | ~33 KB |

All files are self-contained with **zero external dependencies** (except language headers).

---

## What Gets Automatically Bound?

When you use `bind_class<T>()`, Mirror Bridge automatically creates bindings for:

✅ **All public data members** as properties (get/set)
✅ **All public methods** with full parameter passing
✅ **Const methods** (read-only operations)
✅ **Void methods** (no return value)
✅ **Methods with multiple parameters**
✅ **Return values** (numeric, strings, containers, etc.)
✅ **Containers** (vector, array, etc. → native arrays/lists/tables)
✅ **Nested objects** (structs → native objects/tables)
✅ **Enums** (converted to integers)

**No boilerplate required!** Just `bind_class<YourClass>(...)` and everything is bound.

---

## Supported Features

### Types

| C++ Type | Python | Lua | JavaScript |
|----------|--------|-----|------------|
| `int`, `double`, `float` | `int`, `float` | `number` | `number` |
| `std::string` | `str` | `string` | `string` |
| `bool` | `bool` | `boolean` | `boolean` |
| `std::vector<T>` | `list` | `table` | `Array` |
| `enum` / `enum class` | `int` | `number` | `number` |
| Nested structs | `dict` | `table` | `Object` |

### Methods

- **0 parameters**: ✅ `double get_value() const`
- **1 parameter**: ✅ `void set_value(double x)`
- **Multiple parameters**: ✅ `double compute(double x, double y)`
- **Void return**: ✅ `void reset()`
- **String return**: ✅ `std::string to_string() const`
- **Const methods**: ✅ All const methods work

---

## Tested Examples

All single-header versions have been tested with:

**Robot Class** (15 tests):
- Properties: `name`, `battery`, `commands` (vector)
- Methods: `move_forward()`, `get_status()`, `add_command()`, `command_count()`, `reset()`
- Features: properties, methods, containers, multiple params, void methods, string returns

✅ **Python**: 5/5 tests passing
✅ **Lua**: 5/5 tests passing
✅ **JavaScript**: 5/5 tests passing

Test files available in `tests/single_header_test/`.

---

## Compiler Requirements

**Required:**
- Bloomberg's clang-p2996 (C++26 reflection support)
- C++2c standard (`-std=c++2c`)
- Reflection flags: `-freflection -freflection-latest`
- libc++ standard library (`-stdlib=libc++`)

**Docker Available:**
```bash
# We provide a Docker image with everything pre-configured
./start_dev_container.sh
```

---

## Comparison: Multi-Header vs Single-Header

### Multi-Header (Original)
```
your_project/
├── mirror_bridge/
│   ├── core/mirror_bridge_core.hpp
│   ├── python/mirror_bridge_python.hpp
│   └── lua/mirror_bridge_lua.hpp
└── your_binding.cpp  (includes "../mirror_bridge/python/...")
```

**Pros:**
- Clearer separation of concerns
- Easier to develop/maintain the library
- Can switch languages easily

**Cons:**
- Need to copy entire directory structure
- More complex include paths

### Single-Header (Amalgamated)
```
your_project/
├── mirror_bridge_python.hpp  (single file!)
└── your_binding.cpp           (includes "mirror_bridge_python.hpp")
```

**Pros:**
- ✅ Copy just one file
- ✅ Simpler include paths
- ✅ Easier to distribute
- ✅ Less clutter

**Cons:**
- Slightly larger file size (but still small)

**Both produce identical compiled output!**

---

## Regenerating Single-Headers

If you modify the library source code, regenerate the single-headers:

```bash
./amalgamate.sh
```

This combines:
1. `core/mirror_bridge_core.hpp` (shared reflection engine)
2. Language-specific header (`python/`, `lua/`, or `javascript/`)

Into a single self-contained header for each language.

---

## Distribution Recommendations

### For End Users (Using Your Bindings)
✅ **Use single-header versions**
- Easiest to integrate
- Just drop in and compile
- Perfect for releases

### For Library Developers (Working on Mirror Bridge)
✅ **Use multi-header versions**
- Better organization
- Easier to modify
- Clear separation

### For Contributors
✅ **Use multi-header versions + generate single-headers before releases**
- Develop with multi-header
- Run `./amalgamate.sh` before publishing
- Test both versions

---

## Examples

Complete working examples available in:
- `tests/single_header_test/` - Robot class with Python, Lua, JavaScript
- `tests/comprehensive_features/` - Containers, enums, nested objects
- `tests/auto_discovery/` - Auto-discovery with Point and Circle classes

All examples work with both single-header and multi-header versions.

---

## Support

**Verified Working:**
- ✅ Python 3.x (tested on 3.10)
- ✅ Lua 5.4
- ✅ Node.js (tested with system node)

**Platforms:**
- ✅ Linux (aarch64, x86_64)
- ✅ Docker (provided Dockerfile)

**Build Time:**
- ~2-5 seconds per binding (with reflection)
- Zero runtime overhead (compile-time only)

---

## Next Steps

1. **Copy the single-header** for your language to your project
2. **Create your C++ class** with public members/methods
3. **Add 3 lines of binding code** using `bind_class<>()`
4. **Compile** with the provided command
5. **Use** in your target language!

That's it! Your C++ class is now available in Python/Lua/JavaScript with full property access, method calls, and type conversion.

---

## License

Mirror Bridge is open-source. See LICENSE file for details.

## Credits

Powered by:
- **C++26 Reflection (P2996)** - Compile-time introspection
- **Bloomberg clang-p2996** - Reflection implementation

Inspired by:
- **simdjson** - Single-header amalgamation approach
- **pybind11/nanobind** - Python binding patterns
- **Sol2** - Lua metatable approach
- **N-API** - Node.js addon patterns
