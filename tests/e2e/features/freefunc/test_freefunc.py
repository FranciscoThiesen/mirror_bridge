#!/usr/bin/env python3
"""Test free function binding."""

import sys
sys.path.insert(0, '.')

import freefunc_test

def test_basic_arithmetic():
    """Test simple arithmetic functions."""
    assert freefunc_test.add(2, 3) == 5
    assert freefunc_test.add(-1, 1) == 0
    assert abs(freefunc_test.multiply(2.5, 4.0) - 10.0) < 0.001

    print("✓ Basic arithmetic functions work")


def test_string_function():
    """Test function taking and returning string."""
    result = freefunc_test.greet("World")
    assert result == "Hello, World!"

    result = freefunc_test.greet("Python")
    assert result == "Hello, Python!"

    print("✓ String function works")


def test_no_arg_function():
    """Test function with no arguments."""
    pi = freefunc_test.get_pi()
    assert abs(pi - 3.14159265358979) < 1e-10

    print("✓ No-argument function works")


def test_void_function():
    """Test void functions (state modification)."""
    freefunc_test.reset_counter()
    assert freefunc_test.get_counter() == 0

    freefunc_test.increment_counter()
    assert freefunc_test.get_counter() == 1

    freefunc_test.increment_counter()
    freefunc_test.increment_counter()
    assert freefunc_test.get_counter() == 3

    freefunc_test.reset_counter()
    assert freefunc_test.get_counter() == 0

    print("✓ Void functions work")


def test_vector_function():
    """Test function taking vector."""
    result = freefunc_test.sum_vector([1.0, 2.0, 3.0, 4.0])
    assert abs(result - 10.0) < 0.001

    result = freefunc_test.sum_vector([])
    assert result == 0.0

    print("✓ Vector input function works")


def test_multi_arg_function():
    """Test function with multiple arguments."""
    # distance(0, 0, 3, 4) = 5 (3-4-5 triangle)
    d = freefunc_test.distance(0.0, 0.0, 3.0, 4.0)
    assert abs(d - 5.0) < 0.001

    print("✓ Multi-argument function works")


def test_returning_vector():
    """Test function returning vector."""
    result = freefunc_test.range(5)
    assert result == [0, 1, 2, 3, 4]

    result = freefunc_test.range(0)
    assert result == []

    print("✓ Vector returning function works")


def test_function_with_bound_class():
    """Test free functions that work with bound classes."""
    p1 = freefunc_test.Point()
    p1.x = 0.0
    p1.y = 0.0

    p2 = freefunc_test.Point()
    p2.x = 3.0
    p2.y = 4.0

    # point_distance takes two Point objects
    d = freefunc_test.point_distance(p1, p2)
    assert abs(d - 5.0) < 0.001

    print("✓ Function with bound class arguments works")


def test_function_returning_bound_class():
    """Test free function returning a bound class."""
    p = freefunc_test.make_point(3.0, 4.0)
    assert p.x == 3.0
    assert p.y == 4.0
    assert abs(p.length() - 5.0) < 0.001

    print("✓ Function returning bound class works")


def test_wrong_arguments():
    """Test error handling for wrong arguments."""
    try:
        freefunc_test.add()  # Missing arguments
        assert False, "Should have raised TypeError"
    except TypeError:
        pass

    try:
        freefunc_test.add(1, 2, 3)  # Too many arguments
        assert False, "Should have raised TypeError"
    except TypeError:
        pass

    print("✓ Wrong arguments raise TypeError")


if __name__ == "__main__":
    test_basic_arithmetic()
    test_string_function()
    test_no_arg_function()
    test_void_function()
    test_vector_function()
    test_multi_arg_function()
    test_returning_vector()
    test_function_with_bound_class()
    test_function_returning_bound_class()
    test_wrong_arguments()

    print("\n✓ All free function tests passed!")
