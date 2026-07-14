# Multi-Language Support

Mirror Bridge supports **three languages** using a single, elegant reflection-based architecture:

- **Python** (Python C API)
- **JavaScript** (Node.js N-API)
- **Lua** (Lua C API)

All three share the same core reflection infrastructure, ensuring consistent behavior and minimal code duplication.

## Quick Start

### Same C++, Multiple Languages

Write your C++ once:

```cpp
// calculator.hpp
struct Calculator {
    double value = 0.0;
    double add(double x) { return value += x; }
    double subtract(double x) { return value -= x; }
    std::string to_string() const {
        return "Calculator(value=" + std::to_string(value) + ")";
    }
};
```

Generate bindings for each language:

```bash
# Python
mirror_bridge generate src/ --module calc --lang python

# Lua
mirror_bridge generate src/ --module calc --lang lua

# JavaScript
mirror_bridge generate src/ --module calc --lang js

# All at once
mirror_bridge generate src/ --module calc --lang all
```

### Language Usage

**Python:**
```python
import calc
c = calc.Calculator()
c.add(10.0)
c.subtract(3.0)
print(c.value)        # 7.0
print(c.to_string())  # "Calculator(value=7.000000)"
```

**Lua:**
```lua
-- Lua modules are emitted to build/lua/ (Python owns build/<name>.so)
package.cpath = "build/lua/?.so;" .. package.cpath
local calc = require("calc")
local c = calc.Calculator()
c:add(10.0)
c:subtract(3.0)
print(c.value)        -- 7.0
print(c:to_string())  -- "Calculator(value=7.000000)"
```

**JavaScript:**
```javascript
const calc = require('./build/calc');
const c = new calc.Calculator();
c.add(10.0);
c.subtract(3.0);
console.log(c.value);        // 7.0
console.log(c.to_string());  // "Calculator(value=7.000000)"
```

## Type Conversion Matrix

| C++ Type | Python | JavaScript | Lua |
|----------|--------|------------|-----|
| `int`, `double`, `float` | `int`, `float` | `number` | `number` |
| `std::string` | `str` | `string` | `string` |
| `bool` | `bool` | `boolean` | `boolean` |
| `std::vector<T>` | `list` | `Array` | `table` |
| `std::array<T, N>` | `list` | `Array` | `table` |
| `struct {...}` | `dict` | `Object` | `table` |
| `std::unique_ptr<T>` | Converted value | Converted value | Converted value |
| `enum` / `enum class` | `int` | `number` | `number` |

## Architecture

```
mirror_bridge/
├── core/
│   └── mirror_bridge_core.hpp          # Language-agnostic reflection engine
│       ├── Type traits & concepts
│       ├── Reflection utilities
│       ├── Metadata registry
│       └── Member/method caches
│
├── python/
│   └── mirror_bridge_python.hpp        # Python C API bindings
│
├── javascript/
│   └── mirror_bridge_javascript.hpp    # Node.js N-API bindings
│
└── lua/
    └── mirror_bridge_lua.hpp           # Lua C API bindings
```

### Key Design Principles

**1. Separation of Concerns**
- **Core**: Language-agnostic C++26 reflection logic
- **Language Bindings**: Language-specific FFI layer

**2. Zero Code Duplication**
The core reflection logic is shared across all languages. Each language binding only implements:
- Type conversion functions
- FFI-specific wrapper templates
- Language-specific module macros

**3. Consistent API Surface**
All three languages expose the same C++ API:
- Data members as properties
- Methods with full type conversion
- Constructors (default + parameterized)
- Nested objects
- Containers

## Manual Binding (Advanced)

For fine-grained control, you can write manual binding files:

**Python:**
```cpp
#include "python/mirror_bridge_python.hpp"
#include "calculator.hpp"

MIRROR_BRIDGE_MODULE(calc,
    mirror_bridge::bind_class<Calculator>(m, "Calculator");
)
```

**Lua:**
```cpp
#include "lua/mirror_bridge_lua.hpp"
#include "calculator.hpp"

MIRROR_BRIDGE_LUA_MODULE(calc,
    mirror_bridge::lua::bind_class<Calculator>(L, "Calculator");
)
```

**JavaScript:**
```cpp
#include "javascript/mirror_bridge_javascript.hpp"
#include "calculator.hpp"

MIRROR_BRIDGE_JS_MODULE(calc,
    mirror_bridge::javascript::bind_class<Calculator>(env, m, "Calculator");
)
```

## Build Commands

**Python:**
```bash
clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -I. -fPIC -shared $(python3-config --includes --ldflags) \
    binding.cpp -o module.so
```

**Lua:**
```bash
clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -I. -I/usr/include/lua5.4 -fPIC -shared \
    binding.cpp -o module.so -llua5.4
```

**JavaScript:**
```bash
clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -I. -I/usr/include/node -fPIC -shared \
    binding.cpp -o module.node
```

## Container Support

**C++:**
```cpp
struct VectorTest {
    std::vector<int> numbers;
    std::vector<std::string> names;
    int get_sum() const;
    int count_names() const;
};
```

**Python:**
```python
v = VectorTest()
v.numbers = [1, 2, 3, 4, 5]
v.names = ["Alice", "Bob"]
print(v.get_sum())     # 15
print(v.count_names()) # 2
```

**Lua:**
```lua
v = VectorTest()
v.numbers = {1, 2, 3, 4, 5}
v.names = {"Alice", "Bob"}
print(v:get_sum())     -- 15
print(v:count_names()) -- 2
```

**JavaScript:**
```javascript
const v = new VectorTest();
v.numbers = [1, 2, 3, 4, 5];
v.names = ["Alice", "Bob"];
console.log(v.get_sum());     // 15
console.log(v.count_names()); // 2
```

## Nested Object Support

**C++:**
```cpp
struct Address {
    std::string street;
    std::string city;
    int zip;
};

struct Person {
    std::string name;
    int age;
    Address address;
};
```

All languages convert nested objects to native dictionaries/objects/tables with full read/write access.

## Performance

All three bindings achieve **zero runtime overhead** through compile-time code generation:

- **Python**: Direct C API calls
- **JavaScript**: N-API with minimal wrapper objects
- **Lua**: Lightweight metatable-based access

Compile-time performance is optimized via:
- Member/method caches (O(1) lookups)
- Shared reflection infrastructure
- Template instantiation control
