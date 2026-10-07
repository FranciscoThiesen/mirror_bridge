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

## Python Data Model

This section describes the Python backend. Lua and JavaScript bind neither
iteration nor serialization yet.

Four protocol slots are filled from the shape of the C++ class, with nothing
to declare per class.

| You write in C++ | Python gains |
|------------------|--------------|
| `begin()` / `end()` over convertible elements | `for x in obj`, `list(obj)`, `x in obj` |
| the above plus `size()` | `len(obj)`, and `bool(obj)` is `len(obj) != 0` |
| default-constructible, every member assignable | `pickle`, `copy.copy`, `copy.deepcopy` |

### Iteration

`tp_iter`/`tp_iternext` are generated from `begin()`/`end()`: `++it`, `*it`
and the sentinel comparison are splices, and the element goes through the
same conversion gate a method return value does. A range whose element has no
Python representation — a range of raw pointers, say — declines the slot
rather than failing the build. Membership needs nothing extra: with no
`sq_contains`, CPython answers `in` by iterating.

`end()` does not have to return the same type as `begin()`. A C++20 range
that ends at a sentinel iterates normally, and the element is taken from
`std::iterator_traits<It>::value_type` where the iterator has traits, so
`std::vector<bool>` yields `True`/`False` rather than the layout of the proxy
reference `*it` actually returns.

Iterating a `std::map`-shaped class yields `(key, value)` pairs, because that
is what the C++ range yields. `in` therefore tests pairs, not keys:
`("a", 1) in lookup` is `True` where `"a" in lookup` is `False`. This is the
one place where following the C++ range disagrees with the Python convention
that a mapping iterates its keys.

The iterator holds a reference to the object the range belongs to, so
`for x in make_bag()` is safe even though nothing else refers to the
container while the loop runs. This is the equivalent of pybind11's
`py::keep_alive<0, 1>()`, except that it is not something you can forget.

What ownership cannot fix is mutation: a `push_back` that reallocates
invalidates C++ iterators whoever holds the memory. The generated iterator
checks the two things it can check cheaply, and it is worth being precise
about what that does and does not cover.

| mutation during iteration | detected? |
|---|---|
| `push_back`, `insert`, `erase`, `clear` — the count moves | yes, `RuntimeError: Bag changed size during iteration` |
| `reserve`, `shrink_to_fit`, assigning a fresh container — the buffer moves, the count does not | yes, `RuntimeError: Bag reallocated its storage during iteration` |
| an `erase` paired with a `push_back` — same count, same buffer, elements shifted | **no** |
| anything at all, on a range whose iterator cannot be compared to another iterator | **no** |

The first check is the element count, the one CPython's own `dict` and `set`
iterators use; it needs `size()`, so a range without one does not get it. The
second re-reads `begin()` and compares, which is what catches the
reallocation that would otherwise read freed memory.

Neither check sees a mutation that preserves both the count and the buffer.
Such a range keeps iterating and yields shifted elements — wrong values read
from live memory, not a crash. **Mutating a container while iterating it is
undefined, exactly as it is in C++.** The guards turn the most common ways to
do it by accident into an exception; they are not a licence to do it.

### len() and truthiness

`len()` is offered only to a class that is both iterable and sized, never to
one that merely has a method spelled `size()`. The reason is truthiness: a
length slot is where CPython gets `bool(obj)` from, so gating on `size()`
alone would quietly turn `if obj:` into "it has elements" for a gauge whose
`size()` is a physical dimension. A class with `size()` and no range keeps
both its truthiness and its absence of `len()`.

A `size()` whose value will not fit a `Py_ssize_t` raises
`OverflowError: Bag.size() is 18446744073709551615, which cannot be a Python
length`. The realistic way to get there is an unsigned count that underflowed,
`items.size() - 1` on an empty container; reaching CPython as a negative
length instead produces only `SystemError: returned NULL without setting an
exception`.

### bool() is a behavior change

A class that is iterable and sized used to be truthy always, because nothing
in the type defined otherwise. It is now falsy when empty. This changes more
than `if obj:`:

