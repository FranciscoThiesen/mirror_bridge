#!/usr/bin/env python3
"""Test variadic parameter support (>2 parameters)"""

import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..', 'build'))

import math_ops

print("=== Variadic Parameters Test ===\n")

ops = math_ops.MathOps()

# Test 3 parameters
print("Test 1: 3 parameters")
result = ops.add3(10.0, 20.0, 30.0)
print(f"  add3(10, 20, 30) = {result}")
assert result == 60.0
assert ops.value == 60.0
print("  ✓ 3-parameter method works\n")

# Test 4 parameters
print("Test 2: 4 parameters")
result = ops.multiply4(2.0, 3.0, 4.0, 5.0)
print(f"  multiply4(2, 3, 4, 5) = {result}")
assert result == 120.0
print("  ✓ 4-parameter method works\n")

# Test 5 parameters
print("Test 3: 5 parameters")
result = ops.sum5(1.0, 2.0, 3.0, 4.0, 5.0)
print(f"  sum5(1, 2, 3, 4, 5) = {result}")
assert result == 15.0
print("  ✓ 5-parameter method works\n")

# Test mixed types with 3 parameters
print("Test 4: Mixed types (3 params)")
result = ops.format3("Count: ", 42, " items")
print(f"  format3('Count: ', 42, ' items') = '{result}'")
assert result == "Count: 42 items"
print("  ✓ Mixed-type 3-parameter method works\n")

# Test 6 parameters
print("Test 5: 6 parameters (weighted sum)")
result = ops.weighted_sum(10.0, 0.5, 20.0, 0.3, 30.0, 0.2)
print(f"  weighted_sum(10, 0.5, 20, 0.3, 30, 0.2) = {result}")
assert result == (10.0 * 0.5 + 20.0 * 0.3 + 30.0 * 0.2)
print("  ✓ 6-parameter method works\n")

print("="*40)
print("✓ All variadic parameter tests passed!")
print("="*40)
