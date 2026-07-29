# Threading: GIL Release and Free-Threaded Python

## Releasing the GIL around C++ calls

By default, a bound method holds Python's GIL for its entire duration — a
long-running C++ call blocks every other Python thread. Opt a class into
GIL release and its method bodies run with the interpreter released:

```bash
# CLI: applies to every class in the module
mirror_bridge generate src/ --module engine --lang python --release-gil
```

```cpp
// Hand-written binding: per class
MIRROR_BRIDGE_MODULE(engine,
    mirror_bridge::bind_class<Simulation>(m, "Simulation").release_gil();
    mirror_bridge::bind_class<Config>(m, "Config");   // stays GIL-held
)
```

With the policy on, the sequence per call is: convert arguments (GIL held)
→ release → run the C++ body → re-acquire → convert the result. Four
Python threads each calling a 150ms native function finish in ~150ms
instead of ~600ms (`tests/gil_release/` measures exactly this).

### Why this is safe by construction

Unlike `pybind11::gil_scoped_release` — where calling anything that touches
Python without the GIL is undefined behavior you must reason about per
method — every mirror_bridge path that re-enters Python re-acquires the GIL
itself:

- `std::function` parameters: the callback wrapper uses `PyGILState_Ensure`
- Virtual overrides (trampolines): `dispatch_python` and
  `has_python_override` carry a GIL acquire guard — which also makes
  overridden virtuals callable from pure C++ threads that never held the GIL

What remains **your** responsibility is C++-side thread safety: two Python
threads calling methods on the *same* C++ object concurrently race on that
object unless it is internally synchronized. That contract is identical to
pybind11/nanobind.

### When to enable it

Enable for classes whose methods do real work (simulation steps, I/O,
image processing). Leave it off for getter/setter-style classes: the
release/acquire pair costs a few tens of nanoseconds per call, which is
noise on a 1ms call and pure overhead on a 70ns one.

## Free-threaded CPython (3.13+ `--disable-gil`)

On a free-threaded interpreter, any extension module that doesn't declare
a GIL policy causes CPython to **silently re-enable the GIL for the whole
process**. To declare a mirror_bridge module free-threading-ready, compile
it with `-DMB_FREE_THREADED`:

```bash
MB_CXX=g++ mirror_bridge generate src/ --module engine --lang python  # then add
# -DMB_FREE_THREADED via a custom build, or use mirror_bridge build with the flag
```

The declaration is deliberately opt-in (mirroring nanobind's
`FREE_THREADED` flag): mirror_bridge's own entry points are thread-state
correct, but declaring `Py_MOD_GIL_NOT_USED` asserts that *your* C++
classes tolerate concurrent access from Python threads.

## See also

- [tests/gil_release/](../../tests/gil_release/) — the parallelism regression test
- [Benchmarks](../internals/benchmarks.md) — per-call dispatch costs
