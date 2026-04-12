"""Tests for std::expected<T, E> type conversion in Python bindings."""

import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
import expected_test

def test_success_cases():
    """Test that successful expected values are returned directly."""
    svc = expected_test.MathService()

    # Basic division
    assert svc.safe_divide(10.0, 2.0) == 5.0
    assert svc.safe_divide(7.0, 2.0) == 3.5
    assert svc.safe_divide(0.0, 5.0) == 0.0

    # Square root
    assert svc.safe_sqrt(4.0) == 2.0
    assert svc.safe_sqrt(0.0) == 0.0
    assert abs(svc.safe_sqrt(2.0) - 1.4142135623730951) < 1e-10

    # Positive parsing
    assert svc.parse_positive(42) == 42
    assert svc.parse_positive(1) == 1

    # String results
    assert svc.greet("World") == "Hello, World!"

    # Chained operations
    assert svc.safe_divide_then_sqrt(16.0, 4.0) == 2.0
    assert svc.safe_divide_then_sqrt(9.0, 1.0) == 3.0

    print("  PASS: success cases")

def test_error_cases():
    """Test that error expected values raise ValueError."""
    svc = expected_test.MathService()

    # Division by zero
    try:
        svc.safe_divide(10.0, 0.0)
        assert False, "Should have raised ValueError"
    except ValueError as e:
        assert "division by zero" in str(e)

    # Negative square root
    try:
        svc.safe_sqrt(-1.0)
        assert False, "Should have raised ValueError"
    except ValueError as e:
        assert "negative" in str(e)

    # Negative parse
    try:
        svc.parse_positive(-5)
        assert False, "Should have raised ValueError"
    except ValueError as e:
        assert "error code" in str(e)

    # Zero parse
    try:
        svc.parse_positive(0)
        assert False, "Should have raised ValueError"
    except ValueError as e:
        assert "error code" in str(e)

    # Empty name
    try:
        svc.greet("")
        assert False, "Should have raised ValueError"
    except ValueError as e:
        assert "empty" in str(e)

    # Chained error (divide by zero propagated)
    try:
        svc.safe_divide_then_sqrt(10.0, 0.0)
        assert False, "Should have raised ValueError"
    except ValueError as e:
        assert "division by zero" in str(e)

    # Chained error (negative sqrt after valid division)
    try:
        svc.safe_divide_then_sqrt(-4.0, 1.0)
        assert False, "Should have raised ValueError"
    except ValueError as e:
        assert "negative" in str(e)

    print("  PASS: error cases")

def test_precision_member():
    """Test that the precision member is accessible and modifiable."""
    svc = expected_test.MathService()
    assert svc.precision == 1e-10

    # With very small divisor that's above precision threshold
    result = svc.safe_divide(1.0, 1e-5)
    assert abs(result - 1e5) < 1.0

    print("  PASS: precision member")

if __name__ == "__main__":
    print("Testing std::expected<T, E> Python bindings:")
    test_success_cases()
    test_error_cases()
    test_precision_member()
    print("\nAll std::expected tests passed!")
