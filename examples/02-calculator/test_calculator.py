#!/usr/bin/env python3
"""Test the Calculator class binding."""

import sys
sys.path.insert(0, 'build')

import calculator

def test_basic_operations():
    calc = calculator.Calculator()

    assert calc.value == 0.0, f"Expected 0.0, got {calc.value}"

    result = calc.add(10.0)
    assert result == 10.0, f"Expected 10.0, got {result}"
    assert calc.value == 10.0

    result = calc.subtract(3.0)
    assert result == 7.0, f"Expected 7.0, got {result}"

    result = calc.multiply(2.0)
    assert result == 14.0, f"Expected 14.0, got {result}"

    result = calc.divide(2.0)
    assert result == 7.0, f"Expected 7.0, got {result}"

    print("✓ Basic operations test passed")

def test_multiple_params():
    calc = calculator.Calculator()

    # a + b * c = 2 + 3 * 4 = 14
    result = calc.compute(2.0, 3.0, 4.0)
    assert result == 14.0, f"Expected 14.0, got {result}"

    print("✓ Multiple parameters test passed")

def test_const_method():
    calc = calculator.Calculator()
    calc.add(42.0)

    value = calc.get_value()
    assert value == 42.0, f"Expected 42.0, got {value}"

    print("✓ Const method test passed")

def test_to_string():
    calc = calculator.Calculator()
    calc.add(3.14)

    s = calc.to_string()
    assert "3.14" in s, f"Expected '3.14' in string, got '{s}'"

    print("✓ to_string test passed")

def test_exception():
    calc = calculator.Calculator()
    calc.add(10.0)

    try:
        calc.divide(0.0)
        assert False, "Expected exception"
    except RuntimeError as e:
        assert "zero" in str(e).lower(), f"Expected 'zero' in error message, got '{e}'"

    print("✓ Exception test passed")

def test_reset():
    calc = calculator.Calculator()
    calc.add(100.0)
    calc.reset()
    assert calc.value == 0.0, f"Expected 0.0 after reset, got {calc.value}"

    print("✓ Reset test passed")

if __name__ == "__main__":
    test_basic_operations()
    test_multiple_params()
    test_const_method()
    test_to_string()
    test_exception()
    test_reset()
    print("\n✓ All tests passed!")
