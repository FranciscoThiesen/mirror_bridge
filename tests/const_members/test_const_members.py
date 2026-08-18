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

print("\nAll const-member tests passed!")
