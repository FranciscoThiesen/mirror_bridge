"""Tests for bulk array transfer optimization.

Numeric vectors (float, double, int, etc.) should be returned as
array.array objects via single memcpy instead of element-by-element
Python list construction.
"""

import sys
import os
import array

sys.path.insert(0, os.path.dirname(__file__))
import bulk_test

def test_float_array():
    gen = bulk_test.DataGenerator()
    gen.size = 100
    result = gen.get_floats()

    assert isinstance(result, array.array), f"Expected array.array, got {type(result)}"
    assert result.typecode == 'f', f"Expected typecode 'f', got '{result.typecode}'"
    assert len(result) == 100
    assert abs(result[0] - 0.0) < 1e-6
    assert abs(result[1] - 0.5) < 1e-6
    assert abs(result[10] - 5.0) < 1e-6
    print("  PASS: float array bulk transfer")

def test_double_array():
    gen = bulk_test.DataGenerator()
    gen.size = 100
    result = gen.get_doubles()

    assert isinstance(result, array.array), f"Expected array.array, got {type(result)}"
    assert result.typecode == 'd', f"Expected typecode 'd', got '{result.typecode}'"
    assert len(result) == 100
    import math
    assert abs(result[0] - math.sin(0.0)) < 1e-10
    assert abs(result[50] - math.sin(0.5)) < 1e-10
    print("  PASS: double array bulk transfer")

def test_int_array():
    gen = bulk_test.DataGenerator()
    gen.size = 100
    result = gen.get_ints()

    assert isinstance(result, array.array), f"Expected array.array, got {type(result)}"
    assert result.typecode == 'i', f"Expected typecode 'i', got '{result.typecode}'"
    assert len(result) == 100
    assert result[0] == 0
    assert result[5] == 25
    assert result[10] == 100
    print("  PASS: int array bulk transfer")

def test_short_array():
    gen = bulk_test.DataGenerator()
    gen.size = 50
    result = gen.get_shorts()

    assert isinstance(result, array.array), f"Expected array.array, got {type(result)}"
    assert result.typecode == 'h', f"Expected typecode 'h', got '{result.typecode}'"
    assert len(result) == 50
    assert result[0] == 0
    assert result[10] == 10
    print("  PASS: short array bulk transfer")

def test_string_list():
    """String vectors should still return as regular Python lists."""
    gen = bulk_test.DataGenerator()
    result = gen.get_strings()

    assert isinstance(result, list), f"Expected list, got {type(result)}"
    assert len(result) == 5
    assert result[0] == "item_0"
    assert result[4] == "item_4"
    print("  PASS: string list (element-by-element)")

def test_large_array_correctness():
    """Verify bulk transfer works correctly for large arrays."""
    gen = bulk_test.DataGenerator()
    gen.size = 10000
    result = gen.get_floats()

    assert len(result) == 10000
    assert abs(result[9999] - 9999.0 * 0.5) < 1e-3
    print("  PASS: large array correctness")

if __name__ == "__main__":
    print("Testing bulk array transfer:")
    test_float_array()
    test_double_array()
    test_int_array()
    test_short_array()
    test_string_list()
    test_large_array_correctness()
    print("\nAll bulk transfer tests passed!")
