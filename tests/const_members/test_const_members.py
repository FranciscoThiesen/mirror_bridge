#!/usr/bin/env python3
"""const data members bind as read-only attributes: readable, assignment
raises AttributeError, and stubs mark them @property."""

import sys
import os

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))

import const_members

s = const_members.Sensor()

print("Test 1: const members are readable...")
assert s.id == 42
assert s.model == "MB-1000"
print(f"  ✓ id={s.id}, model={s.model!r}")

print("Test 2: assigning a const member raises AttributeError...")
try:
    s.id = 7
    raise AssertionError("expected AttributeError assigning const member")
except AttributeError:
    print("  ✓ AttributeError raised")

print("Test 3: non-const members stay writable...")
s.reading = 21.5
assert s.scaled(2.0) == 43.0
print("  ✓ reading writable, method works")

print("Test 4: stubs mark const members read-only...")
stubs = const_members.__mirror_bridge_stubs__()
assert "@property" in stubs and "def id(self) -> int" in stubs, stubs
assert "reading: float" in stubs, stubs
print("  ✓ @property in stubs for const member")

print("Test 5: a member whose type has no default constructor is readable...")
h = const_members.Holder()
assert h.anchor_value() == 7, h.anchor_value()
assert h.anchor.value == 7, h.anchor.value
print("  ✓ readable through the attribute and through a method")

print("Test 6: ...and not assignable, rather than a compile failure...")
try:
    h.anchor = const_members.Fixed(99)
    raise AssertionError("expected AttributeError assigning an unassignable member")
except AttributeError:
    print("  ✓ AttributeError raised")

print("Test 7: C++ can still reseat it, and the read follows...")
h.reseat(13)
assert h.anchor_value() == 13, h.anchor_value()
assert h.anchor.value == 13, h.anchor.value
print("  ✓ the getter reads the live member")

print("Test 8: its assignable neighbour is unaffected...")
h.adjustable = 4.25
assert h.adjustable == 4.25
print("  ✓ one unassignable member does not freeze the class")

print("\nAll const-member tests passed!")
