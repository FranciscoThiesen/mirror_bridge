#!/usr/bin/env python3
"""Test smart pointer support (unique_ptr, shared_ptr)"""

import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', 'build'))

import resource

print("=== Smart Pointer Support Test ===\n")

rm = resource.ResourceManager()

# Test 1: Smart pointer data members (null handling)
print("Test 1: Null smart pointer data members")
print(f"  Initial unique_data: {rm.unique_data}")
print(f"  Initial shared_data: {rm.shared_data}")
# Smart pointers should convert to None when null
assert rm.unique_data is None
assert rm.shared_data is None
print("  ✓ Null smart pointers convert to None\n")

# Test 2: Methods returning smart pointers
print("Test 2: Methods returning smart pointers")

# These return smart pointers, which return Data objects
unique_result = rm.create_unique("test1", 42)
print(f"  create_unique('test1', 42) = {unique_result}")
# Smart pointers to bound classes return instances of that class
assert unique_result.name == "test1"
assert unique_result.value == 42
print("  ✓ unique_ptr return works\n")

shared_result = rm.create_shared("test2", 99)
print(f"  create_shared('test2', 99) = {shared_result}")
assert shared_result.name == "test2"
assert shared_result.value == 99
print("  ✓ shared_ptr return works\n")

# Test 3: Methods that access smart pointer contents
print("Test 3: Smart pointer accessor methods")
# After creation, check accessors still work
assert rm.get_unique_name() == "null"  # unique_data member not set by create_unique
assert rm.get_shared_name() == "null"  # shared_data member not set by create_shared
print("  ✓ Accessor methods work correctly\n")

# Test 4: Data class standalone
print("Test 4: Data class")
data = resource.Data()
data.name = "test_data"
data.value = 42
assert data.name == "test_data"
assert data.value == 42
print(f"  Data(name='{data.name}', value={data.value})")
print("  ✓ Data class works correctly\n")

# Test 5: ResourceManager counter field (non-smart-pointer)
print("Test 5: ResourceManager primitive field")
rm.counter = 3.14
assert rm.counter == 3.14
print(f"  counter = {rm.counter}")
print("  ✓ Primitive field works\n")

print("="*40)
print("✓ All smart pointer tests passed!")
print("  - unique_ptr and shared_ptr return values work")
print("  - Null smart pointers convert to None")
print("  - Accessor methods work correctly")
print("="*40)
