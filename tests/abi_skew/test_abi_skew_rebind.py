"""Two modules that both bind the same class, at two different sizes.

bind_class keeps its PyTypeObject in a static local of a function template.
Compiled with hidden visibility, as the CLI, the CMake helper and the test
harness all do, each module keeps its own. Without it the dynamic linker
hands both modules the first copy and runs only the first initialiser, so
the second module gets a type whose tp_basicsize belongs to the first and
allocates its larger payload into it. That is measured on GCC 16 at 176
bytes into a 48-byte object.

Either outcome is correct, and which one happens is the build's business:
two honest type objects, or a refused import that names the disagreement.
The one thing that must never happen is both modules importing and sharing a
single type object, because that is the overflow.
"""
import sys

import both_small

failures = []


def check(ok, what):
    print(f"  {'PASS' if ok else 'FAIL'}  {what}")
    if not ok:
        failures.append(what)


small_size = both_small.Thing.__basicsize__
print(f"both_small sized its wrapper at {small_size} bytes")

try:
    import both_big
except Exception as exc:
    # bind_class raises ImportError and the module initialiser returns a
    # module anyway, so CPython reports its own SystemError with ours as the
    # context. The message that matters is the one underneath.
    message = str(exc.__context__ or exc)
    print(f"  the second import was refused: {type(exc).__name__}")
    check("mirror_bridge" in message and "different definition" in message,
          f"and it says why: {message[:110]}")
    check(str(small_size) in message,
          "naming the size the other module built against")
else:
    big_size = both_big.Thing.__basicsize__
    print(f"both_big sized its wrapper at {big_size} bytes")
    check(both_small.Thing is not both_big.Thing,
          "the two modules did not end up sharing one type object")
    check(big_size != small_size,
          f"and each type reports its own size ({small_size} and {big_size})")
    t = both_big.Thing()
    t.a = 9.0
    check(t.a == 9.0, "the second module's object is usable")

print()
if failures:
    print(f"FAIL: {len(failures)} check(s)")
    sys.exit(1)
print("PASS: two builds of one class never share a wrapper layout")
