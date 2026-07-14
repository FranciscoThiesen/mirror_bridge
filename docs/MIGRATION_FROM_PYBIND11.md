# Migrating from pybind11 to Mirror Bridge

This guide helps developers familiar with pybind11 transition to Mirror Bridge's reflection-based binding system.

## Why Migrate?

| Aspect | pybind11 | Mirror Bridge |
|--------|----------|---------------|
| Binding code | Manual per-member | Automatic reflection |
| Code reduction | - | 5-10x less code |
| Compile time | Baseline | 1.4-1.6x faster |
| Runtime overhead | Baseline | Within 10% |
| Multi-language | Python only | Python, Lua, JavaScript |
| Maintenance | Update bindings manually | Auto-reflects changes |

## Quick Comparison

### Before: pybind11

```cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "point.hpp"

namespace py = pybind11;

PYBIND11_MODULE(geometry, m) {
    py::class_<Point2D>(m, "Point2D")
        .def(py::init<>())
        .def(py::init<double, double>())
        .def_readwrite("x", &Point2D::x)
        .def_readwrite("y", &Point2D::y)
        .def("magnitude", &Point2D::magnitude)
        .def("normalize", &Point2D::normalize)
        .def("dot", &Point2D::dot)
        .def("distance_to", &Point2D::distance_to)
        .def("__repr__", [](const Point2D& p) {
            return "Point2D(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";
        });

    py::class_<Line2D>(m, "Line2D")
        .def(py::init<Point2D, Point2D>())
        .def_readwrite("start", &Line2D::start)
        .def_readwrite("end", &Line2D::end)
        .def("length", &Line2D::length)
        .def("midpoint", &Line2D::midpoint);
}
```

### After: Mirror Bridge

```cpp
#include "mirror_bridge.hpp"
#include "point.hpp"

MIRROR_BRIDGE_MODULE(geometry,
    mirror_bridge::bind_class<Point2D>(m, "Point2D");
    mirror_bridge::bind_class<Line2D>(m, "Line2D");
)
```

That's it. All members, methods, and constructors are automatically discovered via C++26 reflection.

## Step-by-Step Migration

### Step 1: Update Include

```diff
- #include <pybind11/pybind11.h>
- #include <pybind11/stl.h>
+ #include "mirror_bridge.hpp"
```

### Step 2: Replace Module Macro

```diff
- namespace py = pybind11;
- PYBIND11_MODULE(mymodule, m) {
+ MIRROR_BRIDGE_MODULE(mymodule,
```

### Step 3: Replace Class Bindings

```diff
-     py::class_<MyClass>(m, "MyClass")
-         .def(py::init<>())
-         .def(py::init<int>())
-         .def_readwrite("value", &MyClass::value)
-         .def("get_value", &MyClass::get_value)
-         .def("set_value", &MyClass::set_value);
+     mirror_bridge::bind_class<MyClass>(m, "MyClass");
```

### Step 4: Close Module

```diff
- }
+ )
```

## Feature Mapping

### Constructors

**pybind11:**
```cpp
py::class_<Widget>(m, "Widget")
    .def(py::init<>())
    .def(py::init<int>())
    .def(py::init<std::string, int>());
```

**Mirror Bridge:**
```cpp
// All public constructors are automatically bound
mirror_bridge::bind_class<Widget>(m, "Widget");
```

### Data Members

**pybind11:**
```cpp
.def_readwrite("value", &Widget::value)      // Read-write
.def_readonly("constant", &Widget::constant)  // Read-only
```

**Mirror Bridge:**
```cpp
// All public data members are automatically bound as read-write
// For read-only, make the member const in C++:
//   const int constant;
mirror_bridge::bind_class<Widget>(m, "Widget");
```

### Methods

**pybind11:**
```cpp
.def("process", &Widget::process)
.def("calculate", &Widget::calculate)
.def_static("create", &Widget::create)
```

