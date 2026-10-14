# Type Conversion Reference

Mirror Bridge automatically converts types between C++ and target languages. This document details all supported conversions.

## Primitive Types

### Numeric Types

| C++ Type | Python | Lua | JavaScript |
|----------|--------|-----|------------|
| `bool` | `bool` | `boolean` | `boolean` |
| `char` | `int` | `number` | `number` |
| `short` | `int` | `number` | `number` |
| `int` | `int` | `number` | `number` |
| `long` | `int` | `number` | `number` |
| `long long` | `int` | `number` | `number` |
| `unsigned int` | `int` | `number` | `number` |
| `float` | `float` | `number` | `number` |
| `double` | `float` | `number` | `number` |

### String Types

| C++ Type | Python | Lua | JavaScript |
|----------|--------|-----|------------|
| `std::string` | `str` | `string` | `string` |
| `std::string_view` | `str` | `string` | `string` |
| `const char*` | `str` | `string` | `string` |

## Container Types

### Sequential Containers

| C++ Type | Python | Lua | JavaScript |
|----------|--------|-----|------------|
| `std::vector<T>` (numeric T) | `array.array` (bulk memcpy) | `table` (array) | `Array` |
| `std::vector<T>` (other T) | `list` | `table` (array) | `Array` |
| `std::array<T, N>` | `list` | `table` (array) | `Array` |
| `std::deque<T>` | `list` | `table` (array) | `Array` |
| `std::list<T>` | `list` | `table` (array) | `Array` |

**Bulk transfer optimization**: For `vector<float>`, `vector<double>`, `vector<int>`, and other numeric types, Python bindings use a single `memcpy` into an `array.array` object instead of element-by-element conversion. This is ~10-50x faster for large arrays. The `from_python` direction accepts both `list` and `array.array` inputs.

**Example:**

```cpp
struct Container {
    std::vector<int> numbers;
    std::vector<std::string> names;
};
```

**Python:**
```python
c = Container()
c.numbers = [1, 2, 3, 4, 5]
c.names = ["Alice", "Bob"]
print(c.numbers)  # [1, 2, 3, 4, 5]
```

**Lua:**
```lua
c = Container()
c.numbers = {1, 2, 3, 4, 5}
c.names = {"Alice", "Bob"}
```

**JavaScript:**
```javascript
const c = new Container();
c.numbers = [1, 2, 3, 4, 5];
c.names = ["Alice", "Bob"];
```

### Associative Containers

| C++ Type | Python | Lua | JavaScript |
|----------|--------|-----|------------|
| `std::map<K, V>` | `dict` | `table` | `Object` |
| `std::unordered_map<K, V>` | `dict` | `table` | `Object` |

## Nested Objects

Nested structs/classes are converted to native dictionary/object/table types:

**C++:**
```cpp
struct Address {
    std::string street;
    std::string city;
    int zip;
};

struct Person {
    std::string name;
    Address address;
};
```

**Python:**
```python
p = Person()
p.name = "Alice"
p.address = {"street": "123 Main St", "city": "Boston", "zip": 12345}

# Reading nested objects
addr = p.address  # Returns a dict
print(addr["city"])  # "Boston"
```

**Lua:**
```lua
p = Person()
p.name = "Alice"
p.address = {street = "123 Main St", city = "Boston", zip = 12345}
```

**JavaScript:**
```javascript
const p = new Person();
p.name = "Alice";
p.address = {street: "123 Main St", city: "Boston", zip: 12345};
```

## Smart Pointers

| C++ Type | Conversion |
|----------|------------|
| `std::unique_ptr<T>` | Converted to/from value |
| `std::shared_ptr<T>` | Converted to/from value |

**C++:**
```cpp
struct Resource {
    std::string name;
    int value;
};

struct Manager {
    std::unique_ptr<Resource> resource;
    std::unique_ptr<Resource> create_resource(std::string n, int v);
};
```

**Python:**
```python
m = Manager()

# Smart pointers convert to/from dicts
result = m.create_resource("test", 42)
print(result)  # {"name": "test", "value": 42}

# Assign dict to smart pointer property
m.resource = {"name": "data", "value": 123}

# None handling for null pointers
m.resource = None  # Sets to nullptr
```

## Enum Types

| C++ Type | Python | Lua | JavaScript |
|----------|--------|-----|------------|
| `enum` | `int` | `number` | `number` |
| `enum class` | `int` | `number` | `number` |

**C++:**
```cpp
enum class Color { Red = 0, Green = 1, Blue = 2 };

struct ColoredShape {
    Color color;
    int get_color_value() const { return static_cast<int>(color); }
};
```

**Python:**
```python
s = ColoredShape()
s.color = 1  # Green
print(s.get_color_value())  # 1
```

## Method Parameters

All supported types can be used as method parameters:

**C++:**
```cpp
struct Processor {
    // Primitive parameters
    int add(int a, int b);

    // String parameters
    void set_name(std::string name);

    // Container parameters
    double sum(std::vector<double> values);

    // Multiple parameters
    std::string format(std::string prefix, int value, std::string suffix);
};
```

**Python:**
```python
p = Processor()
result = p.add(10, 20)  # 30
p.set_name("MyProcessor")
total = p.sum([1.0, 2.0, 3.0])  # 6.0
text = p.format("Value: ", 42, "!")  # "Value: 42!"
```

## Return Values

All supported types can be returned from methods:

| Return Type | Conversion |
|-------------|------------|
| `void` | `None` / `nil` / `undefined` |
| Primitive types | Direct conversion |
| `std::string` | String type |
| Containers | List/Array/Table |
| Nested objects | Dict/Object/Table |
| Smart pointers | Converted value or null |

## Const Correctness

