#!/usr/bin/env python3
"""Test auto-discovered free functions."""

import sys
sys.path.append('build')

import math_funcs

def test_classes():
    """Test that classes are bound."""
    p = math_funcs.Point()
    p.x = 3.0
    p.y = 4.0
    assert abs(p.length() - 5.0) < 0.001
    print("✓ Point class works")

    r = math_funcs.Rectangle()
    r.width = 3.0
    r.height = 4.0
    assert r.area() == 12.0
    print("✓ Rectangle class works")


def test_free_functions():
    """Test that free functions are bound."""
    # Test make_point
    p = math_funcs.make_point(3.0, 4.0)
    assert p.x == 3.0
    assert p.y == 4.0
    print("✓ make_point() works")

    # Test distance
    p1 = math_funcs.make_point(0.0, 0.0)
    p2 = math_funcs.make_point(3.0, 4.0)
    d = math_funcs.distance(p1, p2)
    assert abs(d - 5.0) < 0.001
    print("✓ distance() works")

    # Test add_points
    p3 = math_funcs.add_points(p1, p2)
    assert p3.x == 3.0
    assert p3.y == 4.0
    print("✓ add_points() works")

    # Test scale_point
    p4 = math_funcs.scale_point(p2, 2.0)
    assert p4.x == 6.0
    assert p4.y == 8.0
    print("✓ scale_point() works")

    # Test simple math functions
    assert math_funcs.add(2.0, 3.0) == 5.0
    print("✓ add() works")

    assert math_funcs.multiply(2.0, 3.0) == 6.0
    print("✓ multiply() works")

    assert math_funcs.clamp(5.0, 0.0, 10.0) == 5.0
    assert math_funcs.clamp(-5.0, 0.0, 10.0) == 0.0
    assert math_funcs.clamp(15.0, 0.0, 10.0) == 10.0
    print("✓ clamp() works")

    # Test perimeter
    r = math_funcs.Rectangle()
    r.width = 3.0
    r.height = 4.0
    assert math_funcs.perimeter(r) == 14.0
    print("✓ perimeter() works")


def test_detail_not_exposed():
    """Test that detail:: namespace functions are not exposed."""
    assert not hasattr(math_funcs, 'internal_helper'), \
        "detail::internal_helper should not be exposed"
    print("✓ detail:: functions correctly skipped")


if __name__ == "__main__":
    test_classes()
    test_free_functions()
    test_detail_not_exposed()
    print("\n✓ All auto-discovery function tests passed!")
