#!/usr/bin/env python3
"""
Test for custom type converter extension point.
Verifies that user-defined converters work correctly for types
not natively supported by mirror_bridge.
"""

import sys
import os

# Add build directory to Python path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "build"))

try:
    import pixel
except ImportError as e:
    print(f"Failed to import pixel: {e}")
    sys.exit(1)


def main():
    print("=== Custom Type Converter Test ===\n")

    # Test 1: Create Pixel with default Color3
    print("Test 1: Default construction...")
    p = pixel.Pixel()
    assert p.x == 0 and p.y == 0
    color = p.get_color()
    assert color == [0, 0, 0], f"Expected [0, 0, 0], got {color}"
    print(f"  Default color: {color}")

    # Test 2: Get Color3 as Python list
    print("\nTest 2: Color3 returned as list...")
    p.x = 10
    p.y = 20
    color = p.get_color()
    assert isinstance(color, list), f"Expected list, got {type(color)}"
    assert len(color) == 3, f"Expected 3 elements, got {len(color)}"
    print(f"  Color type: {type(color).__name__}, value: {color}")

    # Test 3: Set Color3 from Python list
    print("\nTest 3: Set color from list...")
    p.set_color([255, 128, 64])
    color = p.get_color()
    assert color == [255, 128, 64], f"Expected [255, 128, 64], got {color}"
    print(f"  Set from list: {color}")

    # Test 4: Set Color3 from Python tuple
    print("\nTest 4: Set color from tuple...")
    p.set_color((100, 150, 200))
    color = p.get_color()
    assert color == [100, 150, 200], f"Expected [100, 150, 200], got {color}"
    print(f"  Set from tuple: {color}")

    # Test 5: Set Color3 from dict with r, g, b keys
    print("\nTest 5: Set color from dict...")
    p.set_color({"r": 50, "g": 100, "b": 150})
    color = p.get_color()
    assert color == [50, 100, 150], f"Expected [50, 100, 150], got {color}"
    print(f"  Set from dict: {color}")

    # Test 6: Method returning Color3 (inverted_color)
    print("\nTest 6: Method returning custom type...")
    p.set_color([100, 50, 25])
    inverted = p.inverted_color()
    assert inverted == [155, 205, 230], f"Expected [155, 205, 230], got {inverted}"
    print(f"  Original: [100, 50, 25], Inverted: {inverted}")

    # Test 7: Color3 as property (direct field access)
    print("\nTest 7: Color3 as direct property...")
    p.color = [10, 20, 30]
    assert p.color == [10, 20, 30], f"Expected [10, 20, 30], got {p.color}"
    print(f"  Direct property access: {p.color}")

    # Test 8: Round-trip consistency
    print("\nTest 8: Round-trip consistency...")
    test_colors = [
        [0, 0, 0],
        [255, 255, 255],
        [128, 64, 32],
        [1, 2, 3],
    ]
    for expected in test_colors:
        p.set_color(expected)
        actual = p.get_color()
        assert actual == expected, f"Round-trip failed: {expected} -> {actual}"
    print(f"  All {len(test_colors)} round-trip tests passed")

    print("\n" + "=" * 40)
    print("All custom type converter tests passed!")
    print("=" * 40)
    return 0


if __name__ == "__main__":
    sys.exit(main())
