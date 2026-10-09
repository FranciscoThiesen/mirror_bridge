#!/usr/bin/env python3
"""Writing through a nested member lands in the owner, not in a temporary.

A non-static data member is interior to the wrapped object by construction, so
the getter can hand back a view onto the owner's storage instead of a copy.
That is decided at compile time from the reflected member list: no return value
policy from the author, and no runtime instance lookup.
"""

import gc
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))

import member_views as mv

print("Test 1: a write through a member lands in the owner...")
o = mv.Owner()
o.plain.x = 7.0
assert o.plain.x == 7.0, o.plain.x
o.plain.tag = 42
assert o.plain.tag == 42, o.plain.tag
print("  ✓ o.plain.x = 7.0 persists")

print("Test 2: the view survives in a local and still writes through...")
v = o.plain
v.x = 11.0
assert o.plain.x == 11.0, o.plain.x
print("  ✓ writes through a bound name reach the owner")

print("Test 3: nested members chain...")
o.middle.leaf.x = 3.0
assert o.middle.leaf.x == 3.0, o.middle.leaf.x
o.middle.weight = 2.5
assert o.middle.weight == 2.5
print("  ✓ o.middle.leaf.x = 3.0 persists two levels down")

print("Test 4: a class bound in its own right views the same way...")
b = mv.Base()
b.origin.x = 1.25
assert b.origin.x == 1.25, b.origin.x
# Owner inherits `origin` from Base but does not expose it. That is a
# pre-existing gap -- base-class data members are not surfaced on a derived
# type on main either -- and is unrelated to how members are handed back.
assert not hasattr(o, "origin")
print("  ✓ Base().origin writes through; inherited members remain unexposed")

print("Test 5: a view keeps the owner's storage alive...")
# The member lives inside the owner's heap allocation, so dropping the last
# Python reference to the owner while a view is live must not free it.
tmp = mv.Owner()
tmp.plain.x = 3.5
view = tmp.plain
del tmp
gc.collect()
assert view.x == 3.5, view.x
view.x = 9.0
assert view.x == 9.0
del view
gc.collect()
print("  ✓ no use-after-free once the owner is dropped")

print("Test 6: a const member still copies, and stays read-only...")
# A view of a const member would hand Python a writable reference to something
# C++ declared it may not change.
frozen = o.frozen
frozen.x = 99.0
assert o.frozen.x == 0.0, o.frozen.x
print("  ✓ writing through a const member's copy does not reach the owner")

print("Test 7: method returns are unchanged, and still copy...")
# Interiority cannot be established for a method return, so these are left
# exactly as they were rather than guessed at.
o.leaf_ref().x = 5.0
assert o.plain.x == 11.0, o.plain.x
assert o.leaf_copy().x == 11.0
print("  ✓ leaf_ref() still returns a copy (documented limitation)")

print("Test 8: non-class members are untouched...")
assert o.label == "owner"
o.label = "renamed"
assert o.label == "renamed"
assert len(o.many) == 2
assert isinstance(o.many, list)
print("  ✓ strings and containers convert by value as before")

print("Test 9: a view is accepted where the class is expected...")
# The boundary type gate must recognise a view as a real Leaf.
assert o.read_leaf(o.plain) == 11.0
assert o.read_leaf(mv.Leaf()) == 0.0
try:
    o.read_leaf(o.label)
    raise AssertionError("a str should not convert to Leaf")
except TypeError:
    pass
print("  ✓ views pass the type gate, wrong types still raise TypeError")

print("Test 10: repeated reads do not leak...")
before = sys.getrefcount(o)
for _ in range(1000):
    o.plain.x
after = sys.getrefcount(o)
assert before == after, (before, after)
print("  ✓ owner refcount is stable over 1000 member reads")

print("Test 11: a four-level chain writes through...")
deep = mv.Level1()
deep.two.three.leaf.x = 9.0
assert deep.two.three.leaf.x == 9.0, deep.two.three.leaf.x
print("  ✓ deep.two.three.leaf.x = 9.0 persists")

print("Test 12: holding only the deepest view keeps the whole chain alive...")
# Each level is a view holding a reference to the level above, so the
# intermediate views are the only thing keeping the root's storage alive once
# the root itself is unreferenced.
leaf_view = mv.Level1().two.three.leaf
gc.collect()
leaf_view.x = 4.0
assert leaf_view.x == 4.0, leaf_view.x
del leaf_view
gc.collect()
print("  ✓ no use-after-free with every intermediate owner unreferenced")

print("Test 13: assigning a whole member is visible through an existing view...")
# py_setter assigns into the member in place, so the view's pointer stays
# good and sees the new value rather than going stale.
m = mv.Middle()
lv = m.leaf
m.leaf = mv.Leaf()
m.leaf.x = 7.0
assert lv.x == 7.0, lv.x
print("  ✓ the view tracks the member across a whole-member assignment")

print("Test 14: refcounts stay balanced over a deep chain...")
before = sys.getrefcount(deep)
for _ in range(2000):
    deep.two.three.leaf.x
assert sys.getrefcount(deep) == before, (before, sys.getrefcount(deep))
print("  ✓ 2000 four-level reads leak no references")

print("\nAll member view tests passed!")
