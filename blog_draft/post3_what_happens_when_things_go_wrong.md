# mirror_bridge: What Happens When Things Go Wrong

*Every binding library makes the happy path look easy. The real test is the sad path.*

---

In my [first post](/Mirror-Bridge/), I showed how mirror_bridge eliminates binding boilerplate — zero lines of glue code, thanks to C++26 reflection. In the [second](/Mirror-Bridge-Multi-Language/), I benchmarked it against V8, PyPy, and LuaJIT. Both posts focused on what happens when everything works.

This post is about what happens when it doesn't.

Because in production, things go wrong. A function divides by zero. A user passes the wrong type. A class has a member that can't be converted. And the quality of a binding library isn't measured by how pretty the happy path is — it's measured by how much it helps you when the floor falls out.

## The Three Failure Modes

When you're crossing language boundaries, there are exactly three places things break:

1. **Compile time** — Your C++ class has a type that the binding library can't handle
2. **Runtime (C++ side)** — Your C++ code throws an exception
3. **Runtime (language side)** — The Python/Lua/JS caller passes invalid data

Let me show you how mirror_bridge handles all three, and why it matters.

## Failure Mode 1: Unconvertible Types

Suppose you have a class like this:

```cpp
struct Processor {
    int id = 0;
    std::string name = "default";
    void (*callback)(int);       // function pointer — not convertible
};
```

I compiled this exact class with both pybind11 (v3.0.3) and mirror_bridge to see what each produces. Here's the pybind11 binding:

```cpp
py::class_<Processor>(m, "Processor")
    .def(py::init<>())
    .def_readwrite("id", &Processor::id)
    .def_readwrite("name", &Processor::name)
    .def_readwrite("callback", &Processor::callback);
```

The result: **73 lines of error output**, starting deep in `pybind11/cast.h` with messages like `variable 'ptr' with type 'const auto *' has incompatible initializer of type 'element_type *' (aka 'void (*)(int)')` followed by a chain of template instantiation notes spanning `type_caster_base.h`, `pybind11.h`, and several helper classes. None of the 73 lines say "function pointers aren't supported" or suggest what to do about it.

Here's the mirror_bridge version — no binding code needed, just `bind_class<Processor>(m, "Processor")`:

```
error: static assertion failed:
  bind_class<T>: T contains members with types that mirror_bridge
  cannot convert. Mark unconvertible members with [[=exclude{}]]
  or add a custom type converter.
  Supported: arithmetic, std::string, containers, smart pointers,
  std::optional, std::expected, enums, and nested bindable classes.
```

**3 error lines.** The first one tells you the problem (unconvertible member type) and how to fix it (exclude it or add a converter). No digging through template instantiation chains.

This is possible because C++26 reflection lets us walk the member list at compile time and validate each type *before* attempting conversion. The validation runs automatically in `bind_class<T>()` across all backends. You can also use it standalone:

```cpp
MIRROR_BRIDGE_VALIDATE(MyClass);  // fires at compile time
```

## Failure Mode 2: C++ Exceptions

Here's a class that can throw:

```cpp
struct MathService {
    double safe_divide(double a, double b) {
        if (b == 0.0) throw std::runtime_error("division by zero");
        return a / b;
    }
};
```

In early mirror_bridge, if `safe_divide` threw, it would crash the host runtime — the Lua VM, the Node.js process, or the Python interpreter. Any binding library that doesn't catch exceptions at the language boundary has this problem.

Now, every method call in every language backend is wrapped in try/catch. The exception becomes a native error in each language:

**Python:**
```python
try:
    svc.safe_divide(10.0, 0.0)
except RuntimeError as e:
    print(e)  # "division by zero"
```

**Lua:**
```lua
local ok, err = pcall(function()
    svc:safe_divide(10.0, 0.0)
end)
print(err)  -- "C++ exception: division by zero"
```

**JavaScript:**
```javascript
try {
    svc.safe_divide(10.0, 0.0);
} catch (e) {
    console.error(e.message);  // "division by zero"
}
```

This covers instance methods, static methods, constructors, and property accessors. If C++ throws anywhere in the binding boundary, the host language gets a catchable error — not a segfault.

**"Doesn't try/catch in every call hurt performance?"** No. Clang uses table-based exception handling: the `try` block emits zero extra instructions on the happy path. The compiler generates a side table (in `.gcc_except_table`) that's only consulted during stack unwinding. I measured it:

| | ns/call |
|--|---------|
| Without try/catch | 1.64 ns |
| With try/catch | 1.72 ns |
| **Overhead** | **0.08 ns** |

For context, a single `PyFloat_FromDouble` + `Py_DECREF` costs 4.5 ns — 56x more than the try/catch overhead. A full binding call (argument parsing, type conversion, result boxing) is 30-100 ns. The safety is free.

## std::expected: The Modern Alternative

C++23 introduced `std::expected<T, E>` as an alternative to exceptions. It's the return type that says "this might fail, and here's the error type."

mirror_bridge converts it idiomatically for each language:

```cpp
struct MathService {
    std::expected<double, std::string> safe_divide(double a, double b) {
        if (b == 0.0) return std::unexpected("division by zero");
        return a / b;
    }
};
```

**Python** — success returns the value, error raises `ValueError`:
```python
result = svc.safe_divide(10.0, 2.0)   # 5.0
svc.safe_divide(10.0, 0.0)            # raises ValueError
```

**Lua** — idiomatic multi-return (value, err):
```lua
local result, err = svc:safe_divide(10.0, 2.0)  -- 5.0, nil
local result, err = svc:safe_divide(10.0, 0.0)  -- nil, "division by zero"
```

**JavaScript** — success returns, error throws:
```javascript
const result = svc.safe_divide(10.0, 2.0);  // 5.0
svc.safe_divide(10.0, 0.0);                 // throws Error
```

No special annotation needed. No registration. Just return `std::expected` from your C++ method and the binding does the right thing.

## Failure Mode 3: The Round-Trip Problem

Here's a subtle one. You have a C++ class with a `vector<float>`:

```cpp
struct Signal {
    std::vector<float> samples;
};
```

For performance, mirror_bridge converts numeric vectors to Python's `array.array` using a single `memcpy` instead of creating N individual Python float objects. For 10,000 floats, this is ~30x faster.

But what happens when the user writes back?

```python
sig = Signal()
data = sig.samples          # Returns array.array('f', [...])
sig.samples = data          # Does this work?
```

If `from_python` only accepted `list`, this round-trip would silently fail. We fixed this by accepting any Python sequence type — `list`, `tuple`, `array.array`, even NumPy arrays — using the sequence protocol instead of a `PyList_Check` gate:

```cpp
if (!PySequence_Check(obj) || PyUnicode_Check(obj) || PyBytes_Check(obj)) {
    return false;
}
```

This means you can do:

```python
import array
sig.samples = [1.0, 2.0, 3.0]                    # list — works
sig.samples = array.array('f', [1.0, 2.0, 3.0])  # array — works
sig.samples = (1.0, 2.0, 3.0)                     # tuple — works
```

Round-trip correctness matters. If `get` and `set` don't speak the same language, your users will file bugs.

## The Developer Loop: watch + diff

Two more things that make the sad path less sad.

**`mirror_bridge watch`** monitors your headers and recompiles on change:

```bash
mirror_bridge watch src/ --module my_lib --lang python
```

No more switching terminals, running the build command, switching back. Change a header, save, bindings rebuild automatically.

**`mirror_bridge diff`** tells you what changed in your binding surface:

```bash
mirror_bridge diff src/
# Binding Surface Changes:
# + 2 added  - 1 removed
```

Catch accidentally removed methods or surprise ABI breaks before your users do.

## What's Next

mirror_bridge now generates bindings for four languages (Python, Lua, JavaScript, Rust), handles errors cleanly across all of them, validates types at compile time, and gives you developer tools for a fast iteration loop.

But we're just getting started. The roadmap includes:

- **`std::span` and range view** support for zero-copy access
- **`mirror_bridge scout`** — profile a Python project, find hotspots, auto-generate C++ replacements
- **Compile-time parallelism** for multi-module projects

If you want to try it:

```bash
git clone https://github.com/FranciscoThiesen/mirror_bridge
cd mirror_bridge && ./start_dev_container.sh
./tests/run_all_tests.sh
```

Or open it in your browser: [![Open in GitHub Codespaces](https://github.com/codespaces/badge.svg)](https://codespaces.new/FranciscoThiesen/mirror_bridge)

---

*mirror_bridge is Apache 2.0 licensed. Star it on [GitHub](https://github.com/FranciscoThiesen/mirror_bridge) if you find it useful.*