```python
if bag: ...                  # now skipped when bag is empty
assert bag                   # now raises on an empty bag
bag or fallback              # now evaluates to fallback when bag is empty
[b for b in bags if b]       # now drops the empty ones
filter(None, bags)           # same
any(bags) / all(bags)        # now answer about emptiness
```

For a class that is really a container this is the Python semantics you want.
The case to look for is a container-shaped class used as a handle or a flag —
a `Connection` that happens to expose a queue through `begin()`/`end()` and
`size()`, where `if conn: conn.send(...)` used to mean "I have a connection"
and now means "my queue is non-empty". That reads the same and silently stops
sending.

The migration is to say what was meant: `if conn is not None:` for an
existence check, `if len(conn):` for a count. There is no per-class opt-out;
truthiness follows from the length slot, and a class gets the length slot only
by having both `begin()`/`end()` and `size()`. Removing `size()` from the
binding surface with `[[=exclude{}]]` is not an option either, since that
annotation applies to data members rather than methods.

One asymmetry is worth knowing: a range *without* `size()` has no length slot,
so it stays truthy even when empty. `bool()` therefore depends on whether the
class offers `size()`, not on whether it is a range.

A class with `operator[]` and `size()` but no `begin()`/`end()` is also not
covered. `mp_subscript` accepts whatever key `operator[]` takes, so reading a
`0..n-1` sequence out of it would be a guess; give the class `begin()` and
`end()` and it iterates.

### Pickling

`__reduce__` serializes the reflected non-static data member list — the same
walk `__repr__` uses, so a member added in C++ joins the pickle without
anyone editing a list. Reconstruction is `cls()` followed by `__setstate__`,
which is why a class qualifies only when Python can default-construct it and
assign every member back. Anything the state would not carry, or would not be
able to put back, disqualifies the class, and `pickle.dumps` raises
`TypeError: cannot pickle 'mod.Frozen' object` rather than handing back an
object that quietly lost a field:

| the class has | why it cannot round-trip |
|---|---|
| no default constructor | reconstruction is `cls()` |
| a `const` or `[[=readonly{}]]` member | read into the state, not assignable back out |
| a `private` member | not in the reflected visible member list, so the state never sees it |
| a `[[=exclude{}]]` member | same |
| a member declared by a base class | `nonstatic_data_members_of` reports direct members only |

The last three are the ones worth naming explicitly, because the member is
invisible from Python and an unpickled object would look right in `repr()`
while holding a default-constructed value for it.

A Python subclass pickles as itself, and its instance `__dict__` travels with
the C++ members.

For pickle to find the class again, a bound type's `tp_name` is
`<module>.<Class>`; CPython reads a static type's `__module__` from the text
before the last dot, and without it every bound class claimed to live in
`builtins`. `__name__`, `__qualname__` and `repr()` are unaffected.

### Hashing

No `tp_hash` is generated, which leaves two different behaviors depending on
whether the class defines `operator==`.

| the class | `hash()` | in a `set` or as a `dict` key |
|---|---|---|
| no `operator==` | identity hash | works; two equal-looking objects are two keys |
| has `operator==` | **unhashable** | `TypeError: unhashable type: 'mod.Point'` |

The second row is CPython's own rule, not a mirror_bridge decision:
`inherit_slots` copies `tp_hash` from the base only when `tp_richcompare` is
also being inherited, so a type that sets comparison and leaves hashing null
ends up with `__hash__` of `None` — the same state Python puts a class in when
it defines `__eq__` and no `__hash__`. It is self-consistent: you never get
`a == b` while `hash(a) != hash(b)`. It is also easy to trip over, because
`operator==` is the thing that takes the class out of a `set`:

```python
a == b          # True
{a, b}          # TypeError: unhashable type: 'mod.Point'
[x for x in xs if x == a]   # the workaround: a list and ==
```

Hashing the member values instead would be worse: every non-`const` member is
assignable from Python, and a hash that changes when the object is mutated
corrupts any `set` or `dict` already holding it. The safe opt-in — every
member `const` or `[[=readonly{}]]` — describes almost no real class.

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