Const methods are supported and bound correctly:

```cpp
struct Data {
    int value;

    int get_value() const { return value; }  // Bound
    void set_value(int v) { value = v; }      // Bound
};
```

## std::optional

| C++ Type | Python | Lua | JavaScript |
|----------|--------|-----|------------|
| `std::optional<T>` (with value) | Converted `T` | Converted `T` | Converted `T` |
| `std::optional<T>` (empty) | `None` | `nil` | `null` |

## std::expected

C++23's `std::expected<T, E>` maps naturally to each language's error handling idiom:

| C++ State | Python | Lua | JavaScript |
|-----------|--------|-----|------------|
| Success (has value) | Returns `T` | Returns `value, nil` | Returns `T` |
| Error (!has_value) | Raises `ValueError` | Returns `nil, error_string` | Throws `Error` |

**C++:**
```cpp
struct Service {
    std::expected<double, std::string> safe_divide(double a, double b) {
        if (b == 0.0) return std::unexpected("division by zero");
        return a / b;
    }
};
```

**Python:**
```python
svc = Service()
result = svc.safe_divide(10.0, 2.0)  # Returns 5.0
try:
    svc.safe_divide(10.0, 0.0)       # Raises ValueError("division by zero")
except ValueError as e:
    print(e)
```

**Lua:**
```lua
local svc = Service()
local result, err = svc:safe_divide(10.0, 2.0)  -- result=5.0, err=nil
local result, err = svc:safe_divide(10.0, 0.0)  -- result=nil, err="division by zero"
```

**JavaScript:**
```javascript
const svc = new Service();
const result = svc.safe_divide(10.0, 2.0);  // 5.0
try {
    svc.safe_divide(10.0, 0.0);  // throws Error("division by zero")
} catch (e) { console.error(e.message); }
```

## Argument Type Checking (Python)

This section describes the Python backend. Lua and JavaScript do not check
argument identity yet.

A parameter, data member, or container element whose type is a bound C++
class accepts only an object of that class, an object of a class derived
from it, or a Python subclass of either. Anything else raises `TypeError`:

```python
curve = pricing.Curve()
pricing.discount(curve, 2.0)   # fine
pricing.discount(42, 2.0)      # TypeError: Argument 1: type conversion failed
pricing.discount(label, 2.0)   # TypeError, even though Label is also bound
```

Conversion to a base class is offset-adjusted, so it is correct for a base
that is not the first one:

```cpp
struct Swap : Observable, Priceable { ... };   // Priceable is not at offset 0
double value(const Priceable&);
```

```python
value(swap)   # reaches the Priceable subobject, not the start of the Swap
```

Virtual bases work too: the offset is resolved at call time rather than
assumed, and the same class bound by two different modules interoperates.

The conversion is recorded by the derived class when it is bound, so the
base and the derived class may live in different modules and be imported in
either order. Two cases are deliberately not convertible:

- A derived class whose module has never been imported, because nothing has
  registered it yet.
- A base that C++ itself would not let you reach from that call site: one
  that is inaccessible (`private`/`protected`) or ambiguous because it is
  inherited twice non-virtually.

Inside an overload set a rejected argument simply moves to the next
candidate, so `TypeError` is raised only when no overload matches.

## Limitations

### Shapes at the edge

Every row below was measured against a built module, not reasoned about. The
ones that are refused are refused by name at build time, so a signature that
cannot cross is something you hear about from `generate` rather than from an
`AttributeError` later.

| Shape | Status | What to write instead |
|-------|--------|-----------------------|
| `T*` as a parameter or return | Not bound | A pointer says nothing about who owns the thing. Return by value, or use `shared_ptr`/`unique_ptr`, which do |
| `const char*` as a parameter or return | Bound, crosses as `str` | Nothing. The caller borrows it and the value is copied into a Python `str` before the call returns. A null crosses as `None`, which is how a C API says "absent" |
| `char*` as a return | Not bound | By convention that one is the caller's to free, so copying it would leak. Return `std::string` |
| `const T&` as a parameter | Bound, no copy | Nothing |
| `T&` as a parameter, `T` a bound class | Bound, aliases the Python object, writes are visible | Nothing |
| `T&` as a parameter, `T` anything else | Not bound | The argument would be a copy and the write would be lost, so this is refused rather than silently doing nothing. Return the value: `double sum(const std::vector<double>&)`, not `void sum(const std::vector<double>&, double&)` |
| `std::span<T>` as a parameter | Not bound | A span does not own its storage. Take `const std::vector<T>&` |
| Overloaded free functions | Not bound | Give one overload a distinct name, or bind a single non-overloaded wrapper |
| Function pointers | Not bound | Take `std::function<R(Args...)>`, which does cross and can be called with any Python callable |
| `weak_ptr` | Not bound | Pass `shared_ptr` |

Every one of these is reported by `mirror_bridge generate`, by name and with
the reason, and carried in `--json` under `declined`. Nothing is dropped in
silence.

### Template Classes

`mirror_bridge generate` plans template instantiations itself: it reads the
parameter list from the headers, tries combinations against the compiler, and
binds the ones that compile. `--instantiate "geom::Vector3<short>"` names one
explicitly and `--no-templates` turns the pass off.

Writing a binding by hand, there is no planner, so the specialization has to
exist before it can be bound:

```cpp
template<typename T>
struct Container {
    T value;
};

// Explicit instantiation
using IntContainer = Container<int>;
using StringContainer = Container<std::string>;

// Now can be bound
MIRROR_BRIDGE_MODULE(my_module,
    mirror_bridge::bind_class<IntContainer>(m, "IntContainer");
    mirror_bridge::bind_class<StringContainer>(m, "StringContainer");
)
```
