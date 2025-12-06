#!/usr/bin/env python3
"""
Integration test for C++17 code generation.

This test verifies the full codegen workflow:
1. Generate C++17 binding code using reflection compiler
2. Compile the generated code with a standard C++17 compiler (g++)
3. Import and test the module from Python
"""

import sys
import os

# Add build directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'build'))

print("=== C++17 Codegen Integration Test ===")
print()

# Import the generated module
try:
    import sample_codegen
    print("Successfully imported sample_codegen module")
except ImportError as e:
    print(f"Failed to import sample_codegen: {e}")
    print("Make sure to run test_codegen.sh first to generate and compile the bindings")
    sys.exit(1)

print()

# Test 1: Calculator with default constructor
print("Test 1: Calculator default constructor")
calc = sample_codegen.Calculator()
assert calc.value == 0.0, f"Expected 0.0, got {calc.value}"
print(f"  calc.value = {calc.value}")
print("  PASS")
print()

# Test 2: Calculator with parameterized constructor
print("Test 2: Calculator parameterized constructor")
calc2 = sample_codegen.Calculator(10.0)
assert calc2.value == 10.0, f"Expected 10.0, got {calc2.value}"
print(f"  calc2.value = {calc2.value}")
print("  PASS")
print()

# Test 3: Calculator methods
print("Test 3: Calculator methods")
calc = sample_codegen.Calculator()
result = calc.add(5.0)
assert result == 5.0, f"Expected 5.0, got {result}"
print(f"  add(5.0) = {result}")

result = calc.multiply(3.0)
assert result == 15.0, f"Expected 15.0, got {result}"
print(f"  multiply(3.0) = {result}")

result = calc.subtract(5.0)
assert result == 10.0, f"Expected 10.0, got {result}"
print(f"  subtract(5.0) = {result}")

result = calc.get_value()
assert result == 10.0, f"Expected 10.0, got {result}"
print(f"  get_value() = {result}")

calc.reset()
assert calc.value == 0.0, f"Expected 0.0, got {calc.value}"
print(f"  reset() -> value = {calc.value}")
print("  PASS")
print()

# Test 4: Calculator to_string
print("Test 4: Calculator to_string")
calc = sample_codegen.Calculator(42.0)
s = calc.to_string()
assert "42" in s, f"Expected '42' in string, got {s}"
print(f"  to_string() = {s}")
print("  PASS")
print()

# Test 5: Point2D default constructor
print("Test 5: Point2D default constructor")
p = sample_codegen.Point2D()
assert p.x == 0.0, f"Expected x=0.0, got {p.x}"
assert p.y == 0.0, f"Expected y=0.0, got {p.y}"
print(f"  Point2D() -> x={p.x}, y={p.y}")
print("  PASS")
print()

# Test 6: Point2D parameterized constructor
print("Test 6: Point2D parameterized constructor")
p = sample_codegen.Point2D(3.0, 4.0)
assert p.x == 3.0, f"Expected x=3.0, got {p.x}"
assert p.y == 4.0, f"Expected y=4.0, got {p.y}"
print(f"  Point2D(3.0, 4.0) -> x={p.x}, y={p.y}")
print("  PASS")
print()

# Test 7: Point2D distance_from_origin
print("Test 7: Point2D distance_from_origin")
p = sample_codegen.Point2D(3.0, 4.0)
dist = p.distance_from_origin()
assert abs(dist - 5.0) < 0.001, f"Expected 5.0, got {dist}"
print(f"  distance_from_origin() = {dist}")
print("  PASS")
print()

# Test 8: Point2D length_squared
print("Test 8: Point2D length_squared")
p = sample_codegen.Point2D(3.0, 4.0)
len_sq = p.length_squared()
assert abs(len_sq - 25.0) < 0.001, f"Expected 25.0, got {len_sq}"
print(f"  length_squared() = {len_sq}")
print("  PASS")
print()

# Test 9: Point2D scale
print("Test 9: Point2D scale")
p = sample_codegen.Point2D(2.0, 3.0)
p.scale(2.0)
assert abs(p.x - 4.0) < 0.001, f"Expected x=4.0, got {p.x}"
assert abs(p.y - 6.0) < 0.001, f"Expected y=6.0, got {p.y}"
print(f"  scale(2.0) -> x={p.x}, y={p.y}")
print("  PASS")
print()

# Test 10: Point2D dot_xy
print("Test 10: Point2D dot_xy")
p = sample_codegen.Point2D(1.0, 2.0)
dot = p.dot_xy(3.0, 4.0)
assert abs(dot - 11.0) < 0.001, f"Expected 11.0, got {dot}"
print(f"  (1,2) dot_xy (3,4) = {dot}")
print("  PASS")
print()

# Test 11: Member modification
print("Test 11: Member modification")
p = sample_codegen.Point2D()
p.x = 10.0
p.y = 20.0
assert p.x == 10.0, f"Expected x=10.0, got {p.x}"
assert p.y == 20.0, f"Expected y=20.0, got {p.y}"
print(f"  Set x=10.0, y=20.0 -> x={p.x}, y={p.y}")
print("  PASS")
print()

print("=" * 50)
print("ALL C++17 CODEGEN TESTS PASSED!")
print("=" * 50)
