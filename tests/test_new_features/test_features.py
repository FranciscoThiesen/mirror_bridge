#!/usr/bin/env python3
"""Test new Mirror Bridge features: stubgen, options, buffer views."""

import sys
import os

# Add build directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def test_basic_binding():
    """Test that basic binding still works."""
    print("Testing basic binding...")
    import test_features

    # Test TestClass
    obj = test_features.TestClass(1, "test")
    assert obj.id == 1
    assert obj.name == "test"

    obj.set_id(42)
    assert obj.get_id() == 42

    result = obj.calculate(3.0, 4.0)
    assert result == 12.0  # 3*4 + 0 (default value)

    # Test Vec2
    v1 = test_features.Vec2(1.0, 2.0)
    v2 = test_features.Vec2(3.0, 4.0)

    dot = v1.dot(v2)
    assert dot == 11.0  # 1*3 + 2*4

    print("  ✓ Basic binding works")


def test_stub_generation():
    """Test that stub generation works."""
    print("Testing stub generation...")
    import test_features

    # Get stub content
    stubs = test_features.get_stub_content()

    # Verify it contains expected content
    assert 'class TestClass:' in stubs, "Missing TestClass"
    assert 'class Vec2:' in stubs, "Missing Vec2"
    assert 'def __init__' in stubs, "Missing __init__"
    assert 'def calculate' in stubs, "Missing calculate method"
    assert 'def dot' in stubs, "Missing dot method"
    assert 'id: int' in stubs, "Missing id property"
    assert 'x: float' in stubs, "Missing x property"

    # Check type hints
    assert 'float' in stubs, "Missing float type hint"
    assert 'int' in stubs, "Missing int type hint"
    assert 'str' in stubs, "Missing str type hint"

    # Write stubs to file
    stub_file = "/tmp/test_features.pyi"
    success = test_features.generate_stubs(stub_file)
    assert success, "Failed to write stubs to file"

    # Verify file was created and has content
    with open(stub_file, 'r') as f:
        content = f.read()
        assert len(content) > 100, "Stub file too small"
        assert 'TestClass' in content, "Stub file missing TestClass"

    print("  ✓ Stub generation works")
    print(f"  ✓ Stubs written to {stub_file}")

    # Print a snippet of the stubs
    print("\n  Generated stubs preview:")
    for line in stubs.split('\n')[:20]:
        print(f"    {line}")
    print("    ...")


def test_buffer_return():
    """Test that buffer return types work."""
    print("Testing buffer returns...")
    import test_features

    obj = test_features.TestClass()

    # Test bytes return
    data = obj.get_bytes()
    assert len(data) == 100, f"Expected 100 bytes, got {len(data)}"

    # Verify data content
    if isinstance(data, (bytes, bytearray)):
        assert data[0] == 0
        assert data[50] == 50
        assert data[99] == 99
    else:
        # Could be a list if not using optimized transfer
        assert data[0] == 0
        assert data[50] == 50

    # Test float return
    floats = obj.get_floats()
    assert len(floats) == 5, f"Expected 5 floats, got {len(floats)}"

    print("  ✓ Buffer return types work")


def test_constructors():
    """Test multiple constructors."""
    print("Testing constructors...")
    import test_features

    # Default constructor
    obj1 = test_features.TestClass()
    assert obj1.id == 0

    # Two-argument constructor
    obj2 = test_features.TestClass(10, "hello")
    assert obj2.id == 10
    assert obj2.name == "hello"

    # Three-argument constructor
    obj3 = test_features.TestClass(20, "world", 3.14)
    assert obj3.id == 20
    assert obj3.name == "world"
    assert abs(obj3.value - 3.14) < 0.001

    print("  ✓ Multiple constructors work")


def test_vec2_operations():
    """Test Vec2 operations."""
    print("Testing Vec2 operations...")
    import test_features

    v1 = test_features.Vec2(3.0, 4.0)
    assert abs(v1.length() - 5.0) < 0.001, "length() failed"

    v2 = test_features.Vec2(1.0, 0.0)
    v3 = v1.add(v2)
    assert abs(v3.x - 4.0) < 0.001, "add() x failed"
    assert abs(v3.y - 4.0) < 0.001, "add() y failed"

    print("  ✓ Vec2 operations work")


def main():
    print("=" * 60)
    print("Mirror Bridge New Features Test Suite")
    print("=" * 60)
    print()

    tests = [
        test_basic_binding,
        test_stub_generation,
        test_buffer_return,
        test_constructors,
        test_vec2_operations,
    ]

    passed = 0
    failed = 0

    for test in tests:
        try:
            test()
            passed += 1
        except Exception as e:
            print(f"  ✗ FAILED: {e}")
            import traceback
            traceback.print_exc()
            failed += 1
        print()

    print("=" * 60)
    print(f"Results: {passed} passed, {failed} failed")
    print("=" * 60)

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
