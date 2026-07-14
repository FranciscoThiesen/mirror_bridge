#!/usr/bin/env python3
"""Test template class bindings"""

import sys
import os

# Add build directory to Python path (use absolute path for CI compatibility)
# Build output is at repo_root/build, which is 3 levels up from tests/e2e/templates
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..', 'build'))

import container

print("=== Template Class Binding Test ===")
print()

# Test 1: ContainerInt
print("Test 1: ContainerInt")
c_int = container.ContainerInt()
c_int.push(10)
c_int.push(20)
c_int.push(30)
assert c_int.size() == 3, f"Expected size 3, got {c_int.size()}"
assert c_int.sum() == 60, f"Expected sum 60, got {c_int.sum()}"
print(f"  size: {c_int.size()}, sum: {c_int.sum()}")
print("  PASS")
print()

# Test 2: ContainerDouble
print("Test 2: ContainerDouble")
c_double = container.ContainerDouble()
c_double.push(1.5)
c_double.push(2.5)
c_double.push(3.0)
assert c_double.size() == 3, f"Expected size 3, got {c_double.size()}"
assert abs(c_double.sum() - 7.0) < 0.001, f"Expected sum 7.0, got {c_double.sum()}"
print(f"  size: {c_double.size()}, sum: {c_double.sum()}")
print("  PASS")
print()

# Test 3: Independence of instantiations
print("Test 3: Instantiation independence")
c_int.clear()
assert c_int.size() == 0, "ContainerInt should be empty after clear"
assert c_double.size() == 3, "ContainerDouble should still have 3 elements"
print("  ContainerInt cleared, ContainerDouble unchanged")
print("  PASS")
print()

print("=" * 40)
print("ALL TEMPLATE TESTS PASSED!")
print("=" * 40)
