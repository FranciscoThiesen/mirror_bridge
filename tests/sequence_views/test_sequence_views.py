#!/usr/bin/env python3
"""A std::vector data member is a live view of the C++ vector, not a copy.

Before this, `b.items.append(9)` and `b.items[0] = 9` were both accepted and
both silently discarded: the getter built a fresh array.array or list on every
read, the mutation landed on that temporary, and the temporary was dropped at
the end of the statement. No error anywhere. Reading the member also
reallocated and refilled the whole sequence every time, so walking a member
vector of a million doubles copied eight megabytes per read.

This is the container half of what member views did for class members, and it
follows the same rules: a const member keeps its copy, and a method return
keeps its copy because interiority cannot be established for one.
"""

import array
import gc
import os
import sys
from collections.abc import MutableSequence

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build', 'tests'))

import sequence_views as sv

failures = []


def check(cond, label):
    if cond:
        print(f"  PASS  {label}")
    else:
        print(f"  FAIL  {label}")
        failures.append(label)


print("Test 1: a mutation through a vector member reaches C++")
b = sv.Bag()
b.ints.append(5)
b.ints.append(7)
check(b.total() == 12, f"append reaches C++: total() == {b.total()}")
b.ints[0] = 10
check(b.total() == 17, f"item assignment reaches C++: total() == {b.total()}")
b.ints.extend([1, 2])
check(b.total() == 20, f"extend reaches C++: total() == {b.total()}")
b.ints.insert(0, 3)
check(b.total() == 23 and list(b.ints) == [3, 10, 7, 1, 2], f"insert: {list(b.ints)}")
check(b.ints.pop() == 2 and b.total() == 21, f"pop returns and shrinks: {list(b.ints)}")
check(b.ints.pop(0) == 3 and b.total() == 18, f"pop(i): {list(b.ints)}")
del b.ints[0]
check(b.total() == 8, f"del reaches C++: {list(b.ints)}")
b.ints.clear()
check(b.total() == 0 and len(b.ints) == 0, "clear reaches C++")

print()
print("Test 2: it behaves like the sequence it claims to be")
b = sv.Bag()
b.ints = [1, 2, 3]
check(isinstance(b.ints, MutableSequence), "registers as a MutableSequence")
check(len(b.ints) == 3, "len")
check(b.ints == [1, 2, 3], "equal to an equal list")
check(b.ints == (1, 2, 3), "equal to an equal tuple")
check(b.ints != [1, 2], "unequal to a shorter list")
check(b.ints != 7, "unequal to a non-sequence rather than raising")
check(not (b.ints == [1, 2, 3, 4]), "unequal to a longer list")
check(2 in b.ints and 9 not in b.ints, "membership")
check(b.ints[-1] == 3, "negative index")
check(list(b.ints) == [1, 2, 3], "iteration")
check([x * 2 for x in b.ints] == [2, 4, 6], "iteration in a comprehension")
check(repr(b.ints) == "[1, 2, 3]", f"repr reads like a list: {repr(b.ints)}")
check(b.ints.tolist() == [1, 2, 3] and isinstance(b.ints.tolist(), list),
      "tolist() gives a real list")

print()
print("Test 2b: the MutableSequence surface the stub promises really exists")
# register() makes isinstance say yes without bringing the ABC's mixins, so
# every method a MutableSequence is expected to have is checked here.
b = sv.Bag()
b.ints = [5, 7, 5]
check(b.ints.index(7) == 1, "index")
check(b.ints.index(5, 1) == 2, "index with a start")
check(b.ints.count(5) == 2, "count")
b.ints.remove(5)
check(list(b.ints) == [7, 5], f"remove: {list(b.ints)}")
b.ints.reverse()
check(list(b.ints) == [5, 7], f"reverse: {list(b.ints)}")
check(list(reversed(b.ints)) == [7, 5], "reversed()")
b.ints += [1]
check(list(b.ints) == [5, 7, 1] and b.total() == 13, f"+= extends in place: {list(b.ints)}")
for missing, label in [(lambda: b.ints.index(99), "index"), (lambda: b.ints.remove(99), "remove")]:
    try:
        missing()
        check(False, f"{label} of a missing value should raise")
    except ValueError:
        check(True, f"{label} of a missing value raises ValueError")
try:
    hash(b.ints)
    check(False, "a mutable view must not be hashable")
except TypeError:
    check(True, "unhashable, like list")

print()
print("Test 3: a slice is a copy, deliberately")
b = sv.Bag()
b.ints = [1, 2, 3]
sliced = b.ints[0:2]
check(isinstance(sliced, list) and sliced == [1, 2], f"a slice gives a list: {sliced!r}")
sliced.append(99)
check(list(b.ints) == [1, 2, 3], "mutating the slice does not reach C++")
check(b.ints[::-1] == [3, 2, 1], f"a strided slice: {b.ints[::-1]}")
try:
    b.ints[0:2] = [8, 9]
    check(False, "slice assignment should be refused, not silently dropped")
except TypeError as exc:
    check("assign the whole attribute" in str(exc),
          "slice assignment raises and says what to do instead")

print()
print("Test 4: out of range and bad types raise")
for bad, exc_type, label in [
    (lambda: b.ints[3], IndexError, "index past the end"),
    (lambda: b.ints[-4], IndexError, "index before the start"),
    (lambda: b.ints["x"], TypeError, "a string index"),
]:
    try:
        bad()
        check(False, f"{label} should raise")
    except exc_type:
        check(True, f"{label} raises {exc_type.__name__}")

