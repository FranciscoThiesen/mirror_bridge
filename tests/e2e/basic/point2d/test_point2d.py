#!/usr/bin/env python3
"""
Test for Point2D binding
"""

import sys
import os

# Add build directory to Python path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))

try:
    import point2d
except ImportError as e:
    print(f"❌ Failed to import point2d: {e}")
    sys.exit(1)

def main():
    print("=== Point2D Binding Test ===\n")

    # Test 1: Creation
    print("Test 1: Creating Point2D...")
    p = point2d.Point2D()
    assert p.x == 0.0 and p.y == 0.0
    print(f"  ✓ Created: ({p.x}, {p.y})")

    # Test 2: Setting values
    print("\nTest 2: Setting values...")
    p.x = 3.0
    p.y = 4.0
    assert p.x == 3.0 and p.y == 4.0
    print(f"  ✓ Set to: ({p.x}, {p.y})")

    # Test 3: Multiple instances
    print("\nTest 3: Multiple instances...")
    p1 = point2d.Point2D()
    p2 = point2d.Point2D()
    p1.x = 1.0
    p2.x = 2.0
    assert p1.x == 1.0 and p2.x == 2.0
    print(f"  ✓ p1.x={p1.x}, p2.x={p2.x}")

    print("\n" + "="*40)
    print("✓ All tests passed!")
    print("="*40)
    return 0

if __name__ == "__main__":
    sys.exit(main())
