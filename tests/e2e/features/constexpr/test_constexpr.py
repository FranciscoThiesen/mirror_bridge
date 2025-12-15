#!/usr/bin/env python3
"""Test static constexpr member binding."""

import sys
sys.path.insert(0, '.')

import constexpr

def test_numeric_constexpr():
    """Test double/float static constexpr members."""
    # Access via class (not instance)
    assert abs(constexpr.PhysicsConstants.PI - 3.14159265358979) < 1e-10
    assert abs(constexpr.PhysicsConstants.E - 2.71828182845904) < 1e-10
    assert constexpr.PhysicsConstants.SPEED_OF_LIGHT == 299792458.0
    assert abs(constexpr.PhysicsConstants.PLANCK_CONSTANT - 6.62607015e-34) < 1e-44

    print("✓ Numeric constexpr members work")


def test_integer_constexpr():
    """Test integer static constexpr members."""
    assert constexpr.PhysicsConstants.MAX_ITERATIONS == 1000
    assert constexpr.PhysicsConstants.BUFFER_SIZE == 4096
    assert constexpr.PhysicsConstants.LARGE_NUMBER == 1234567890123

    print("✓ Integer constexpr members work")


def test_bool_constexpr():
    """Test boolean static constexpr members."""
    assert constexpr.PhysicsConstants.DEBUG_MODE == False

    print("✓ Boolean constexpr members work")


def test_constexpr_via_instance():
    """Test that static constexpr can also be accessed via instance."""
    obj = constexpr.PhysicsConstants()

    # Should be accessible via instance too
    assert abs(obj.PI - 3.14159265358979) < 1e-10
    assert obj.MAX_ITERATIONS == 1000

    print("✓ Constexpr accessible via instance")


def test_methods_using_constexpr():
    """Test methods that use constexpr constants internally."""
    obj = constexpr.PhysicsConstants()

    # circle_area uses PI internally
    area = obj.circle_area(2.0)
    expected = 3.14159265358979 * 4.0
    assert abs(area - expected) < 1e-10, f"Expected {expected}, got {area}"

    # circle_circumference uses PI internally
    circ = obj.circle_circumference(1.0)
    expected = 2.0 * 3.14159265358979
    assert abs(circ - expected) < 1e-10

    print("✓ Methods using constexpr work")


def test_mixed_class():
    """Test class with both constexpr and non-static members."""
    # Static constexpr via class
    assert constexpr.Config.VERSION_MAJOR == 2
    assert constexpr.Config.VERSION_MINOR == 5
    assert constexpr.Config.VERSION_PATCH == 0

    # Non-static members via instance
    cfg = constexpr.Config()
    assert cfg.setting_a == 10
    assert cfg.setting_b == 20.0

    # Method that uses constexpr
    assert cfg.get_version_number() == 20500

    print("✓ Mixed class (constexpr + regular members) works")


def test_constexpr_immutability():
    """Test that static constexpr members cannot be modified."""
    # Attempting to set should raise TypeError (Python 3.10+) or AttributeError
    try:
        constexpr.PhysicsConstants.PI = 3.0
        assert False, "Should have raised TypeError or AttributeError"
    except (TypeError, AttributeError):
        pass  # Expected - Python prevents modification of class attrs on immutable types

    try:
        obj = constexpr.PhysicsConstants()
        obj.PI = 3.0
        assert False, "Should have raised TypeError or AttributeError"
    except (TypeError, AttributeError):
        pass  # Expected

    print("✓ Constexpr members are read-only")


if __name__ == "__main__":
    test_numeric_constexpr()
    test_integer_constexpr()
    test_bool_constexpr()
    test_constexpr_via_instance()
    test_methods_using_constexpr()
    test_mixed_class()
    test_constexpr_immutability()

    print("\n✓ All constexpr tests passed!")
