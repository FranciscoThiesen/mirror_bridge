#!/usr/bin/env python3
"""Lvalue-reference parameters of bound classes alias the Python-held object:
const T& passes without copying, T& mutations are visible from Python, and
by-value parameters still copy."""

import sys
import os

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))

import byref

t = byref.Tracked()
t.value = 41
m = byref.Mutator()

print("Test 1: const T& argument does not copy...")
byref.Tracked.reset_copies()
assert m.read(t) == 41
copies = byref.Tracked.copies()
assert copies == 0, f"const Tracked& argument made {copies} copies, expected 0"
print("  ✓ zero copies")

print("Test 2: T& mutation is visible on the Python object...")
m.bump(t)
assert t.value == 42, f"mutation through Tracked& was dropped (value={t.value})"
print("  ✓ mutation visible")

print("Test 3: by-value argument still copies...")
byref.Tracked.reset_copies()
assert m.read_value(t) == 42
copies = byref.Tracked.copies()
assert copies >= 1, "by-value Tracked argument should copy"
print(f"  ✓ copy semantics preserved ({copies} copy)")

print("Test 4: None is rejected for reference parameters...")
try:
    m.read(None)
    raise AssertionError("expected TypeError for None argument")
except TypeError:
    print("  ✓ TypeError raised")

print("\nAll by-reference argument tests passed!")
