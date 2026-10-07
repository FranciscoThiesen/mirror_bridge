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

## Argument Type Checking

A parameter, data member, or container element whose type is a bound C++
class accepts only an object of that class, an object of a class derived
from it, or a host-language subclass of either. Anything else is refused:
`TypeError` in Python and JavaScript, a Lua error in Lua.

```python
curve = pricing.Curve()
pricing.discount(curve, 2.0)   # fine
pricing.discount(42, 2.0)      # TypeError: Argument 1: type conversion failed
pricing.discount(label, 2.0)   # TypeError, even though Label is also bound
```

```lua
local curve = pricing.Curve.new()
curve:at(2.0)        -- fine
curve.at(label, 2.0) -- error: at: expected Curve, got Label
```

```javascript
const curve = new pricing.Curve();
curve.at(2.0);                                  // fine
pricing.Curve.prototype.at.call(label, 2.0);    // TypeError: at: expected Curve, got Label
```

The receiver is checked as well as the arguments. `obj.method(x, ...)` in Lua
and `Class.prototype.method.call(x, ...)` in JavaScript reach the same code as
`obj:method(...)` and `obj.method(...)`, with whatever the caller put first as
`self`, so an object of the wrong class there is refused too.

Conversion to a base class is offset-adjusted, so it is correct for a base
that is not the first one:

```cpp
struct Swap : Observable, Priceable { ... };   // Priceable is not at offset 0
double value(const Priceable&);
```

```python
value(swap)   # reaches the Priceable subobject, not the start of the Swap
```

Virtual bases work too in Python: the offset is resolved at call time rather
than assumed.

The conversion is recorded by the derived class when it is bound, so the
base and the derived class may live in different modules and be loaded in
either order. Two cases are deliberately not convertible:

- A derived class whose module has never been loaded, because nothing has
  registered it yet.
- A base that C++ itself would not let you reach from that call site: one
  that is inaccessible (`private`/`protected`) or ambiguous because it is
  inherited twice non-virtually.

Inside an overload set a rejected argument simply moves to the next
candidate, so the error is raised only when no overload matches.

### What differs between the backends

| | Python | Lua | JS (N-API) | V8 (direct) |
|---|---|---|---|---|
| Wrong class refused | yes | yes | yes | untested |
| Derived-to-base, offset-adjusted | yes | yes | yes | untested |
| Same class bound by two modules | yes | yes | yes | untested |
| Derived class bound by *another* module | yes | yes | no | no |

The first three are what the identity check has to preserve. The fourth is
where the backends differ in how far their cross-module state reaches: Python
keeps it in `sys.modules` and Lua in the Lua registry, both shared by every
module in the process, while the N-API backend keeps its upcast table per
`.node` file — which matches the rest of that backend, where `to_javascript`
already falls back to a plain-object snapshot for a class the module did not
bind.

The V8 column is marked untested because it is. `javascript/mirror_bridge_v8.hpp`
does not compile against the V8 in the dev image (7.8, from `libnode-dev`): it
calls `String::NewFromUtf8Literal` and the one-argument
`String::NewFromUtf8`, both V8 8.x, and a bound-class parameter or data member
hits a declaration-order defect in `from_v8` besides. The gate it carries was
exercised by hand against a locally patched copy and behaved, but nothing in
CI covers it. Use the N-API backend.

Two modules recognise each other's objects by the class's reflected C++
spelling, so they have to spell it the same way. For every shape a discovered
class actually has — plain, nested, namespaced, inherited — clang-p2996 and
GCC 16 agree byte for byte, and `std::string` and `std::string_view` are
special-cased so they agree too. They do not agree on a key that names any
other standard-library template, because clang is only usable with libc++ and
that puts its ABI inline namespace in the name: `std::__1::vector<int, ...>`
against `std::vector<int, ...>`. Those keys reach error messages, not
identity, since a container can never be the expected class. The exception is
a class that *is* a template specialisation mentioning one — a hand-written
`bind_class<MyVec<int>>`, where `MyVec`'s defaulted allocator is spelled out,
or a discovered class deriving from one. Two such modules built by different
compilers would stop recognising each other, in a process where they are
already ABI-incompatible for other reasons.

### Forged identity

The check asks what a value *is*, so it is worth being precise about what it
trusts. Both backends read the answer out of memory the backend itself wrote
into the wrapper, after establishing that the memory is theirs:

- **Lua** reads the class's metatable address from the wrapper's payload,
  having first confirmed with `lua_touserdata` and `lua_rawlen` that the value
  is a full userdata long enough to hold one. It deliberately does *not* trust
  the metatable attached to the value, because Lua lets a script attach any
  metatable to any value — `setmetatable({}, getmetatable(curve))` needs no
  privileged library, and `debug.setmetatable` reaches userdata that belongs to
  other C libraries entirely. Nothing in Lua writes a full userdata's payload,
  so the payload cannot be forged the same way.
- **N-API** reads a magic word and then the class tag, because `napi_unwrap`
  establishes that an object was wrapped and not by whom: another addon's
  wrapped objects come back through it with that addon's payload layout.

Neither is a proof. A full userdata or a foreign payload whose first word
happened to match would be accepted, which is a coincidence a script cannot
aim for — it never learns the address and cannot write those bytes. N-API 8
(Node 14.17+) offers `napi_type_tag_object`, which would be a proof, at the
cost of a property lookup on every boundary crossing.

In Lua a plain table is still accepted where a bound class is expected: that
is how nested structs are written from Lua, and it reads the fields rather
than the object's bytes, so the check does not close it.

## Limitations

### Not Currently Supported

| Type | Status |
|------|--------|
| Raw pointers (T*) | Not supported |
| References as parameters | Partial support |
| Function pointers | Not supported |
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
