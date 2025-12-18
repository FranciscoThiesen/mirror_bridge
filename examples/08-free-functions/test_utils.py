#!/usr/bin/env python3
"""Test free functions example."""

import sys
import math
sys.path.insert(0, 'build')

import utilities

def test_arithmetic():
    """Test basic arithmetic functions."""
    assert utilities.add(2, 3) == 5
    assert utilities.add(-1, 1) == 0
    assert abs(utilities.multiply(2.5, 4.0) - 10.0) < 1e-10

    # Division with error handling
    assert abs(utilities.divide(10.0, 2.0) - 5.0) < 1e-10

    try:
        utilities.divide(1.0, 0.0)
        assert False, "Should have raised"
    except RuntimeError as e:
        assert "Division by zero" in str(e)

    print("✓ Arithmetic functions work")


def test_math_utilities():
    """Test math utility functions."""
    # Clamp
    assert utilities.clamp(5.0, 0.0, 10.0) == 5.0
    assert utilities.clamp(-5.0, 0.0, 10.0) == 0.0
    assert utilities.clamp(15.0, 0.0, 10.0) == 10.0

    # Lerp (linear interpolation)
    assert utilities.lerp(0.0, 10.0, 0.5) == 5.0
    assert utilities.lerp(0.0, 10.0, 0.0) == 0.0
    assert utilities.lerp(0.0, 10.0, 1.0) == 10.0

    # Angle conversion
    assert abs(utilities.degrees_to_radians(180.0) - math.pi) < 1e-10
    assert abs(utilities.radians_to_degrees(math.pi) - 180.0) < 1e-10

    print("✓ Math utilities work")


def test_string_utilities():
    """Test string utility functions."""
    assert utilities.to_upper("hello") == "HELLO"
    assert utilities.to_lower("WORLD") == "world"
    assert utilities.repeat("ab", 3) == "ababab"
    assert utilities.repeat("x", 0) == ""

    assert utilities.starts_with("hello world", "hello")
    assert not utilities.starts_with("hello", "world")

    assert utilities.ends_with("hello world", "world")
    assert not utilities.ends_with("world", "hello")

    assert utilities.count_words("hello world") == 2
    assert utilities.count_words("  spaces  around  ") == 2
    assert utilities.count_words("") == 0

    print("✓ String utilities work")


def test_vector_utilities():
    """Test vector/list utility functions."""
    values = [1.0, 2.0, 3.0, 4.0, 5.0]

    assert utilities.sum(values) == 15.0
    assert utilities.mean(values) == 3.0
    assert utilities.min_value(values) == 1.0
    assert utilities.max_value(values) == 5.0

    # Empty list handling
    assert utilities.sum([]) == 0.0
    assert utilities.mean([]) == 0.0

    print("✓ Vector utilities work")


def test_range_functions():
    """Test range generation functions."""
    # Integer range
    r = utilities.range(0, 5)
    assert r == [0, 1, 2, 3, 4]

    r = utilities.range(5, 10)
    assert r == [5, 6, 7, 8, 9]

    # Linspace (like numpy.linspace)
    ls = utilities.linspace(0.0, 1.0, 5)
    assert len(ls) == 5
    assert abs(ls[0] - 0.0) < 1e-10
    assert abs(ls[-1] - 1.0) < 1e-10
    assert abs(ls[2] - 0.5) < 1e-10

    print("✓ Range functions work")


def test_validation():
    """Test validation functions."""
    # Should not raise
    utilities.validate_positive(1.0)
    utilities.validate_positive(0.001)

    # Should raise
    try:
        utilities.validate_positive(0.0)
        assert False, "Should have raised"
    except RuntimeError:
        pass

    try:
        utilities.validate_positive(-1.0)
        assert False, "Should have raised"
    except RuntimeError:
        pass

    print("✓ Validation functions work")


if __name__ == "__main__":
    test_arithmetic()
    test_math_utilities()
    test_string_utilities()
    test_vector_utilities()
    test_range_functions()
    test_validation()

    print("\n✓ All free function tests passed!")
