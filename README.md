# Mirror Bridge

**Modern C++ meets Multiple Languages: Automatic bindings using C++26 reflection - zero boilerplate, pure compile-time magic.**

> ⚠️ **EXPERIMENTAL**: This project requires C++26 reflection (P2996), which is not yet standardized. It only works with [Bloomberg's clang-p2996 fork](https://github.com/bloomberg/clang-p2996). Not recommended for production use until P2996 lands in standard C++26.

## One C++ Class, Three Languages

```cpp
// Write your C++ code once
struct Calculator {
    double value = 0.0;
    double add(double x) { return value += x; }
    double subtract(double x) { return value -= x; }
};
```

**Python:**
```python
import cpp_calc
calc = cpp_calc.Calculator()
calc.add(10)
calc.subtract(3)
print(calc.value)  # 7.0
```

**JavaScript (Node.js):**
```javascript
const calc = new addon.Calculator();
calc.add(10);
calc.subtract(3);
console.log(calc.x);  // 7.0
```

**Lua:**
```lua
local calc = cpp_calc.Calculator()
calc:add(10)
calc:subtract(3)
print(calc.value)  -- 7.0
```

**No manual binding code. No wrapper macros. Just pure C++26 reflection.** 🎉

## Supported Languages

| Language | Status | API | Example |
|----------|--------|-----|---------|
| **Python** | ✅ Stable | Python C API | `import my_module` |
| **JavaScript** | ✅ Stable | Node.js N-API | `const mod = require('my_module')` |
| **Lua** | ✅ Stable | Lua C API | `local mod = require("my_module")` |

## What is Mirror Bridge?

Mirror Bridge is a **header-only library** that uses C++26 reflection (P2996) to automatically introspect your C++ classes at compile-time and generate bindings for Python, JavaScript, and Lua. It discovers:

- ✅ **Data members** - automatic getters/setters with type safety
- ✅ **Methods** (any number of parameters) - variadic parameter support
- ✅ **Constructors** - including parameterized constructors
- ✅ **Method overloading** - automatic name mangling for overloads
- ✅ **Smart pointers** - `std::unique_ptr`, `std::shared_ptr` with automatic conversion
- ✅ **Nested classes** - recursive handling, cross-file dependencies
- ✅ **Cross-module types** - types from separate `.so` files work together seamlessly
- ✅ **Containers** - `std::vector`, `std::array` with bidirectional conversion
- ✅ **Exception handling** - C++ exceptions → Python exceptions
- ✅ **Enums** - automatic conversion to/from Python int
- ✅ **Object representation** - automatic `__repr__` implementation
- ✅ **Inheritance** - reflection automatically discovers inherited members

**Zero overhead:** All binding code is generated at compile-time through template metaprogramming and reflection - no runtime costs.

## Quick Start

```bash
# 1. Get the environment
./start_dev_container.sh
# Choose option 1 to pull pre-built image (~2 min)

# 2. Inside container - verify it works
cd /workspace && ./tests/run_all_tests.sh

# 3. Try an example
cd examples/option2
../../mirror_bridge_auto src/ --module math_module
python3 test_option2.py
```

**That's it!** See [QUICKSTART.md](QUICKSTART.md) for a detailed walkthrough.

## Two Approaches to Auto-Generation

Mirror Bridge offers two workflows optimized for different use cases:

### Option 1: Auto-Discovery (Minimal Friction)

**Just point at a directory - bindings are auto-generated for all classes.**

**Python:**
```bash
mirror_bridge_auto src/ --module my_module
```

**Lua:**
```bash
mirror_bridge_auto_lua src/ --module my_module
```

**JavaScript (Node.js):**
```bash
mirror_bridge_auto_js src/ --module my_module
```

- ✅ **Zero configuration** - discovers all classes automatically
- ✅ **Perfect for prototyping** and small projects
- ✅ **Opt-out via comments** - mark classes to skip
- ✅ **Works for all three languages** - Python, Lua, JavaScript

**Example:**
```cpp
// src/calculator.hpp
struct Calculator {
    double value;
    double add(double x);
};

// src/vector3.hpp
struct Vector3 {
    double x, y, z;
    double length();
};
```

```bash
# One command binds BOTH classes
mirror_bridge_auto src/ --module mylib
```

```python
import mylib
calc = mylib.Calculator()
vec = mylib.Vector3()
```

See [`examples/option2/`](examples/option2) for full example.

### Option 2: Config File (Production)

**Declarative config for explicit control over what gets bound.**

Create `my_module.mirror`:
```
module: my_module

include_dirs: src/, include/

Calculator: calculator.hpp
Vector3: vector3.hpp
```

```bash
mirror_bridge_generate my_module.mirror
```

- ✅ **Explicit control** - only bind what you specify
- ✅ **Version control friendly** - declarative config
- ✅ **Class renaming** - `Foo::Bar: foo.hpp as Bar`

See [`examples/option3/`](examples/option3) for full example.

**Full comparison:** [examples/README.md](examples/README.md)

## Single-Header Distribution

For easier integration, Mirror Bridge provides **single-header amalgamated versions** for each language. Just copy one file to your project!

**Generate single-headers:**
```bash
./amalgamate.sh
```

This creates:
- `single_header/mirror_bridge_python.hpp` (~1771 lines, 65KB)
- `single_header/mirror_bridge_lua.hpp` (~859 lines, 32KB)
- `single_header/mirror_bridge_javascript.hpp` (~875 lines, 33KB)

**Usage:**
```cpp
// Instead of: #include "python/mirror_bridge_python.hpp"
// Just:
#include "mirror_bridge_python.hpp"  // Single self-contained header!

MIRROR_BRIDGE_MODULE(my_module,
    mirror_bridge::bind_class<MyClass>(m, "MyClass");
)
```

See [SINGLE_HEADER_GUIDE.md](SINGLE_HEADER_GUIDE.md) for complete documentation.

## What Gets Auto-Generated?

### Data Members
```cpp
struct Point { double x, y; };
```
```python
p = my_module.Point()
p.x = 3.0  # Automatic getter/setter
p.y = 4.0
```

### Methods (Variadic Parameters)
```cpp
struct MathOps {
    double add3(double a, double b, double c) { return a + b + c; }
    double sum5(double a, double b, double c, double d, double e) {
        return a + b + c + d + e;
    }
    void reset() { value = 0; }  // Zero parameters work too
};
```
```python
ops = my_module.MathOps()
ops.add3(1.0, 2.0, 3.0)           # 3 parameters ✓
ops.sum5(1, 2, 3, 4, 5)           # 5 parameters ✓
ops.reset()                        # 0 parameters ✓
# ANY number of parameters supported through variadic templates
```

### Constructors with Parameters
```cpp
struct Rectangle {
    Rectangle() : width(0), height(0) {}
    Rectangle(double w, double h) : width(w), height(h) {}
    Rectangle(double w, double h, std::string name)
        : width(w), height(h), name(name) {}

    double width, height;
    std::string name;
};
```
```python
r1 = my_module.Rectangle()              # Default constructor
r2 = my_module.Rectangle(10.0, 5.0)    # 2-parameter constructor
r3 = my_module.Rectangle(10, 5, "box") # 3-parameter constructor
# Automatic constructor discovery and parameter matching
```

### Method Overloading
```cpp
struct Printer {
    void print(int value) { /* ... */ }
    void print(double value) { /* ... */ }
    void print(std::string value) { /* ... */ }
};
```
```python
p = my_module.Printer()
p.print_int(42)                    # int overload
p.print_double(3.14)               # double overload
p.print_std__string("hello")       # string overload
# Automatic name mangling distinguishes overloads
```

### Smart Pointers
```cpp
struct Data {
    std::string name;
    int value;
};

struct ResourceManager {
    std::unique_ptr<Data> unique_data;
    std::shared_ptr<Data> shared_data;

    std::unique_ptr<Data> create_unique(std::string n, int v);
};
```
```python
rm = my_module.ResourceManager()

# Smart pointers convert to/from Python dicts
result = rm.create_unique("test", 42)
print(result)  # {'name': 'test', 'value': 42}

# Set from dict - creates managed pointer automatically
rm.unique_data = {'name': 'data', 'value': 123}

# None handling for null pointers
rm.unique_data = None  # Sets to nullptr
```

### Nested Classes
```cpp
struct Address {
    std::string city;
};

struct Person {
    std::string name;
    Address addr;  // Nested!
};
```
```python
p = my_module.Person()
p.addr = {'city': 'Boston'}  # Dict conversion
```

### Cross-Module Type Sharing

Types bound in separate `.so` files can work together seamlessly. When a method returns a type that was bound in a different module, you get a proper wrapper object (not a dict):

```cpp
// vertex.hpp - bound in vertex.so
struct Vertex {
    double x, y;
    double distance_from_origin() const;
};

// bbox.hpp - bound in bbox.so (separate module!)
#include "vertex.hpp"
struct BoundingBox {
    Vertex top_left;
    Vertex get_top_left() const { return top_left; }
};
```

```python
import vertex  # Must import first to register Vertex type
import bbox

box = bbox.BoundingBox()
box.top_left = vertex.Vertex(1.0, 2.0)

# Returns a proper Vertex wrapper, NOT a dict!
corner = box.get_top_left()
print(type(corner))  # <class 'vertex.Vertex'>
print(corner.distance_from_origin())  # Methods work!
```

**Important:** Import the module containing the dependent type first (e.g., `import vertex` before `import bbox`).

**Type Registry Management:**

Mirror Bridge maintains a global type registry in `sys.modules['_mirror_bridge_types']`. For advanced use cases:

```python
import sys

# Inspect registered types
registry = sys.modules.get('_mirror_bridge_types', {})
print(f"Registered types: {len(registry)}")

# Clear registry (useful before reloading modules during development)
if '_mirror_bridge_types' in sys.modules:
    sys.modules['_mirror_bridge_types'].clear()
```

### Containers
```cpp
struct Data {
    std::vector<double> values;
    std::array<int, 3> coords;
};
```
```python
d.values = [1.0, 2.0, 3.0]  # List → vector
d.coords = [1, 2, 3]         # List → array
```

### Exception Handling
```cpp
double divide(double x) {
    if (x == 0) throw std::runtime_error("Division by zero");
    return value / x;
}
```
```python
try:
    calc.divide(0)
except RuntimeError as e:
    print(e)  # "Division by zero"
```

## Project Structure

```
mirror_bridge/
├── mirror_bridge.hpp           # Single-header library (core reflection logic)
├── mirror_bridge_pch.hpp       # Precompiled header wrapper (optional)
├── mirror_bridge_auto          # Auto-discovery script
├── mirror_bridge_generate      # Config file script
├── mirror_bridge_build         # Direct compilation script
├── mirror_bridge_build_pch     # PCH builder script (optional)
├── start_dev_container.sh      # Docker setup (persistent container)
├── examples/
│   ├── README.md               # Detailed usage guide
│   ├── option2/                # Auto-discovery example
│   └── option3/                # Config file example
└── tests/
    ├── run_all_tests.sh        # Automated test suite
    ├── test_pch.sh             # PCH functionality test
    └── e2e/                    # End-to-end tests
        ├── basic/              # Point2D, Vector3
        ├── containers/         # std::vector, std::array
        ├── nesting/            # Nested classes, cross-file
        └── methods/            # Method binding (Calculator)
```

## How It Works

Mirror Bridge leverages C++26 reflection at compile-time:

1. **Discovery**: Uses `std::meta::nonstatic_data_members_of(^^T)` to find all class members
2. **Method Introspection**: Uses `std::meta::members_of` + `std::meta::is_function` to find methods
3. **Type Extraction**: Uses `std::meta::type_of` and `std::meta::identifier_of` for names
4. **Code Generation**: Generates Python C API bindings via template metaprogramming
5. **Compilation**: Compiles to `.so` module with reflection-enabled clang

**All binding logic is resolved at compile-time - zero runtime overhead.**

See [CONTRIBUTING.md](CONTRIBUTING.md#architecture-overview) for technical details.

## Requirements

- **Compiler**: Bloomberg clang-p2996 (P2996 reflection support)
  - Provided via Docker: `./start_dev_container.sh`
  - Or build from: https://github.com/bloomberg/clang-p2996
- **Python**: 3.7+
- **Platform**: Linux (or macOS with Docker)

## Testing

```bash
# Inside Docker container:

# Run all automated tests
./tests/run_all_tests.sh

# Output:
# ✓ Built: 12 bindings
# ✓ Passed: 12 tests
# ✓ ALL TESTS PASSED!

# Test coverage:
# - Basic data members (Point2D, Vector3)
# - Containers (std::vector, std::array)
# - Nested classes (2-level, 3-level, cross-file)
# - Methods (Calculator with various signatures)
# - Variadic parameters (3, 4, 5, 6 parameter methods)
# - Constructors with parameters (0, 2, 3 parameters)
# - Method overloading (int/double/string overloads)
# - Smart pointers (unique_ptr, shared_ptr conversion)
```

**Advanced feature tests** ([`tests/e2e/advanced/`](tests/e2e/advanced/)):
- **Variadic**: Methods with 3-6 parameters, weighted sums, format functions
- **Constructors**: Default, 2-param, 3-param constructor matching
- **Overloading**: Type-based name mangling for overloaded methods
- **Smart Pointers**: Bidirectional dict conversion, nullptr handling, return values

## Documentation

- **[CONTRIBUTING.md](CONTRIBUTING.md)** - Development guide: setup, testing, CLI tools, architecture
- **[examples/README.md](examples/README.md)** - Usage examples and workflow comparisons
- **API Reference** - Inline documentation in [`mirror_bridge.hpp`](mirror_bridge.hpp)

## Current Limitations

- Passing bound class instances as parameters (requires reference/pointer handling)
- Template classes (must be explicitly instantiated before binding)
- Const method overloads (treated as same method currently)
- Advanced smart pointers (`weak_ptr`, custom deleters)

## Roadmap

**Recently Completed:**
- ✅ Variadic parameter support (any number of parameters)
- ✅ Constructor parameter binding
- ✅ Method overloading via name mangling
- ✅ Smart pointer support (`unique_ptr`, `shared_ptr`)

**Next:**
- [ ] Reference parameters and bound class passing
- [ ] Const method overload distinction
- [ ] Template class binding automation
- [ ] Additional backends (Rust, Lua)
- [ ] Python stub generation (.pyi files)

## Performance

Mirror Bridge delivers significant performance improvements over pybind11:

### Compile-Time: **2-3x faster**
- **Simple project** (1 class): 816ms vs 1,938ms pybind11 (2.4x faster)
- **Medium project** (10 classes): 1,543ms vs 3,637ms pybind11 (2.4x faster)
- **Why:** Reflection eliminates template metaprogramming overhead

### Runtime: **3-5x faster**
- **Function calls**: 35ns vs 127ns pybind11 (3.6x faster)
- **Object construction**: 47ns vs 256ns pybind11 (5.4x faster)
- **Why:** Direct Python C API calls, no template dispatch

### Developer Experience: **Zero boilerplate**
- **Auto-discovery**: `mirror_bridge_auto src/ --module name`
- **No binding code required** vs 18-103 lines for pybind11
- **Instant**: Add members/methods → automatically bound

**Methodology:** 5 runs per test, median ± stddev reported, identical optimization flags (`-O3 -DNDEBUG`)

### Precompiled Headers (PCH): **3-6x faster compilation**

For even faster builds, use precompiled headers to cache the Mirror Bridge infrastructure:

```bash
# One-time: Build PCH (takes ~600ms, reuse forever)
./mirror_bridge_build_pch -o build -t release

# Every build: Use PCH for 3-6x faster compilation
mirror_bridge_auto src/ --module my_module --use-pch build/mirror_bridge_pch.hpp.gch
```

**Performance with PCH:**
- **Simple project**: 567ms → 194ms (66% faster, 2.9x speedup)
- **Medium project**: 1580ms → 252ms (84% faster, 6.3x speedup)
- **One-time cost**: ~600ms to build PCH (amortized across all builds)

**Key benefits:**
- ✅ **Shared across projects** - build PCH once, use everywhere
- ✅ **Debug/Release PCH** - separate PCH for different build configurations
- ✅ **Zero code changes** - just add `--use-pch` flag
- ✅ **Automatic detection** - `mirror_bridge_auto` finds PCH automatically

**Complete guide:** See [PCH_GUIDE.md](PCH_GUIDE.md) and [WORKFLOW_GUIDE.md](WORKFLOW_GUIDE.md)

**Test suite:** Run `./tests/test_pch.sh` to verify PCH infrastructure

## Benchmarks

Run comprehensive tests yourself:

```bash
./run_benchmarks.sh
```

See [benchmarks/FINAL_RESULTS.md](benchmarks/FINAL_RESULTS.md) for complete results and analysis.

## Contributing

This is an experimental project exploring C++26 reflection. Contributions welcome!

**Areas needing work:**
- Extended parameter support for methods
- Template class handling
- Additional backends (Rust, Lua)

## License

Apache License 2.0 - See [LICENSE](LICENSE) for details.

## Acknowledgments

- [Bloomberg's clang-p2996](https://github.com/bloomberg/clang-p2996) - P2996 reflection implementation
- [P2996 Reflection Proposal](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p2996r7.html)
- simdjson - Concept-based design inspiration

---

**Status**: Experimental - C++26 reflection is not yet supported on all C++ compilers. This project uses Bloomberg's clang-p2996 implementation.

**Yes, method binding works!** See calculator tests for full examples.
