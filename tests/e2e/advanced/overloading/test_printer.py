#!/usr/bin/env python3
"""Test method overloading via type-based dispatch (pybind11-style)"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'build'))

import printer

print("=== Method Overloading Test ===\n")

p = printer.Printer()

# List available methods
print("Available methods:")
methods = [m for m in dir(p) if not m.startswith('_')]
for m in methods:
    print(f"  - {m}")
print()

# Test overloaded print methods - all use the same name 'print'
print("Test 1: Overloaded print methods (dispatch by type)")

assert hasattr(p, 'print'), "Should have print method"

# Test int overload
p.print(42)
assert p.last_output == "int: 42", f"Expected 'int: 42', got '{p.last_output}'"
print(f"  print(42) -> '{p.last_output}' ✓")

# Test double overload
p.print(3.14)
assert "3.14" in p.last_output, f"Expected '3.14' in output, got '{p.last_output}'"
print(f"  print(3.14) -> '{p.last_output}' ✓")

# Test string overload
p.print("hello")
assert p.last_output == "string: hello", f"Expected 'string: hello', got '{p.last_output}'"
print(f"  print('hello') -> '{p.last_output}' ✓")

print()

# Test overloaded format methods with return values
print("Test 2: Overloaded format methods with return values")

assert hasattr(p, 'format'), "Should have format method"

# Test int,int overload
result = p.format(10, 20)
print(f"  format(10, 20) = '{result}'")
assert result == "10,20", f"Expected '10,20', got '{result}'"
print("  ✓")

# Test double,double overload
result = p.format(1.5, 2.5)
print(f"  format(1.5, 2.5) = '{result}'")
assert "1.5" in result and "2.5" in result
print("  ✓")

# Test string,string overload
result = p.format("foo", "bar")
print(f"  format('foo', 'bar') = '{result}'")
assert result == "foo + bar", f"Expected 'foo + bar', got '{result}'"
print("  ✓")

print()

# Test non-overloaded method
print("Test 3: Non-overloaded method (should keep original name)")
assert hasattr(p, 'get_last')
result = p.get_last()
print(f"  get_last() = '{result}'")
print("  ✓ Non-overloaded method unchanged\n")

print("="*40)
print("✓ Method overloading via type-based dispatch works!")
print("  Overloaded methods auto-dispatch based on argument types")
print("="*40)
