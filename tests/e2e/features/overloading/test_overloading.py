#!/usr/bin/env python3
"""Test method overloading dispatch."""

import sys
sys.path.insert(0, '.')

import overload_test

def test_single_arg_overloads():
    """Test overloaded methods with single argument of different types."""
    calc = overload_test.Calculator()

    # add(int)
    calc.add(10)
    assert calc.get() == 10.0, f"Expected 10.0, got {calc.get()}"

    # add(double)
    calc.add(2.5)
    assert calc.get() == 12.5, f"Expected 12.5, got {calc.get()}"

    print("✓ Single argument overloads work")


def test_multi_arg_overloads():
    """Test overloaded methods with different argument counts."""
    calc = overload_test.Calculator()

    # add(int)
    calc.add(5)
    assert calc.get() == 5.0

    # add(int, int) - different arity
    calc.add(3, 7)
    assert calc.get() == 15.0, f"Expected 15.0, got {calc.get()}"

    print("✓ Multi-argument overloads work")


def test_string_overload():
    """Test overload with string parameter."""
    calc = overload_test.Calculator()

    # set(string) - parses string to double
    calc.set("42.5")
    assert calc.get() == 42.5, f"Expected 42.5, got {calc.get()}"

    # set(int)
    calc.set(100)
    assert calc.get() == 100.0

    # set(double)
    calc.set(3.14)
    assert abs(calc.get() - 3.14) < 0.001

    print("✓ String overload works")


def test_non_overloaded_methods():
    """Test that non-overloaded methods still work."""
    calc = overload_test.Calculator()
    calc.add(10)

    # get() - not overloaded
    assert calc.get() == 10.0

    # reset() - not overloaded
    calc.reset()
    assert calc.get() == 0.0

    print("✓ Non-overloaded methods work")


def test_wrong_arguments():
    """Test error handling for wrong argument types/counts."""
    calc = overload_test.Calculator()

    # Wrong number of arguments
    try:
        calc.add()  # No args when at least 1 required
        assert False, "Should have raised TypeError"
    except TypeError as e:
        assert "No matching overload" in str(e)

    # Too many arguments
    try:
        calc.add(1, 2, 3, 4)  # Too many args
        assert False, "Should have raised TypeError"
    except TypeError as e:
        assert "No matching overload" in str(e)

    print("✓ Wrong arguments raise TypeError")


def test_static_methods():
    """Test static factory methods (not overloaded but testing they still work)."""
    # from_coords(double, double)
    p1 = overload_test.Point.from_coords(3.0, 4.0)
    assert p1.x == 3.0
    assert p1.y == 4.0
    assert p1.distance() == 5.0

    # from_single(double)
    p2 = overload_test.Point.from_single(2.0)
    assert p2.x == 2.0
    assert p2.y == 2.0

    print("✓ Static methods work")


if __name__ == "__main__":
    test_single_arg_overloads()
    test_multi_arg_overloads()
    test_string_overload()
    test_non_overloaded_methods()
    test_wrong_arguments()
    test_static_methods()

    print("\n✓ All overloading tests passed!")
