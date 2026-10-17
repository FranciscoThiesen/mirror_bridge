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
| `std::vector<T>` returned from a method (numeric T) | `array.array` (bulk memcpy) | `table` (array) | `Array` |
| `std::vector<T>` returned from a method (other T) | `list` | `table` (array) | `Array` |
| `std::vector<T>` read as a **data member** | live view (see below) | `table` (array) | `Array` |
| `std::array<T, N>` | `list` | `table` (array) | `Array` |
| `std::deque<T>` | `list` | `table` (array) | `Array` |
| `std::list<T>` | `list` | `table` (array) | `Array` |

**A vector data member is a live view, not a copy.** Reading one gives an
object that holds a pointer to the C++ vector itself and a reference to its
owner, so `obj.points.append(p)` and `obj.points[0] = p` reach the C++ object
instead of mutating a temporary that is then discarded. It registers as a
`collections.abc.MutableSequence`, which is what the `.pyi` says it is, and
supports `len`, indexing, negative indices, iteration, `in`, `==` against any
sequence, `append`, `extend`, `insert`, `pop`, `clear`, `del` and `tolist()`.
A numeric one also exports the buffer protocol, so `numpy.asarray(obj.xs)`
shares the vector's own storage rather than copying it.

Because the view does not copy, reading a member is O(1) instead of O(n):
measured on a `vector<double>` member, 122 ns to 44 ns at ten elements and
54.0 us to 44 ns at a hundred thousand.

Three cases deliberately keep the copy they always had:

- **A method return.** A reference out of a method cannot be shown to point
  inside the receiver, so there is nothing to keep alive and nothing to view.
- **A `const` member.** A writable view of one would get around what C++ said.
- **`std::vector<bool>`.** It packs its elements and yields a proxy, so there
  is no element to take a reference to.

A slice (`v[:]`), `list(v)` and `v.tolist()` each give a real copy, which is
how you take a snapshot. Resizing is refused with `BufferError` while a
`memoryview` or numpy array is outstanding, because it would reallocate the
storage they point at. Holding an element of a vector of classes across an
`append` has the same hazard the underlying C++ reference would.

**Bulk transfer optimization**: For `vector<float>`, `vector<double>`, `vector<int>`, and other numeric types, Python bindings copy the buffer in bulk into an `array.array` object instead of converting element by element. This is ~10-50x faster for large arrays. At or above 1 KB the buffer is copied once, out of the C++ container through a borrowed `memoryview`; below that it goes through an intermediate `bytes` object, which is two copies but one fewer call into the interpreter and measurably cheaper at that size. The crossover is a measured one and can be moved with `-DMIRROR_BRIDGE_ARRAY_BORROW_BYTES=<n>`. The `from_python` direction accepts both `list` and `array.array` inputs.

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
print(c.numbers)  # array('i', [1, 2, 3, 4, 5])  -- see the note above
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
| `enum` | `enum.IntEnum` | `number` | `number` |
| `enum class` | `enum.IntEnum` | `number` | `number` |

The Python backend exports the enum *type* as an `enum.IntEnum` whose members
carry the C++ enumerator names and values. Nothing about the *value*
conversion changed, because an `IntEnum` member is an `int`: a function that
took a bare `int` still takes one, and one that returns an enum still returns
a plain `int`, which compares equal to the member. An enumerator whose name is
a Python keyword gets a trailing underscore, since `Permission.None` is a
syntax error rather than an attribute lookup. Lua and JavaScript have no enum
object to map onto and their values keep crossing as numbers.

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
