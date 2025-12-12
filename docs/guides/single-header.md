# Single-Header Distribution Guide

Mirror Bridge provides **single-header versions** for all three supported languages, making it easy to integrate into your projects. Just copy one file and start binding!

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

### 5. Use:

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

## File Sizes

| Language | File | Lines | Size |
|----------|------|-------|------|
| Python | `mirror_bridge_python.hpp` | ~1771 | ~65 KB |
| Lua | `mirror_bridge_lua.hpp` | ~859 | ~32 KB |
| JavaScript | `mirror_bridge_javascript.hpp` | ~875 | ~33 KB |

All files are self-contained with **zero external dependencies** (except language headers).

## What Gets Automatically Bound?

When you use `bind_class<T>()`, Mirror Bridge automatically creates bindings for:

- **All public data members** as properties (get/set)
- **All public methods** with full parameter passing
- **Const methods** (read-only operations)
- **Void methods** (no return value)
- **Methods with multiple parameters**
- **Return values** (numeric, strings, containers, etc.)
- **Containers** (vector, array, etc.)
- **Nested objects** (structs)
- **Enums** (converted to integers)

**No boilerplate required!** Just `bind_class<YourClass>(...)` and everything is bound.

## Multi-Header vs Single-Header

### Multi-Header (Original)
```
your_project/
├── mirror_bridge/
│   ├── core/mirror_bridge_core.hpp
│   ├── python/mirror_bridge_python.hpp
│   └── lua/mirror_bridge_lua.hpp
└── your_binding.cpp
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
└── your_binding.cpp
```

**Pros:**
- Copy just one file
- Simpler include paths
- Easier to distribute
- Less clutter

**Cons:**
- Slightly larger file size (but still small)

**Both produce identical compiled output!**

## Regenerating Single-Headers

If you modify the library source code, regenerate the single-headers:

```bash
./amalgamate.sh
```

This combines:
1. `core/mirror_bridge_core.hpp` (shared reflection engine)
2. Language-specific header (`python/`, `lua/`, or `javascript/`)

Into a single self-contained header for each language.

## Distribution Recommendations

| User Type | Recommendation |
|-----------|---------------|
| End users (using bindings) | Single-header versions |
| Library developers | Multi-header versions |
| Contributors | Multi-header + generate single-headers before releases |

## Compiler Requirements

**Required:**
- Bloomberg's clang-p2996 (C++26 reflection support)
- C++2c standard (`-std=c++2c`)
- Reflection flags: `-freflection -freflection-latest`
- libc++ standard library (`-stdlib=libc++`)

**Docker Available:**
```bash
./start_dev_container.sh
```
