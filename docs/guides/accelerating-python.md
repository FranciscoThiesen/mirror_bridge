# Replacing a hot Python function with C++

This is the workflow for taking one slow function out of a Python program and
running it in C++ instead. It is written for whoever is doing the replacing,
including an agent working without a human in the loop, so every number and
every claim below was measured rather than estimated.

Nothing here requires you to write binding code. You write a header and the
generator reads it.

## The loop

```bash
mirror_bridge generate src --module hot --lang python --output build --json
```

```python
import sys; sys.path.insert(0, "build")
import hot
```

On a header with ten free functions and one class, `generate` takes about four
seconds end to end, including the reflection pass over the headers. `--pch`
takes it to about three. That is the whole iteration cost, so there is no
reason to batch up changes before trying them.

Free functions do not need a class to live in. A header of plain functions
produces a module of plain functions.

## Decide whether it is worth doing first

The speedup is not "C++ is faster than Python". It tracks how much
*interpreted per-element work* you remove, minus what it costs to marshal the
arguments across. Measured on the same machine, same data:

| What the Python version does | n | Speedup |
|---|---|---|
| `sorted(range(len(v)), key=lambda i: -v[i])[:10]` | 100,000 | **72.7x** |
| `[x/nrm for x in v]` after a generator sum | 100,000 | **8.3x** |
| `sum(a*b for a, b in zip(xs, xs))` | 1,000,000 | **4.4x** |
| a Python class accumulating in a list, 20,000 calls | 20,000 | **4.9x** |
| nested `for` loops over `str.split()` | 2,000 docs | **2.4x** |
| `sum(xs) / len(xs)` | 1,000,000 | **0.70x (slower)** |

The last row is the one to understand before writing anything. `sum()` is
already a C loop over the list, so there is no interpreted work left to
remove, and converting a million-element list into a `std::vector<double>`
costs more than the arithmetic saves. A function that is already one call into
a C builtin cannot be made faster this way.

What does pay:

- **A `key=` lambda, or any callback invoked per element.** That is one
  interpreter entry per element and it disappears entirely. This is where the
  72x comes from.
- **Nested loops and element-wise arithmetic written in Python.**
- **State that stays on the C++ side across many calls**, so the data is
  marshalled once instead of per call. Hold a bound class and call methods on
  it.

What does not pay:

- Anything already delegating to C: `sum`, `min`, `sorted` without a key,
  `str.join`, numpy.
- A function called a huge number of times with tiny arguments, where the call
  overhead is the whole cost.

## What crosses, and what to write

`docs/reference/type-conversion.md` has the full table. The parts that matter
most when you are writing a header from scratch:

```cpp
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>

namespace hot {

// Containers cross both ways. Take them by const reference.
double dot(const std::vector<double>& a, const std::vector<double>& b);

// Return by value. A returned pointer is refused, because a pointer says
// nothing about who owns the thing.
std::vector<double> normalize(const std::vector<double>& xs);

// Maps become dicts, pairs and tuples become tuples, optional becomes None.
std::unordered_map<std::string, int> word_count(const std::vector<std::string>& docs);
std::optional<double> first_above(const std::vector<double>& xs, double t);

// A std::function parameter accepts any Python callable, so you can keep a
// predicate on the Python side. It costs an interpreter entry per call, which
// is usually the thing you were trying to remove.
double apply(const std::vector<double>& xs, const std::function<double(double)>& f);

// State that stays in C++. Marshalled once at construction, not per call.
struct RollingStats {
    std::vector<double> window;
    std::size_t capacity = 16;

    void push(double v);
    double mean() const;
};

}  // namespace hot
```

Three things to avoid, each of which is refused by name at build time rather
than silently misbehaving:

- **Out-parameters.** `void sum_into(const std::vector<double>&, double& total)`
  does not bind. Python would pass a copy and the write would be lost, so the
  function would return `None` having done nothing. Return the value.
- **`std::span`.** A span does not own its storage. Take `const std::vector<T>&`.
- **Overloaded free functions.** Give one of them a distinct name.

## Two results that will surprise you

**A numeric vector comes back as `array.array`, not a `list`.** This is
deliberate: the conversion is a bulk copy of the buffer rather than building a
list of boxed floats, which is where a good part of the speedup above comes
from. From 1 KB up it is one copy, straight out of the C++ container; below
that it goes through an intermediate `bytes` object, because at that size the
extra call into the interpreter costs more than the extra copy. It indexes,
iterates and measures like a list, but it is not one:

```python
>>> hot.normalize([3.0, 4.0])
array('d', [0.6, 0.8])
```

So `json.dumps(result)` and `isinstance(result, list)` and `result + [x]` all
fail. If the caller needs a real list, say `list(result)` and pay for it
knowingly. `from_python` accepts both, so a round trip is fine.

**Out-of-range numbers raise instead of truncating.** Passing `2**40` to an
`int32_t` parameter is a `TypeError`, not a silent wrap. So is `3.7` to an
integer parameter, `-1` to an unsigned one, and a list with a `None` in it.
This is the opposite of the usual default and it is worth relying on.

## Read the build output, not just the exit code

`generate` reports every name you wrote that is not in the finished module,
with the reason:

```
✓ Successfully built bindings for: python

⚠ 3 names were not bound into 'hot' — Python raises AttributeError on them:

    1 class(es) the module declined
      Node — a member type has no converter; mark it [[=exclude{}]] or add a
      custom converter

    2 free function(s) the module declined
      out_param — a non-const reference parameter, whose writes cannot reach
      the caller: Python would pass a copy and the result would be lost.
      Return the value instead
      label — the return type has no converter
```

`--json` carries the same thing, which is the form to use when nothing is
reading the terminal:

```json
{
  "status": "ok",
  "imported": {"ok": true, "reason": "", "suggestion": ""},
  "declined": [
    {"kind": "function", "name": "out_param",
     "reason": "a non-const reference parameter, whose writes cannot reach ..."}
  ]
}
```

Three fields are worth branching on:

- `status` is `error` if any language failed to compile **or** if the built
  module does not import.
- `imported.ok` is the end-to-end check. `generate` imports the Python module
  it just built, because a `.so` that raises `ImportError` is not a result.
  `imported.suggestion` says what to do about it.
- `declined` is every name that did not make it. An empty array means
  everything you wrote crossed.

If `declined` is non-empty and you expected otherwise, fix the signature and
run again. It costs three seconds.

## Checking you actually won

The generator will not tell you whether the result is correct or faster. Both
are worth measuring before you delete the Python version:

```python
import math, timeit

reps = 20
py = timeit.timeit(lambda: py_normalize(xs), number=reps) / reps
cpp = timeit.timeit(lambda: hot.normalize(xs), number=reps) / reps
assert all(math.isclose(a, b) for a, b in zip(hot.normalize(xs), py_normalize(xs)))
print(f"{py/cpp:.2f}x")
```

Measure at the size the program actually uses, because the ratio moves with
it. Across n=100 to n=1,000,000, `dot` held between 5.7x and 4.4x, drifting
down as the conversion cost grew. `mean` was a loss at every size and a worse
one as n rose, 0.93x to 0.70x: its conversion cost grew the same way and there
was never any interpreted work to pay for it.

## Related

- [Type conversion reference](../reference/type-conversion.md) -- the full table
- [CLI reference](../reference/cli.md) -- every flag
- [Errors](../reference/errors.md) -- what each diagnostic means
- [Threading](threading.md) -- `--release-gil`, for a long C++ call that should
  not block other Python threads
