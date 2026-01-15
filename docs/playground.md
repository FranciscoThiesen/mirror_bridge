# Interactive Playground

Try mirror_bridge and C++26 reflection without any local installation.

## Quick Start Options

| Option | Best For | Time to Start |
|--------|----------|---------------|
| [Godbolt Examples](#godbolt-examples) | Understanding C++26 reflection | Instant |
| [GitHub Codespaces](#github-codespaces) | Full development environment | ~2 minutes |
| [Docker](#docker) | Local development | ~1 minute |

---

## Godbolt Examples

These examples run on [Compiler Explorer (Godbolt)](https://godbolt.org) using Bloomberg's clang-p2996 compiler with C++26 reflection support.

### Setup Instructions

1. Go to [godbolt.org](https://godbolt.org)
2. Select compiler: **clang (reflection)** (search for "reflection" in the compiler dropdown)
3. Add compiler flags: `-std=c++2c -freflection -freflection-latest`
4. Copy and paste any example below

### Example 1: Struct Member Introspection

See how C++26 reflection discovers struct members at compile-time:

```cpp
#include <meta>
#include <iostream>

struct Point {
    double x;
    double y;
};

// Consteval helper to get member count
template<typename T>
consteval std::size_t member_count() {
    return std::meta::nonstatic_data_members_of(
        ^^T, std::meta::access_context::current()).size();
}

// Consteval helper to get member by index
template<typename T, std::size_t Index>
consteval auto get_member() {
    return std::meta::nonstatic_data_members_of(
        ^^T, std::meta::access_context::current())[Index];
}

// Print member names using fold expression
template<typename T, std::size_t... Is>
void print_members(std::index_sequence<Is...>) {
    ((std::cout << "  - " << std::meta::identifier_of(get_member<T, Is>()) << "\n"), ...);
}

int main() {
    std::cout << "Point has " << member_count<Point>() << " members:\n";
    print_members<Point>(std::make_index_sequence<member_count<Point>()>{});
    return 0;
}
```

**Expected Output:**
```
Point has 2 members:
  - x
  - y
```

---

### Example 2: Compile-Time Type Information with Type Names

Extract detailed type information at compile-time:

```cpp
#include <meta>
#include <iostream>
#include <string>
#include <vector>

struct GameEntity {
    int id;
    std::string name;
    double x, y, z;
    std::vector<int> tags;
};

template<typename T>
consteval std::size_t member_count() {
    return std::meta::nonstatic_data_members_of(
        ^^T, std::meta::access_context::current()).size();
}

template<typename T, std::size_t I>
consteval auto get_member() {
    return std::meta::nonstatic_data_members_of(
        ^^T, std::meta::access_context::current())[I];
}

template<typename T, std::size_t... Is>
void print_members(std::index_sequence<Is...>) {
    ((std::cout << "  " << std::meta::identifier_of(get_member<T, Is>())
                << " : " << std::meta::display_string_of(
                            std::meta::type_of(get_member<T, Is>()))
                << "\n"), ...);
}

int main() {
    std::cout << "GameEntity has " << member_count<GameEntity>() << " members:\n";
    print_members<GameEntity>(
        std::make_index_sequence<member_count<GameEntity>()>{});
    return 0;
}
```

**Expected Output:**
```
GameEntity has 6 members:
  id : int
  name : std::string
  x : double
  y : double
  z : double
  tags : std::vector<int>
```

---

### Example 3: Automatic Serialization via Reflection

A practical example showing how reflection enables automatic JSON serialization:

```cpp
#include <meta>
#include <iostream>
#include <string>
#include <sstream>

struct Person {
    std::string name;
    int age;
    double height;
};

// Helper to get member count
template<typename T>
consteval std::size_t member_count() {
    return std::meta::nonstatic_data_members_of(
        ^^T, std::meta::access_context::current()).size();
}

// Helper to get member by index
template<typename T, std::size_t I>
consteval auto get_member() {
    return std::meta::nonstatic_data_members_of(
        ^^T, std::meta::access_context::current())[I];
}

// Serialize a single member to JSON
template<typename T, std::size_t I>
void serialize_member(std::ostream& os, const T& obj, bool& first) {
    if (!first) os << ", ";
    first = false;

    constexpr auto member = get_member<T, I>();
    os << "\"" << std::meta::identifier_of(member) << "\": ";

    // Access member value via splicer [:member:]
    const auto& value = obj.[:member:];

    if constexpr (std::is_arithmetic_v<std::remove_cvref_t<decltype(value)>>) {
        os << value;
    } else if constexpr (std::is_same_v<std::remove_cvref_t<decltype(value)>, std::string>) {
        os << "\"" << value << "\"";
    }
}

// Serialize all members using fold expression
template<typename T, std::size_t... Is>
void serialize_members(std::ostream& os, const T& obj, std::index_sequence<Is...>) {
    bool first = true;
    (serialize_member<T, Is>(os, obj, first), ...);
}

// Generic to_json using C++26 reflection
template<typename T>
std::string to_json(const T& obj) {
    std::ostringstream oss;
    oss << "{";
    serialize_members<T>(oss, obj, std::make_index_sequence<member_count<T>()>{});
    oss << "}";
    return oss.str();
}

int main() {
    Person alice{"Alice", 30, 1.65};
    std::cout << to_json(alice) << "\n";

    Person bob{"Bob", 25, 1.80};
    std::cout << to_json(bob) << "\n";

    return 0;
}
```

**Expected Output:**
```json
{"name": "Alice", "age": 30, "height": 1.65}
{"name": "Bob", "age": 25, "height": 1.80}
```

---

### Example 4: Enum Reflection

C++26 reflection works on enums too:

```cpp
#include <meta>
#include <iostream>

enum class Color { Red, Green, Blue, Yellow };

// Get enumerator count
template<typename E>
consteval std::size_t enum_count() {
    return std::meta::enumerators_of(^^E).size();
}

// Get enumerator by index
template<typename E, std::size_t I>
consteval auto get_enumerator() {
    return std::meta::enumerators_of(^^E)[I];
}

// Print all enumerators
template<typename E, std::size_t... Is>
void print_enumerators(std::index_sequence<Is...>) {
    ((std::cout << "  " << std::meta::identifier_of(get_enumerator<E, Is>())
                << " = " << static_cast<int>([:get_enumerator<E, Is>():])
                << "\n"), ...);
}

int main() {
    std::cout << "Color enum has " << enum_count<Color>() << " values:\n";
    print_enumerators<Color>(std::make_index_sequence<enum_count<Color>()>{});
    return 0;
}
```

**Expected Output:**
```
Color enum has 4 values:
  Red = 0
  Green = 1
  Blue = 2
  Yellow = 3
```

---

## How mirror_bridge Uses Reflection

The examples above show the core C++26 reflection features. mirror_bridge combines these to automatically generate Python, Lua, and JavaScript bindings.

Here's the equivalent mirror_bridge code for binding a Calculator class:

### Traditional Binding (pybind11) - 18 lines

```cpp
#include <pybind11/pybind11.h>
namespace py = pybind11;

PYBIND11_MODULE(calc, m) {
    py::class_<Calculator>(m, "Calculator")
        .def(py::init<>())
        .def_readwrite("value", &Calculator::value)
        .def_readwrite("name", &Calculator::name)
        .def("add", &Calculator::add)
        .def("subtract", &Calculator::subtract)
        .def("multiply", &Calculator::multiply)
        .def("divide", &Calculator::divide)
        .def("get_value", &Calculator::get_value)
        .def("compute", &Calculator::compute)
        .def("reset", &Calculator::reset)
        .def("to_string", &Calculator::to_string);
}
```

### mirror_bridge - 3 lines

```cpp
#include "mirror_bridge.hpp"
#include "calculator.hpp"

MIRROR_BRIDGE_MODULE(calc,
    mirror_bridge::bind_class<Calculator>(m, "Calculator");
)
```

The reflection-based approach:
- **Zero manual binding code** - members discovered automatically
- **Always in sync** - add a field to C++, it appears in Python
- **Type-safe** - compile-time verification of all bindings

---

## GitHub Codespaces

The fastest way to try full mirror_bridge functionality:

[![Open in GitHub Codespaces](https://github.com/codespaces/badge.svg)](https://codespaces.new/FranciscoThiesen/mirror_bridge)

Once the Codespace starts (~2 minutes):

```bash
# Run the test suite
./tests/run_all_tests.sh

# Try the hello-world example
cd examples/01-hello-world
../../tools/mirror_bridge generate src/ --module greeter --lang python
python3 test_greeter.py

# Generate bindings for all three languages
../../tools/mirror_bridge generate src/ --module greeter --lang all
```

---

## Docker

Run locally with a single command:

```bash
docker run -it --rm -v $(pwd):/workspace ghcr.io/franciscothiesen/mirror_bridge:latest
```

Inside the container:

```bash
# Navigate to examples
cd examples/01-hello-world

# Generate Python bindings
../../tools/mirror_bridge generate src/ --module greeter --lang python
python3 test_greeter.py

# Generate Lua bindings
../../tools/mirror_bridge generate src/ --module greeter --lang lua
lua test_greeter.lua

# Generate JavaScript bindings
../../tools/mirror_bridge generate src/ --module greeter --lang js
node test_greeter.js
```

---

## Example Code to Try

Copy these into your own files:

### Simple Point Class

```cpp
// point.hpp
#pragma once

struct Point {
    double x = 0.0;
    double y = 0.0;

    double distance() const {
        return std::sqrt(x*x + y*y);
    }
};
```

```cpp
// point_binding.cpp
#include "mirror_bridge.hpp"
#include "point.hpp"

MIRROR_BRIDGE_MODULE(geometry,
    mirror_bridge::bind_class<Point>(m, "Point");
)
```

```python
# test_point.py
import geometry

p = geometry.Point()
p.x = 3.0
p.y = 4.0
print(f"Distance: {p.distance()}")  # Output: Distance: 5.0
```

### Container Support

```cpp
// data.hpp
#pragma once
#include <vector>
#include <string>
#include <map>

struct DataStore {
    std::vector<int> numbers;
    std::map<std::string, double> scores;

    void add_number(int n) { numbers.push_back(n); }
    void set_score(const std::string& name, double score) {
        scores[name] = score;
    }
};
```

```python
# test_data.py
import data

store = data.DataStore()
store.add_number(1)
store.add_number(2)
store.add_number(3)
print(store.numbers)  # [1, 2, 3]

store.set_score("Alice", 95.5)
store.set_score("Bob", 87.0)
print(store.scores)  # {'Alice': 95.5, 'Bob': 87.0}
```

---

## Next Steps

- **[Quick Start Guide](getting-started/quickstart.md)** - Full setup instructions
- **[Examples](../examples/)** - Progressive examples from simple to advanced
- **[Type Conversion Reference](reference/type-conversion.md)** - Supported C++ types
- **[CLI Reference](reference/cli.md)** - All command-line options