**Mirror Bridge:**
```cpp
// All public methods (including static) are automatically bound
mirror_bridge::bind_class<Widget>(m, "Widget");
```

### Method Overloading

**pybind11:**
```cpp
.def("add", py::overload_cast<int>(&Calculator::add))
.def("add", py::overload_cast<double>(&Calculator::add))
.def("add", py::overload_cast<const std::string&>(&Calculator::add))
```

**Mirror Bridge:**
```cpp
// Overloads are automatically detected and dispatched
mirror_bridge::bind_class<Calculator>(m, "Calculator");
```

Python usage remains the same - the correct overload is selected based on argument types.

### Static Methods

**pybind11:**
```cpp
.def_static("create", &Factory::create)
.def_static("version", &Factory::version)
```

**Mirror Bridge:**
```cpp
// Static methods are automatically detected and bound
mirror_bridge::bind_class<Factory>(m, "Factory");
```

### Static Constexpr Members

**pybind11:**
```cpp
py::class_<Constants>(m, "Constants")
    .def_readonly_static("PI", &Constants::PI)
    .def_readonly_static("E", &Constants::E);
```

**Mirror Bridge:**
```cpp
// static constexpr members are automatically bound as class attributes
mirror_bridge::bind_class<Constants>(m, "Constants");
```

### STL Containers

**pybind11:**
```cpp
#include <pybind11/stl.h>  // Required for automatic conversion

.def("get_items", &Container::get_items)  // Returns std::vector
.def("set_items", &Container::set_items)  // Takes std::vector
```

**Mirror Bridge:**
```cpp
// STL containers are automatically converted
// No additional includes needed
mirror_bridge::bind_class<Container>(m, "Container");
```

Supported containers:
- `std::vector<T>` ↔ Python `list`
- `std::map<K, V>` ↔ Python `dict`
- `std::set<T>` ↔ Python `set`
- `std::optional<T>` ↔ Python `T | None`
- `std::string` ↔ Python `str`

### Smart Pointers

**pybind11:**
```cpp
py::class_<Resource, std::shared_ptr<Resource>>(m, "Resource")
    .def(py::init<>())
    ...
```

**Mirror Bridge:**
```cpp
// Smart pointer semantics are automatically handled
mirror_bridge::bind_class<Resource>(m, "Resource");

// For factory methods returning shared_ptr:
// std::shared_ptr<Resource> Factory::create();
// Just works - returns the wrapped object to Python
```

### Callbacks (std::function)

**pybind11:**
```cpp
.def("set_callback", &Widget::set_callback)  // Just works
```

**Mirror Bridge:**
```cpp
// std::function parameters accept Python callables
mirror_bridge::bind_class<Widget>(m, "Widget");
```

Python usage:
```python
widget.set_callback(lambda x: x * 2)
```

### Enums

**pybind11:**
```cpp
py::enum_<Color>(m, "Color")
    .value("Red", Color::Red)
    .value("Green", Color::Green)
    .value("Blue", Color::Blue);
```

**Mirror Bridge:**
```cpp
// Enums are automatically bound when used by a bound class
// Standalone enum binding:
mirror_bridge::bind_enum<Color>(m, "Color");
```

### Free Functions

**pybind11:**
```cpp
m.def("compute", &compute);
m.def("transform", &transform);
```

**Mirror Bridge:**
```cpp
mirror_bridge::bind_function<&compute>(m, "compute");
mirror_bridge::bind_function<&transform>(m, "transform");

// Or use auto-discovery with mirror_bridge_auto which finds
// all functions in namespaces that contain bound classes
```

### Custom __repr__

**pybind11:**
```cpp
.def("__repr__", [](const Point& p) {
    return "Point(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";
});
```

**Mirror Bridge:**
```cpp
// Add a to_string() or operator<< to your class:
std::string Point::to_string() const {
    return "Point(" + std::to_string(x) + ", " + std::to_string(y) + ")";
}
// It will be used for __repr__ automatically
```

