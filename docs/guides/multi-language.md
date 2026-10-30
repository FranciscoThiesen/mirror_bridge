# Multi-Language Support

Mirror Bridge supports **four languages** using a single, elegant reflection-based architecture:

- **Python** (Python C API)
- **JavaScript** (Node.js N-API)
- **Lua** (Lua C API)
- **C#** (P/Invoke over a generated flat C ABI)

All four share the same core reflection infrastructure, ensuring consistent
behavior and minimal code duplication. C# is the one that cannot call into a
reflection-compiled translation unit directly, because every P/Invoke target
needs its own exported symbol and reflection cannot declare a function whose
name is computed; it therefore emits its C ABI as text and compiles that in a
second, ordinary pass. See [the CLI reference](../reference/cli.md) for what
that means in practice.

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

# C#
mirror_bridge generate src/ --module calc --lang csharp -o out/

# All at once -- python, lua and js. C# is not included, because its output is
# a .cs source file to add to a dotnet project rather than a module the CLI can
# import and verify.
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

**C#:** add `out/calc.cs` to a project and put `libcalc.so` where the runtime
can find it. Names are the C++ names; every bound object is disposable, because
each one owns a native pointer.

```csharp
using calc;

using var c = new Calculator();
c.add(10.0);
c.subtract(3.0);
Console.WriteLine(c.value);        // 7
Console.WriteLine(c.to_string());  // Calculator(value=7.000000)
```

## Type Conversion Matrix

| C++ Type | Python | JavaScript | Lua | C# |
|----------|--------|------------|-----|----|
| `int`, `double`, `float` | `int`, `float` | `number` | `number` | `int`, `double`, `float` |
| `std::string` | `str` | `string` | `string` | `string` |
| `bool` | `bool` | `boolean` | `boolean` | `bool` |
| `std::vector<T>` | `list` | `Array` | `table` | an `IList<T>` view; `IReadOnlyList<T>` inbound |
| `std::array<T, N>` | `list` | `Array` | `table` | a fixed-size `IList<T>` view; `T[]` when returned by value |
| `struct {...}` | `dict` | `Object` | `table` | a generated class, held by `SafeHandle` |
| `std::unique_ptr<T>` | Converted value | Converted value | Converted value | not supported |
| `enum` / `enum class` | `int` | `number` | `number` | a generated `enum` |

The C# column differs in kind, not only in spelling: the other three backends
**convert** a container to a native value, while C# hands back a *view* of the
C++ object, so writing through it writes through to C++ and the view is
disposable. `std::map`/`std::unordered_map` is an `IDictionary<K,V>` view, and
`std::optional<T>` is `T?` for a scalar or an enum (`optional<string>` is still
plain `string` until the generated source carries nullable annotations).

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
All four languages expose the same C++ API:
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

**C#:** there is no single command, because generation is two stages. The
reflection pass is an executable that *writes* a flat C shim, the C# source and
a C header; a second, ordinary compile turns the shim into the shared library.
The CLI does both:

```bash
mirror_bridge generate include/ --module mylib --lang csharp -o out/
# out/mylib.cs  out/mylib_shim.cpp  out/mylib.h  out/libmylib.so
```

Run it by hand only to see the seam:

```bash
clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -I. generator.cpp -o gen && ./gen        # writes the three files
c++ -std=c++17 -fPIC -shared -O2 mylib_shim.cpp -o libmylib.so
```

The second line needs no reflection support at all, which is what makes the
generated `.h` usable from plain C or C++17 — see
[issue #3](https://github.com/FranciscoThiesen/mirror_bridge/issues/3).

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

**C#:**
```csharp
using var v = new VectorTest();
// A container member is a live view of the C++ object, not a snapshot, so
// there is no whole-container setter: writes through the view are the C++
// writes. Add a C++ setter method if you want to replace one wholesale.
v.numbers.Add(1); v.numbers.Add(2); v.numbers.Add(3);
v.names.Add("Alice"); v.names.Add("Bob");
Console.WriteLine(v.get_sum());        // 6
Console.WriteLine(v.count_names());    // 2
```

A container also crosses *inward*. The parameter type is `IReadOnlyList<T>`, so
a generated view satisfies it directly and one object's member reaches another
object's method with no copy on the managed side:

```csharp
// given  double Stats::mean(const std::vector<double>&) const;
Console.WriteLine(stats.mean(v.numbers));
```

A container of strings works the same way — `IReadOnlyList<string>`, and a
plain `string[]` or `List<string>` is accepted too.

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
