#!/usr/bin/env python3
"""Test free function binding."""

import sys
sys.path.insert(0, '.')

import freefunc

def test_basic_arithmetic():
    """Test simple arithmetic functions."""
    assert freefunc.add(2, 3) == 5
    assert freefunc.add(-1, 1) == 0
    assert abs(freefunc.multiply(2.5, 4.0) - 10.0) < 0.001

    print("✓ Basic arithmetic functions work")


def test_string_function():
    """Test function taking and returning string."""
    result = freefunc.greet("World")
    assert result == "Hello, World!"

    result = freefunc.greet("Python")
    assert result == "Hello, Python!"

    print("✓ String function works")


def test_no_arg_function():
    """Test function with no arguments."""
    pi = freefunc.get_pi()
    assert abs(pi - 3.14159265358979) < 1e-10

    print("✓ No-argument function works")


def test_void_function():
    """Test void functions (state modification)."""
    freefunc.reset_counter()
    assert freefunc.get_counter() == 0

    freefunc.increment_counter()
    assert freefunc.get_counter() == 1

    freefunc.increment_counter()
    freefunc.increment_counter()
    assert freefunc.get_counter() == 3

    freefunc.reset_counter()
    assert freefunc.get_counter() == 0

    print("✓ Void functions work")


def test_vector_function():
    """Test function taking vector."""
    result = freefunc.sum_vector([1.0, 2.0, 3.0, 4.0])
    assert abs(result - 10.0) < 0.001

    result = freefunc.sum_vector([])
    assert result == 0.0

    print("✓ Vector input function works")


def test_multi_arg_function():
    """Test function with multiple arguments."""
    # distance(0, 0, 3, 4) = 5 (3-4-5 triangle)
    d = freefunc.distance(0.0, 0.0, 3.0, 4.0)
    assert abs(d - 5.0) < 0.001

    print("✓ Multi-argument function works")


def test_returning_vector():
    """Test function returning vector."""
    result = freefunc.range(5)
    assert list(result) == [0, 1, 2, 3, 4]

    result = freefunc.range(0)
    assert list(result) == []

    print("✓ Vector returning function works")


def test_function_with_bound_class():
    """Test free functions that work with bound classes."""
    p1 = freefunc.Point()
    p1.x = 0.0
    p1.y = 0.0

    p2 = freefunc.Point()
    p2.x = 3.0
    p2.y = 4.0

    # point_distance takes two Point objects
    d = freefunc.point_distance(p1, p2)
    assert abs(d - 5.0) < 0.001

    print("✓ Function with bound class arguments works")


def test_function_returning_bound_class():
    """Test free function returning a bound class."""
    p = freefunc.make_point(3.0, 4.0)
    assert p.x == 3.0
    assert p.y == 4.0
    assert abs(p.length() - 5.0) < 0.001

    print("✓ Function returning bound class works")


def test_wrong_arguments():
    """Test error handling for wrong arguments."""
    try:
        freefunc.add()  # Missing arguments
        assert False, "Should have raised TypeError"
    except TypeError:
        pass

    try:
        freefunc.add(1, 2, 3)  # Too many arguments
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