## Excluding Members

### Skip a Class

```cpp
// Add comment before class definition
// MIRROR_BRIDGE_SKIP
class InternalHelper {
    // This class won't be bound
};
```

### Skip a File

```cpp
// At the top of the file
// MIRROR_BRIDGE_SKIP_FILE
```

## Build System Changes

### CMake

**Before (pybind11):**
```cmake
find_package(pybind11 REQUIRED)
pybind11_add_module(mymodule src/bindings.cpp)
```

**After (Mirror Bridge):**
```cmake
# Use mirror_bridge_build script or:
add_library(mymodule SHARED src/bindings.cpp)
target_compile_options(mymodule PRIVATE
    -std=c++2c -freflection -freflection-latest -stdlib=libc++
)
target_include_directories(mymodule PRIVATE
    ${CMAKE_SOURCE_DIR}/path/to/mirror_bridge
    ${Python3_INCLUDE_DIRS}
)
```

### Command Line

**Before:**
```bash
c++ -O3 -shared -std=c++17 -fPIC \
    $(python3 -m pybind11 --includes) \
    bindings.cpp -o mymodule$(python3-config --extension-suffix)
```

**After:**
```bash
./mirror_bridge_build bindings.cpp -o build/
# Or use mirror_bridge_auto for auto-discovery:
./mirror_bridge_auto src/ --module mymodule
```

## Common Migration Issues

### Issue: Private Members Being Bound

Mirror Bridge only binds public members. If you had pybind11 bindings for private members using friend declarations, you'll need to:
1. Make the members public, or
2. Add public accessor methods

### Issue: Custom Type Converters

If you had custom `type_caster` specializations in pybind11:

```cpp
namespace pybind11 { namespace detail {
    template <> struct type_caster<MyType> { ... };
}}
```

Mirror Bridge uses concepts for type handling. For custom types, ensure they satisfy one of the built-in concepts or add a specialization.

### Issue: Keep-Alive Policies

pybind11's `py::keep_alive<>()` doesn't have a direct equivalent. Mirror Bridge uses reference counting and shared_ptr semantics. If you need specific lifetime management, use `std::shared_ptr`.

### Issue: Return Value Policies

pybind11's return value policies (`return_value_policy::reference`, etc.) are not needed. Mirror Bridge automatically:
- Copies value types
- Wraps pointer/reference returns appropriately
- Uses move semantics where applicable

## Gradual Migration

You can migrate incrementally:

1. **Start with a single class**: Replace one pybind11 class binding with Mirror Bridge
2. **Keep both systems**: Have separate binding files during transition
3. **Verify behavior**: Run your existing tests
4. **Expand gradually**: Migrate more classes as confidence grows

## Getting Help

- Check `examples/` for common patterns
- Run `./mirror_bridge_doctor` to verify your setup
- See `ARCHITECTURE.md` for system understanding
- Review `docs/internals/benchmarks.md` for performance expectations (regenerated monthly by CI)

## Summary

| pybind11 | Mirror Bridge |
|----------|---------------|
| `PYBIND11_MODULE(name, m)` | `MIRROR_BRIDGE_MODULE(name,` |
| `py::class_<T>(m, "T")` | `mirror_bridge::bind_class<T>(m, "T");` |
| `.def(py::init<...>())` | (automatic) |
| `.def_readwrite("x", &T::x)` | (automatic) |
| `.def("method", &T::method)` | (automatic) |
| `.def_static(...)` | (automatic) |
| `py::enum_<E>(m, "E")` | `mirror_bridge::bind_enum<E>(m, "E");` |
| `m.def("func", &func)` | `mirror_bridge::bind_function<&func>(m, "func");` |
| `}` | `)` |

The key insight: **Mirror Bridge trades explicit bindings for automatic reflection.** You write less code, but you need C++26 reflection support (Bloomberg clang-p2996).
