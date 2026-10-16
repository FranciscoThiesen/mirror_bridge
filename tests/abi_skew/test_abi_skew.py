"""Two modules that disagree about the size of the same C++ class.

The forward type registry is keyed by typeid name, and two revisions of a
header produce the same typeid name for different layouts. A module that
returns xm::Thing by value asks the registry for a type object to put the
result in; if it is handed one that another module sized for a smaller
Thing, tp_alloc allocates the smaller size and the copy is placement-
constructed past the end of it.

Keying on the wrapper's size as well as its ABI tag makes the lookup miss
instead, so the result degrades to a dict snapshot. Degraded is the point:
there is no honest wrapper to hand back.

Without the size in the key this test segfaults rather than failing, which
is why it allocates neighbours to be trampled: the two modules alone come
back looking fine.
"""
import sys

# Import order matters: skew_small registers its Thing in the shared registry
# first, so it is the type skew_big's lookup could find.
import skew_small
import skew_big

failures = []


def check(ok, what):
    print(f"  {'PASS' if ok else 'FAIL'}  {what}")
    if not ok:
        failures.append(what)


small_size = skew_small.Thing.__basicsize__
print(f"skew_small sized its wrapper at {small_size} bytes")

# pymalloc serves same-size objects from one pool, back to back, so these sit
# where an oversized payload written into a neighbouring slot would land.
neighbours = [skew_small.Thing() for _ in range(64)]
for i, n in enumerate(neighbours):
    n.a = float(i)

results = [skew_big.Factory().make() for _ in range(64)]

print("what skew_big's by-value return came back as:")
check(not isinstance(results[0], skew_small.Thing),
      "it is not the other module's Thing, which was sized for a smaller class")
check(isinstance(results[0], dict),
      f"it is the dict snapshot the unregistered path falls back to, got "
      f"{type(results[0]).__name__}")
check(results[0].get("a") == 7.0, "and it carries the value the method set")

print("what the other module's objects look like afterwards:")
damaged = [i for i, n in enumerate(neighbours) if n.a != float(i)]
check(not damaged,
      f"none of the 64 neighbouring objects were overwritten"
      + (f", {len(damaged)} were" if damaged else ""))

print()
if failures:
    print(f"FAIL: {len(failures)} check(s)")
    sys.exit(1)
print("PASS: a module built against a different layout does not borrow the type")
