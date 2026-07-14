#!/usr/bin/env python3
"""Test constructor parameter binding"""

import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..', 'build'))

import rectangle

print("=== Constructor Parameters Test ===\n")

# Test default constructor (0 parameters)
print("Test 1: Default constructor")
r1 = rectangle.Rectangle()
print(f"  Rectangle() -> width={r1.width}, height={r1.height}, name='{r1.name}'")
assert r1.width == 0.0
assert r1.height == 0.0
assert r1.name == "unnamed"
print("  ✓ Default constructor works\n")

# Test 2-parameter constructor
print("Test 2: 2-parameter constructor")
r2 = rectangle.Rectangle(5.0, 3.0)
print(f"  Rectangle(5.0, 3.0) -> width={r2.width}, height={r2.height}, name='{r2.name}'")
assert r2.width == 5.0
assert r2.height == 3.0
assert r2.name == "rectangle"
print(f"  area() = {r2.area()}")
assert r2.area() == 15.0
print("  ✓ 2-parameter constructor works\n")

# Test 3-parameter constructor
print("Test 3: 3-parameter constructor (mixed types)")
r3 = rectangle.Rectangle(10.0, 20.0, "my_rect")
print(f"  Rectangle(10.0, 20.0, 'my_rect') -> width={r3.width}, height={r3.height}, name='{r3.name}'")
assert r3.width == 10.0
assert r3.height == 20.0
assert r3.name == "my_rect"
print(f"  perimeter() = {r3.perimeter()}")
assert r3.perimeter() == 60.0
print("  ✓ 3-parameter constructor works\n")

# Test that methods work on constructed objects
print("Test 4: Methods on constructed objects")
r4 = rectangle.Rectangle(4.0, 6.0, "test")
area = r4.area()
perim = r4.perimeter()
print(f"  Rectangle(4, 6) -> area={area}, perimeter={perim}")
assert area == 24.0
assert perim == 20.0
print("  ✓ Methods work on constructed objects\n")

print("="*40)
print("✓ All constructor tests passed!")
print("="*40)
