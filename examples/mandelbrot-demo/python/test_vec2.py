#!/usr/bin/env python3
"""Test Vec2 Python binding - Multi-language demo"""

import sys
sys.path.insert(0, 'build')

import vec2

def main():
    print("╔══════════════════════════════════════════╗")
    print("║        Vec2 · Python Binding Test        ║")
    print("╚══════════════════════════════════════════╝")
    print()

    # Create vectors
    a = vec2.Vec2(3.0, 4.0)
    b = vec2.Vec2(1.0, 0.0)

    print(f"  a = Vec2({a.x}, {a.y})")
    print(f"  b = Vec2({b.x}, {b.y})")
    print()

    # Test methods
    print("  ┌─────────────────────────────────────┐")
    print(f"  │ a.length()      = {a.length():<18} │")
    print(f"  │ a.dot(b)        = {a.dot(b):<18} │")

    n = a.normalized()
    print(f"  │ a.normalized()  = ({n.x:.2f}, {n.y:.2f})          │")

    scaled = a.scale(2.0)
    print(f"  │ a.scale(2.0)    = ({scaled.x:.1f}, {scaled.y:.1f})           │")

    added = a.add(b)
    print(f"  │ a.add(b)        = ({added.x:.1f}, {added.y:.1f})            │")

    dist = a.distance_to(b)
    print(f"  │ a.distance_to(b)= {dist:<18.4f} │")
    print("  └─────────────────────────────────────┘")
    print()

    # Verify correctness
    assert abs(a.length() - 5.0) < 0.0001, "length() failed"
    assert abs(a.dot(b) - 3.0) < 0.0001, "dot() failed"
    assert abs(n.x - 0.6) < 0.0001, "normalized() x failed"
    assert abs(n.y - 0.8) < 0.0001, "normalized() y failed"

    print("  ✓ All assertions passed!")
    print()
    return 0

if __name__ == "__main__":
    sys.exit(main())
