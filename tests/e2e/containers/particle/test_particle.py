#!/usr/bin/env python3
"""
Test container support in mirror_bridge.
Tests std::vector and std::array bindings.
"""

import sys
import os

# Add build directory to path
sys.path.append(os.path.join(os.path.dirname(__file__), 'build'))

try:
    import particle
except ImportError as e:
    print(f"❌ Failed to import particle: {e}")
    print("Make sure to build the bindings first with ../build_bindings.sh")
    sys.exit(1)

def test_particle_containers():
    """Test Particle class with vector and array containers"""
    print("=== Mirror Bridge Container Test ===\n")

    # Test 1: Create Particle and check default values
    print("Test 1: Creating Particle with default values...")
    p = particle.Particle()
    print(f"  ✓ Created Particle with mass: {p.mass}")
    print(f"  ✓ Default position (vector): {p.position}")
    print(f"  ✓ Default velocity (vector): {p.velocity}")
    print(f"  ✓ Default acceleration (array): {p.acceleration}")
    assert p.mass == 1.0, "Default mass should be 1.0"
    assert list(p.position) == [0.0, 0.0, 0.0], "Default position should be [0, 0, 0]"
    assert list(p.velocity) == [0.0, 0.0, 0.0], "Default velocity should be [0, 0, 0]"
    assert list(p.acceleration) == [0.0, 0.0, 0.0], "Default acceleration should be [0, 0, 0]"
    print()

    # Test 2: Modify vector containers
    print("Test 2: Modifying vector containers...")
    p.position = [1.0, 2.0, 3.0]
    p.velocity = [0.5, 0.0, -0.5]
    print(f"  Set position: {p.position}")
    print(f"  Set velocity: {p.velocity}")
    assert list(p.position) == [1.0, 2.0, 3.0], "Position should be updated"
    assert list(p.velocity) == [0.5, 0.0, -0.5], "Velocity should be updated"
    print(f"  ✓ Vector containers work correctly")
    print()

    # Test 3: Modify array containers
    print("Test 3: Modifying array containers...")
    p.acceleration = [0.0, -9.8, 0.0]
    print(f"  Set acceleration: {list(p.acceleration)}")
    assert list(p.acceleration) == [0.0, -9.8, 0.0], "Acceleration should be updated"
    print(f"  ✓ Array containers work correctly")
    print()

    # Test 4: Different sized vectors
    print("Test 4: Vectors with different sizes...")
    p.position = [1.0, 2.0, 3.0, 4.0, 5.0]  # 5 elements
    print(f"  Set position to 5 elements: {p.position}")
    assert len(p.position) == 5, "Should handle different vector sizes"
    p.velocity = [1.0]  # 1 element
    print(f"  Set velocity to 1 element: {p.velocity}")
    assert len(p.velocity) == 1, "Should handle single-element vectors"
    print(f"  ✓ Variable-sized vectors work correctly")
    print()

    # Test 5: Multiple instances
    print("Test 5: Multiple independent instances...")
    p1 = particle.Particle()
    p2 = particle.Particle()
    p1.position = [1.0, 0.0, 0.0]
    p2.position = [0.0, 1.0, 0.0]
    print(f"  p1.position = {p1.position}, p2.position = {p2.position}")
    assert p1.position != p2.position, "Instances should be independent"
    print(f"  ✓ Multiple instances work independently")
    print()

    print("========================================")
    print("✓ All container tests passed!")
    print("========================================")

if __name__ == "__main__":
    test_particle_containers()