try:
    b.ints[0] = "not an int"
    check(False, "assigning a str to an int vector should raise")
except TypeError:
    check(True, "assigning a str to an int vector raises TypeError")

print()
print("Test 5: string and class elements")
b = sv.Bag()
b.names.append("alpha")
b.names.append("beta")
check(b.name_count() == 2 and list(b.names) == ["alpha", "beta"],
      f"strings reach C++: {list(b.names)}")
b.names[0] = "renamed"
check(list(b.names) == ["renamed", "beta"], "string element assignment")

b.points.append(sv.Point())
# An element of a class vector is itself a view, so this writes into the
# vector's storage rather than into a copy of the element.
b.points[0].x = 5.0
check(b.points[0].x == 5.0, f"writing through a class element: {b.points[0].x}")
check(len(b.points) == 1, "and did not grow the vector")

print()
print("Test 6: what deliberately still copies")
b = sv.Bag()
b.doubles = [1.0, 2.0]
check(isinstance(b.doubled(), array.array),
      f"a method return is still array.array, not a view: {type(b.doubled()).__name__}")
check(isinstance(b.frozen, array.array),
      f"a const member still copies: {type(b.frozen).__name__}")
check(isinstance(b.flags, list),
      f"a vector<bool> still copies: {type(b.flags).__name__}")
b.doubles = [3.0, 4.0]
check(b.sum_doubles() == 7.0, "assigning the whole member still works")

print()
print("Test 6b: the class repr renders each member the way reading it does")
# These used to disagree: the repr printed array('d', [...]) for a member
# that now reads as a view, so the repr claimed a type the attribute does
# not hand back.
b = sv.Bag()
b.ints = [1, 2, 3]
text = repr(b)
check("ints=[1, 2, 3]" in text, f"a live member prints as a list: {text[:60]}")
check("frozen=array('d', [1.0, 2.0])" in text,
      f"a const member still prints as the array.array it still gives: {text[:90]}")

print()
print("Test 7: the buffer is the vector's own storage")
b = sv.Bag()
b.doubles = [1.0, 2.0, 3.0]
mv = memoryview(b.doubles)
check(mv.format == "d" and mv.shape == (3,), f"format and shape: {mv.format} {mv.shape}")
mv[1] = 99.0
check(b.sum_doubles() == 103.0, "a write through the memoryview reaches C++")

# A resize would reallocate the storage the memoryview is pointing at, so it
# is refused while any buffer is outstanding. The count is kept per C++
# container and not per view object, because every attribute read builds a
# new view of the same vector.
for label, op in [
    ("append", lambda: b.doubles.append(4.0)),
    ("insert", lambda: b.doubles.insert(0, 4.0)),
    ("pop", lambda: b.doubles.pop()),
    ("clear", lambda: b.doubles.clear()),
    ("del", lambda: b.doubles.__delitem__(0)),
]:
    try:
        op()
        check(False, f"{label} must not resize while a buffer is exported")
    except BufferError:
        check(True, f"{label} is refused while a buffer is exported")

b.doubles[0] = 7.0
check(b.doubles[0] == 7.0, "an in-place write is still allowed while exported")

second = memoryview(b.doubles)
mv.release()
try:
    b.doubles.append(4.0)
    check(False, "still pinned while a second buffer is outstanding")
except BufferError:
    check(True, "still pinned while a second buffer is outstanding")
second.release()
b.doubles.append(4.0)
check(len(b.doubles) == 4, "resizing works once every buffer is released")

try:
    import numpy as np
    b2 = sv.Bag()
    b2.doubles = [1.0, 2.0, 3.0]
    arr = np.asarray(b2.doubles)
    check(arr.dtype == np.float64 and list(arr) == [1.0, 2.0, 3.0], "numpy sees it")
    arr[0] = -1.0
    check(b2.sum_doubles() == 4.0, "a write through numpy reaches C++ (zero copy)")
    del arr
except ImportError:
    print("  SKIP  numpy is not installed here")

print()
print("Test 8: lifetime and refcounts")
b = sv.Bag()
b.ints = [1, 2, 3]
view = b.ints
del b
gc.collect()
check(list(view) == [1, 2, 3], "a view keeps its owner alive")

b = sv.Bag()
b.ints = [1, 2, 3]
before = sys.getrefcount(b)
for _ in range(2000):
    v = b.ints
    del v
gc.collect()
check(sys.getrefcount(b) == before,
      f"2000 reads leak no references ({before} -> {sys.getrefcount(b)})")

elem_owner = sv.Bag()
elem_owner.points.append(sv.Point())
before = sys.getrefcount(elem_owner)
for _ in range(2000):
    p = elem_owner.points[0]
    del p
gc.collect()
check(sys.getrefcount(elem_owner) == before,
      f"2000 element reads leak no references ({before} -> {sys.getrefcount(elem_owner)})")

print()
print("Test 9: an empty vector")
b = sv.Bag()
check(len(b.ints) == 0 and list(b.ints) == [] and repr(b.ints) == "[]", "reads as empty")
check(b.ints == [], "equals the empty list")
try:
    b.ints.pop()
    check(False, "popping an empty vector should raise")
except IndexError:
    check(True, "popping an empty vector raises IndexError")
check(len(memoryview(b.doubles)) == 0, "an empty numeric vector still exports a buffer")

print()
if failures:
    print(f"FAIL: {len(failures)} check(s)")
    for f in failures:
        print(f"  - {f}")
    sys.exit(1)
print("PASS: a vector member is a live view of the C++ vector")
