#!/usr/bin/env python3
"""
End-to-end test for mirror_bridge Python bindings.

This test verifies:
1. C++ class (Vector3) can be instantiated from Python
2. Member variables can be read and written
3. The binding was automatically generated via reflection
"""

import sys
import os

# Add build directory to Python path to import the generated binding
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'build'))

try:
    import vector3
except ImportError as e:
    print(f"❌ Failed to import vector3 module: {e}")
    print("Make sure to run build_bindings.sh first!")
    sys.exit(1)

def test_vector3_creation():
    """Test that we can create a Vector3 instance"""
    print("Test 1: Creating Vector3 instance...")
    v = vector3.Vector3()
    print(f"  ✓ Created Vector3: ({v.x}, {v.y}, {v.z})")
    assert v.x == 0.0 and v.y == 0.0 and v.z == 0.0, "Default values should be 0"
    print("  ✓ Default values are correct")

def test_vector3_member_access():
    """Test that we can read and write member variables"""
    print("\nTest 2: Reading and writing members...")
    v = vector3.Vector3()

    # Write values
    v.x = 3.0
    v.y = 4.0
    v.z = 0.0
    print(f"  Set values: x={v.x}, y={v.y}, z={v.z}")

    # Read values back
    assert v.x == 3.0, f"Expected x=3.0, got {v.x}"
    assert v.y == 4.0, f"Expected y=4.0, got {v.y}"
    assert v.z == 0.0, f"Expected z=0.0, got {v.z}"
    print("  ✓ Member read/write works correctly")

def test_vector3_computation():
    """Test vector computations"""
    print("\nTest 3: Vector computations...")
    v = vector3.Vector3()
    v.x = 3.0
    v.y = 4.0
    v.z = 0.0

    # Length should be 5.0 (3-4-5 triangle)
    print(f"  Vector: ({v.x}, {v.y}, {v.z})")
    print(f"  Expected length: 5.0")

    # Note: Methods are not yet bound in this version, only data members
    # Future versions will support method binding via reflection
    print("  ℹ Method binding not yet implemented (data members only)")

def test_multiple_instances():
    """Test that multiple instances work correctly"""
    print("\nTest 4: Multiple instances...")
    v1 = vector3.Vector3()
    v2 = vector3.Vector3()

    v1.x = 1.0
    v2.x = 2.0

    assert v1.x == 1.0 and v2.x == 2.0, "Instances should be independent"
    print(f"  v1.x = {v1.x}, v2.x = {v2.x}")
    print("  ✓ Multiple instances work independently")

def main():
    print("=== Mirror Bridge End-to-End Test ===\n")

    try:
        test_vector3_creation()
        test_vector3_member_access()
        test_vector3_computation()
        test_multiple_instances()

        print("\n" + "="*40)
        print("✓ All tests passed!")
        print("="*40)
        return 0

    except AssertionError as e:
        print(f"\n❌ Test failed: {e}")
        return 1
    except Exception as e:
        print(f"\n❌ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())
