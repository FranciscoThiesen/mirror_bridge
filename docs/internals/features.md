# Verified Features Matrix

All features have been tested and verified across all supported languages.

**Last Updated**: 2026-04-12
**Environment**: Docker (Bloomberg clang-p2996)

## Supported Languages

| Language | Status | Auto-Discovery | Tests Passing |
|----------|--------|---------------|---------------|
| **Python** | Stable | `mirror_bridge generate --lang python` | 100% |
| **Lua** | Stable | `mirror_bridge generate --lang lua` | 100% |
| **JavaScript** | Stable | `mirror_bridge generate --lang js` | 100% |
| **Rust** | Beta | Code generation via `generate_bindings<T>()` | 100% |

## Feature Matrix

| Feature | Python | Lua | JavaScript |
|---------|--------|-----|------------|
| **Default Constructors** | Yes | Yes | Yes |
| **Parameterized Constructors** | Yes | Yes | Yes |
| **Property Getters** | Yes | Yes | Yes |
| **Property Setters** | Yes | Yes | Yes |
| **Methods (0 params)** | Yes | Yes | Yes |
| **Methods (1+ params)** | Yes | Yes | Yes |
| **Methods (variadic)** | Yes | Yes | Yes |
| **Void Methods** | Yes | Yes | Yes |
| **Const Methods** | Yes | Yes | Yes |
| **Return Values (numeric)** | Yes | Yes | Yes |
| **Return Values (string)** | Yes | Yes | Yes |
| **Containers (vector)** | Yes | Yes | Yes |
| **Containers (array)** | Yes | Yes | Yes |
| **Nested Objects** | Yes | Yes | Yes |
| **Enums** | Yes | Yes | Yes |
| **Method Overloading** | Yes | Partial | Partial |
| **Smart Pointers** | Yes | Yes | Yes |
| **Cross-Module Types** | Yes | No | No |
| **Inheritance** | Yes | Yes | Yes |
| **Exception Handling** | Yes | Yes | Yes |
| **Auto-Discovery** | Yes | Yes | Yes |
| **Async/Await (std::future)** | Yes | No | Yes |
| **std::optional** | Yes | Yes | Yes |
| **std::expected** | Yes | Yes | Yes |
| **Bulk Array Transfer** | Yes (array.array) | No | No |
| **Compile-Time Validation** | Yes | Yes | Yes |

## Test Coverage

### Python Tests

| Category | Tests | Status |
|----------|-------|--------|
| Basic (Point2D, Vector3) | 3 | Passing |
| Containers | 2 | Passing |
| Nesting | 3 | Passing |
| Methods (Calculator) | 4 | Passing |
| Advanced (variadic) | 2 | Passing |
| Advanced (constructors) | 2 | Passing |
| Advanced (overloading) | 2 | Passing |
| Advanced (smart_ptrs) | 2 | Passing |

### Lua Tests

| Category | Tests | Status |
|----------|-------|--------|
| Calculator | 12 | Passing |
| Auto-Discovery | 3 | Passing |
| Comprehensive Features | 4 | Passing |
| **Total** | **19** | **100%** |

### JavaScript Tests

| Category | Tests | Status |
|----------|-------|--------|
| Calculator | 12 | Passing |
| Auto-Discovery | 3 | Passing |
| Comprehensive Features | 4 | Passing |
| **Total** | **19** | **100%** |

## Feature Details

### Constructors

```cpp
struct Rectangle {
    Rectangle() : width(0), height(0) {}
    Rectangle(double w, double h) : width(w), height(h) {}
    double width, height;
};
```

All languages support:
- Default constructors: `Rectangle()`
- Parameterized constructors: `Rectangle(10.0, 5.0)`

### Methods

```cpp
struct Calculator {
    double value = 0.0;
    double add(double x) { return value += x; }
    void reset() { value = 0; }
    double compute(double x, double y) { return (x + y) * 2; }
    std::string to_string() const;
};
```

Supported:
- Any number of parameters (variadic)
- Void return type
- Const methods
- String return values

### Containers

```cpp
struct VectorTest {
    std::vector<int> numbers;
    std::vector<std::string> names;
    std::array<double, 3> coords;
};
```

Bidirectional conversion:
- C++ `vector` <-> Python `list` / Lua `table` / JS `Array`
- C++ `array` <-> Python `list` / Lua `table` / JS `Array`

### Nested Objects

```cpp
struct Address { std::string city; int zip; };
struct Person { std::string name; Address address; };
```

Nested structs convert to native dict/object/table types with full read/write access.

### Enums

```cpp
enum class Color { Red = 0, Green = 1, Blue = 2 };
struct Shape { Color color; };
```

Enums convert to integers in all languages.

### Smart Pointers

```cpp
struct Manager {
    std::unique_ptr<Resource> resource;
    std::shared_ptr<Resource> shared_resource;
};
```

All three languages support smart pointers as members, parameters, and
return values with deep-copy semantics (the target-language value is
materialized into a fresh pointee). `None`/`nil`/`null` resets the pointer.
Smart pointers to abstract types support only the reset round-trip; the
pointee can't be materialized by value.

## Known Limitations

### All Languages

| Limitation | Status |
|------------|--------|
| Raw pointers (T*) | Not supported |
| `std::optional` | Supported |
| `std::variant` | Supported |
| `std::tuple`/`std::pair` | Supported |
| `std::map`/`std::unordered_map` | Supported |
| `std::set`/`std::unordered_set` | Supported |
| `std::function` | Python only (callbacks) |
| Template classes | Must explicitly instantiate |

### Lua & JavaScript

| Limitation | Status |
|------------|--------|
| Cross-module types | Not supported |
| Method overloading | Limited |

## Performance

| Metric | Value |
|--------|-------|
| Compilation (simple) | <1 second |
| Compilation (medium) | 1-5 seconds |
| Runtime overhead | Zero (compile-time binding) |
| Lines per class | 0 (auto-discovery) or 3 (manual) |

## Running Tests

```bash
# All languages
./tests/run_all_tests.sh

# Python only
./tests/run_all_tests.sh python

# Lua only
./tests/lua/run_lua_tests.sh

# JavaScript only
./tests/js/run_js_tests.sh
```
