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
| `std::vector<T>` | `list` | `table` (array) | `Array` |
| `std::array<T, N>` | `list` | `table` (array) | `Array` |
| `std::deque<T>` | `list` | `table` (array) | `Array` |
| `std::list<T>` | `list` | `table` (array) | `Array` |

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

## Limitations

### Not Currently Supported

| Type | Status |
|------|--------|
| Raw pointers (T*) | Not supported |
| References as parameters | Partial support |
| `std::optional<T>` | Not yet implemented |
| `std::variant<Ts...>` | Not yet implemented |
| `std::tuple<Ts...>` | Not yet implemented |
| Function pointers | Not supported |
| `std::function<>` | Not supported |
| `weak_ptr` | Not supported |

### Template Classes

Template classes must be explicitly instantiated before binding:

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
