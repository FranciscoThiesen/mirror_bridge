#!/usr/bin/env python3
"""What a ColumnView does once it reaches Python.

The claim this feature makes is that the view is a drop-in for the list it
replaces, so most of what is here is the sequence behaviour rather than the
speed. Three of these cases were failures found in review:

  * an empty vector gave numpy a null data pointer, so np.asarray raised on an
    ordinary no-results return
  * a struct with a member called f1 collided with the name numpy gives an
    unnamed padding entry at index 1, and np.asarray raised
  * slicing was a TypeError and a `match` pattern silently fell through to the
    next case, which is worse than an error

Run by tests/columnar/test_columnar.sh against a module built with
-DMIRROR_BRIDGE_COLUMNAR_VECTORS.
"""
import collections.abc as abc
import copy
import sys

import colv
import numpy as np

failures = []


def check(ok, what):
    print(f"  {'✓' if ok else '✗'} {what}")
    if not ok:
        failures.append(what)


src = colv.Source()
src.fill(6)
pts = src.get_points()

print("the view stands in for the list:")
check(len(pts) == 6, "len()")
check(pts[2].y == 4.0, "indexing")
check(pts[-1].z == 15.0, "negative indexing")
check([p.x for p in pts] == [0.0, 1.0, 2.0, 3.0, 4.0, 5.0], "iteration")
check([p.x for p in pts[1:4]] == [1.0, 2.0, 3.0], "slicing")
check([p.x for p in pts[::2]] == [0.0, 2.0, 4.0], "a strided slice")
check([p.x for p in reversed(pts)] == [5.0, 4.0, 3.0, 2.0, 1.0, 0.0], "reversed()")
check(isinstance(pts, abc.Sequence), "isinstance(pts, collections.abc.Sequence)")
check([p.x for p in copy.copy(pts)] == [0.0, 1.0, 2.0, 3.0, 4.0, 5.0], "copy.copy")
check(len(pts.tolist()) == 6 and isinstance(pts.tolist(), list), "tolist()")

match pts:
    case [first, *rest]:
        check(first.x == 0.0 and len(rest) == 5, "a structural pattern matches")
    case _:
        check(False, "a structural pattern matches")

try:
    pts["nope"]
    check(False, "a non-index subscript is a TypeError")
except TypeError:
    check(True, "a non-index subscript is a TypeError")

try:
    pts[99]
    check(False, "past the end is an IndexError")
except IndexError:
    check(True, "past the end is an IndexError")

print("numpy reads it in place:")
arr = np.asarray(pts)
check(arr.dtype.names == ("x", "y", "z"), "the dtype names the members")
check(arr.dtype.itemsize == 24, "itemsize is sizeof")
check(not arr.flags.owndata, "no copy was made")
check(arr.base is pts, "and the array keeps the view alive")
check(list(arr["y"]) == [0.0, 2.0, 4.0, 6.0, 8.0, 10.0], "a column reads correctly")

print("a struct with padding:")
ticks = np.asarray(src.get_ticks())
check(ticks.dtype.itemsize == 16, "itemsize counts the padding")
check(list(ticks["price"])[:3] == [0.0, 0.5, 1.0], "the price column is right")
pads = [n for n in ticks.dtype.names if n.startswith(" pad")]
check(len(pads) == 1, "the padding is one named field")
check(not any(n.isidentifier() for n in pads),
      "whose name cannot collide with a member")

print("a member called f1, which is what numpy calls an unnamed pad:")
f = src.get_fnames()
arr_f = np.asarray(f)
check(arr_f.dtype.names[0] == "f1", "the member keeps its name")
check(list(arr_f["x"])[:3] == [0.0, 1.5, 3.0], "and its neighbour reads right")

print("an empty vector:")
src.fill(0)
empty = src.get_points()
check(len(empty) == 0, "len() is zero")
check(empty.tolist() == [], "tolist() is empty")
check(np.asarray(empty).shape == (0,), "np.asarray gives an empty array")
check(np.asarray(empty).dtype.itemsize == 24, "with the dtype still right")

print("what the flag does not touch:")
src.fill(4)
named = src.get_named()
check(isinstance(named, list), "a struct with a std::string still returns a list")
check(named[1].label == "n1", "and its elements work")
flat = src.get_flat()
check(type(flat).__name__ == "array", "a vector<double> is still an array.array")

print("a member is the live vector, not a columnar snapshot:")
src.fill(4)
member_view = src.points
# --columnar converts a method RESULT, which is a value the caller just
# computed and is about to drop, so a snapshot loses nothing. A member is the
# object's own storage, and handing back a snapshot is what made
# `obj.points[0].x = 5` discard the write in silence. Each feature owns the
# conversion where it is correct; this assertion used to read the other way
# and said so itself ("it is a snapshot, not a window onto the live member").
check(not type(member_view).__name__.startswith("ColumnView"),
      "reading the member does not go through the columnar conversion")
check(len(member_view) == 4, "the member view reports the C++ length")
check(member_view[1].x == 1.0, "and its elements read")
member_view[2].x = 77.0
check(src.points[2].x == 77.0, "a write through the member view reaches C++")
src.fill(9)
check(len(member_view) == 9,
      "and it is a window onto the live member, so it sees a resize")

print("indexing copies, as the list's elements were copies too:")
check(pts[0] is not pts[0], "each index gives a new object")

print()
if failures:
    print(f"FAIL: {len(failures)} check(s)")
    for f in failures:
        print(f"  - {f}")
    sys.exit(1)
print("PASS: the view stands in for the list")
