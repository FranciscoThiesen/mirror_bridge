#!/usr/bin/env python3
"""Test static methods and constexpr members."""

import sys
import math
sys.path.insert(0, 'build')

import static_math

def test_constexpr_constants():
    """Test static constexpr members."""
    # Access via class (preferred)
    assert abs(static_math.Constants.PI - math.pi) < 1e-10
    assert abs(static_math.Constants.E - math.e) < 1e-10
    assert static_math.Constants.MAX_ITERATIONS == 1000
    assert static_math.Constants.DEBUG_MODE == False

    # Access via instance (also works)
    c = static_math.Constants()
    assert abs(c.PI - math.pi) < 1e-10

    print("✓ Constexpr constants accessible")


def test_instance_methods_with_constants():
    """Test instance methods that use static constants."""
    c = static_math.Constants()

    # Circle calculations using internal PI constant
    area = c.circle_area(2.0)
    expected_area = math.pi * 4.0
    assert abs(area - expected_area) < 1e-10

    circumference = c.circle_circumference(1.0)
    expected_circ = 2.0 * math.pi
    assert abs(circumference - expected_circ) < 1e-10

    print("✓ Instance methods using constants work")


def test_static_factory_methods():
    """Test static factory methods on Point2D."""
    # Origin factory
    origin = static_math.Point2D.origin()
    assert origin.x == 0.0
    assert origin.y == 0.0

    # Unit vectors
    unit_x = static_math.Point2D.unit_x()
    assert unit_x.x == 1.0
    assert unit_x.y == 0.0

    unit_y = static_math.Point2D.unit_y()
    assert unit_y.x == 0.0
    assert unit_y.y == 1.0

    # Polar coordinates factory
    p = static_math.Point2D.from_polar(1.0, math.pi / 4)
    expected = math.sqrt(2) / 2
    assert abs(p.x - expected) < 1e-10
    assert abs(p.y - expected) < 1e-10

    print("✓ Static factory methods work")


def test_static_utility_methods():
    """Test static utility methods."""
    p1 = static_math.Point2D()
    p1.x, p1.y = 0.0, 0.0

    p2 = static_math.Point2D()
    p2.x, p2.y = 3.0, 4.0

    # Static distance method
    dist = static_math.Point2D.distance(p1, p2)
    assert abs(dist - 5.0) < 1e-10

    # Static midpoint method
    mid = static_math.Point2D.midpoint(p1, p2)
    assert mid.x == 1.5
    assert mid.y == 2.0

    print("✓ Static utility methods work")


def test_version_static_methods():
    """Test Config class with version info."""
    # Static constexpr version numbers
    assert static_math.Config.VERSION_MAJOR == 2
    assert static_math.Config.VERSION_MINOR == 1
    assert static_math.Config.VERSION_PATCH == 0

    # Static methods
    version_str = static_math.Config.version_string()
    assert version_str == "2.1.0"

    version_num = static_math.Config.version_number()
    assert version_num == 20100

    print("✓ Version static methods work")


def test_instance_with_static():
    """Test instance that uses static info."""
    cfg = static_math.Config()
    cfg.name = "myapp"

    full = cfg.full_name()
    assert full == "myapp v2.1.0"

    print("✓ Instance methods using static info work")


def test_class_level_state():
    """Test static member variable shared across instances."""
    # Reset to known state
    static_math.Counter.reset_total()
    assert static_math.Counter.get_total() == 0

    # Create multiple counters
    c1 = static_math.Counter()
    c2 = static_math.Counter()

    c1.increment()
    c1.increment()
    c2.increment()

    # Each has own value
    assert c1.value == 2
    assert c2.value == 1

    # But total is shared
    assert static_math.Counter.get_total() == 3

    c1.decrement()
    assert static_math.Counter.get_total() == 2

    print("✓ Class-level state works across instances")


if __name__ == "__main__":
    test_constexpr_constants()
    test_instance_methods_with_constants()
    test_static_factory_methods()
    test_static_utility_methods()
    test_version_static_methods()
    test_instance_with_static()
    test_class_level_state()

    print("\n✓ All static/constexpr tests passed!")
