#!/usr/bin/env python3
"""Which methods get a generated batch sibling, and what the sibling does.

`--batch` gives a method taking and returning one arithmetic value a sibling
that takes the whole sequence, so Python crosses the boundary once instead of
once per element. The eligibility rule is narrow on purpose, and this pins it
by behaviour: every shape in batched.hpp is here, including the ones that must
not get a sibling.

Run by tests/batch_methods/test_batch.sh, twice: once against a module built
with -DMIRROR_BRIDGE_BATCH_METHODS and once without. The expected set of
generated names differs, and nothing else about the module may.
"""
import array
import os
import sys

import batched

ENABLED = os.environ.get("MB_BATCH_ENABLED") == "1"

failures = []


def check(ok, what):
    print(f"  {'✓' if ok else '✗'} {what}")
    if not ok:
        failures.append(what)


names = {n for n in dir(batched.Batched) if not n.startswith("_")}
generated = {n for n in names if n.endswith("_batch")}

# manual_batch is declared in the header, so it is present either way and is
# not something this feature generated.
AUTHORS_OWN = {"manual_batch"}
EXPECTED = {"scale_batch", "quantize_batch", "accumulate_batch"} if ENABLED else set()

print(f"generation is {'on' if ENABLED else 'off'}:")
check(generated == EXPECTED | AUTHORS_OWN,
      f"the generated set is exactly {sorted(EXPECTED)}")

print("shapes that must never get one:")
for absent, why in (
    ("reset_batch", "a method with no parameter"),
    ("mix_batch", "two parameters: nothing says how they pair up"),
    ("label_batch", "a std::string return"),
    ("parse_batch", "a std::string parameter"),
    ("positive_batch", "a bool return: vector<bool> is the proxy type"),
    ("flip_batch", "a bool parameter"),
    ("note_batch", "a void return"),
    ("over_batch", "an overloaded name: resolving per element is the cost"),
):
    check(absent not in names, f"no {absent} -- {why}")

b = batched.Batched()

print("the author's own batch method is untouched:")
# manual(x) is x - 1.0; the hand-written manual_batch multiplies by 1000, so a
# generated one would be caught by the value, not just the name.
check(b.manual(5.0) == 4.0, "manual itself is bound")
check(list(b.manual_batch([5.0])) == [4000.0],
      "manual_batch is the author's, not a generated one")

if not ENABLED:
    print()
    if failures:
        print(f"FAIL: {len(failures)} check(s)")
        for f in failures:
            print(f"  - {f}")
        sys.exit(1)
    print("PASS: nothing is generated without the flag")
    sys.exit(0)

print("the generated sibling computes what the method computes:")
xs = [1.0, 2.0, 3.0, 4.0]
check(list(b.scale_batch(xs)) == [b.scale(x) for x in xs],
      "scale_batch equals the loop, element for element")
check(list(b.quantize_batch([1.5, 2.5])) == [15, 25],
      "a differing return type converts correctly")

print("state carries across the batch, which a comprehension cannot fake:")
fresh = batched.Batched()
check(list(fresh.accumulate_batch([0.0, 0.0, 0.0])) == [1.0, 2.0, 3.0],
      "accumulate_batch threads the object's state through the loop")
check(fresh.calls == 3, "and leaves the object in the state the loop would")

print("inputs it accepts:")
check(list(b.scale_batch(array.array("d", [4.0, 5.0]))) == [8.0, 10.0],
      "an array.array, which is what another batch call returns")
check(list(b.scale_batch([])) == [], "an empty sequence")
check(list(b.scale_batch((1.0, 2.0))) == [2.0, 4.0], "a tuple")
check(list(b.quantize_batch([1, 2])) == [10, 20],
      "ints where the parameter is double")

print("inputs it refuses:")
for bad, what in (("not a sequence", "a string"), (5.0, "a bare number"),
                  ([1.0, "x"], "a sequence with a non-number in it"),
                  ([1.0, None], "a sequence with None in it")):
    try:
        b.scale_batch(bad)
        check(False, f"{what} raises")
    except TypeError:
        check(True, f"{what} raises TypeError")

print("the stub says the sibling takes no keyword, because it cannot:")
# METH_O accepts one positional argument and nothing else. Without a `/` in
# the stub a type checker approves scale_batch(xs=[...]), which fails at
# runtime, and the keyword-capability answer would be derived twice: once
# from ml_flags here and once from is_static in the stub generator.
stubs = batched.__mirror_bridge_stubs__()
line = [l for l in stubs.splitlines() if "def scale_batch(" in l]
check(bool(line), "the stub declares scale_batch")
if line:
    check("/" in line[0], f"and marks it positional-only: {line[0].strip()}")
try:
    b.scale_batch(xs=[1.0])
    check(False, "a keyword call is refused at runtime")
except TypeError:
    check(True, "a keyword call is refused at runtime, as the stub says")

print("the result is the type the conversion table promises:")
out = b.scale_batch([1.0, 2.0])
check(type(out).__name__ == "array" and out.typecode == "d",
      "a double batch returns array('d'), like any numeric vector")

print()
if failures:
    print(f"FAIL: {len(failures)} check(s)")
    for f in failures:
        print(f"  - {f}")
    sys.exit(1)
print("PASS: generated batch methods behave like the method they batch")
