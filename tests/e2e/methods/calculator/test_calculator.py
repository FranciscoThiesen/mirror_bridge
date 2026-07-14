#!/usr/bin/env python3
"""
Test method binding (member functions)
"""

import sys
import os

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..', 'build'))

try:
    import calculator
except ImportError as e:
    print(f"❌ Failed to import calculator: {e}")
    sys.exit(1)

def main():
    print("=== Mirror Bridge Method Binding Test ===\n")

    # Test 1: Create Calculator
    print("Test 1: Creating Calculator...")
    calc = calculator.Calculator()
    print(f"  ✓ Created Calculator")
    print(f"  Initial value: {calc.value}")
    assert calc.value == 0.0
    print()

    # Test 2: Test add method
    print("Test 2: Testing add method...")
    result = calc.add(5.0)
    print(f"  calc.add(5.0) = {result}")
    assert result == 5.0
    assert calc.value == 5.0
    print("  ✓ add() works correctly")
    print()

    # Test 3: Test subtract method
    print("Test 3: Testing subtract method...")
    result = calc.subtract(2.0)
    print(f"  calc.subtract(2.0) = {result}")
    assert result == 3.0
    assert calc.value == 3.0
    print("  ✓ subtract() works correctly")
    print()

    # Test 4: Test multiply method
    print("Test 4: Testing multiply method...")
    result = calc.multiply(4.0)
    print(f"  calc.multiply(4.0) = {result}")
    assert result == 12.0
    assert calc.value == 12.0
    print("  ✓ multiply() works correctly")
    print()

    # Test 5: Test divide method
    print("Test 5: Testing divide method...")
    result = calc.divide(3.0)
    print(f"  calc.divide(3.0) = {result}")
    assert result == 4.0
    assert calc.value == 4.0
    print("  ✓ divide() works correctly")
    print()

    # Test 6: Test const method (get_value)
    print("Test 6: Testing const method...")
    value = calc.get_value()
    print(f"  calc.get_value() = {value}")
    assert value == 4.0
    print("  ✓ const method works correctly")
    print()

    # Test 7: Test method with multiple parameters
    print("Test 7: Testing method with multiple parameters...")
    result = calc.compute(10.0, 5.0)
    print(f"  calc.compute(10.0, 5.0) = {result}")
    assert result == 30.0  # (10 + 5) * 2
    assert calc.value == 30.0
    print("  ✓ multi-parameter method works correctly")
    print()

    # Test 8: Test void method
    print("Test 8: Testing void method...")
    result = calc.reset()
    print(f"  calc.reset() returned: {result}")
    assert result is None
    assert calc.value == 0.0
    print("  ✓ void method works correctly")
    print()

    # Test 9: Test method returning string
    print("Test 9: Testing method returning string...")
    calc.value = 42.0
    s = calc.to_string()
    print(f"  calc.to_string() = '{s}'")
    assert "42" in s
    print("  ✓ string-returning method works correctly")
    print()

    # Test 10: Test exception handling
    print("Test 10: Testing exception handling...")
    try:
        calc.divide(0.0)
        print("  ❌ Should have thrown exception!")
        return 1
    except RuntimeError as e:
        print(f"  ✓ Caught exception: {e}")
    print()

    print("="*40)
    print("✓ All method binding tests passed!")
    print("="*40)
    return 0

if __name__ == "__main__":
    sys.exit(main())
