#!/usr/bin/env python3
"""
Cross-Module Type Sharing Test

This test verifies that types bound in one shared library (.so) are properly
recognized and returned as wrapper objects (not dicts) when accessed from
methods in another shared library.

Test scenario:
1. vertex.so binds the Vertex class
2. bbox.so binds the BoundingBox class (which has Vertex members)
3. When BoundingBox.get_top_left() returns a Vertex, it should be a proper
   Vertex wrapper object with methods, not a plain Python dict.

This works because of GlobalTypeRegistry in mirror_bridge_core.hpp, which
provides a truly global type registry that is shared across all .so files.
"""

import sys
import os

# Ensure build directory is in path
build_dir = os.path.join(os.path.dirname(__file__), '../../build')
if build_dir not in sys.path:
    sys.path.insert(0, build_dir)

def test_cross_module_type_sharing():
    """Test that types are properly shared across modules."""

    # IMPORTANT: Import vertex first to register Vertex in GlobalTypeRegistry
    import vertex
    import bbox

    # Create a BoundingBox using default constructor, then set members directly
    box = bbox.BoundingBox()
    box.top_left = vertex.Vertex(1.0, 2.0)
    box.bottom_right = vertex.Vertex(5.0, 6.0)

    # Get the top-left corner - this should return a Vertex wrapper, not a dict
    top_left = box.get_top_left()

    # Verify it's a Vertex, not a dict
    assert type(top_left).__name__ == 'Vertex', \
        f"Expected Vertex, got {type(top_left).__name__}. Cross-module type sharing failed!"

    # Verify the Vertex is an instance of the Vertex class from the vertex module
    assert isinstance(top_left, vertex.Vertex), \
        f"top_left should be instance of vertex.Vertex, got {type(top_left)}"

    # Verify data members are accessible
    assert top_left.x == 1.0, f"Expected x=1.0, got {top_left.x}"
    assert top_left.y == 2.0, f"Expected y=2.0, got {top_left.y}"

    # Verify methods work on the returned Vertex object
    # This is the key benefit of returning a wrapper instead of a dict!
    distance = top_left.distance_from_origin()
    expected_distance = (1.0**2 + 2.0**2)**0.5
    assert abs(distance - expected_distance) < 1e-9, \
        f"Expected distance {expected_distance}, got {distance}"

    print("  - get_top_left() returns Vertex wrapper: PASS")
    print(f"  - Vertex type check: {type(top_left).__name__}")
    print(f"  - Vertex.x = {top_left.x}, Vertex.y = {top_left.y}")
    print(f"  - Vertex.distance_from_origin() = {distance}")

def test_multiple_vertex_returns():
    """Test that multiple Vertex returns all work correctly."""
    import vertex
    import bbox

    box = bbox.BoundingBox()
    box.top_left = vertex.Vertex(0.0, 0.0)
    box.bottom_right = vertex.Vertex(10.0, 10.0)

    # Test all methods that return Vertex
    top_left = box.get_top_left()
    bottom_right = box.get_bottom_right()
    center = box.get_center()

    # All should be Vertex wrappers
    assert isinstance(top_left, vertex.Vertex), "get_top_left() should return Vertex"
    assert isinstance(bottom_right, vertex.Vertex), "get_bottom_right() should return Vertex"
    assert isinstance(center, vertex.Vertex), "get_center() should return Vertex"

    # Verify values
    assert center.x == 5.0 and center.y == 5.0, \
        f"Expected center (5.0, 5.0), got ({center.x}, {center.y})"

    # Test method chaining - use Vertex.add() which returns a Vertex
    shifted = top_left.add(vertex.Vertex(1.0, 1.0))
    assert isinstance(shifted, vertex.Vertex), "Vertex.add() should return Vertex"
    assert shifted.x == 1.0 and shifted.y == 1.0, \
        f"Expected (1.0, 1.0), got ({shifted.x}, {shifted.y})"

    print("  - Multiple Vertex returns: PASS")
    print(f"  - center = ({center.x}, {center.y})")
    print(f"  - Vertex.add() chaining works: ({shifted.x}, {shifted.y})")

def test_bbox_methods():
    """Test that BoundingBox methods work with Vertex members."""
    import vertex
    import bbox

    # Create BoundingBox and set vertices
    box = bbox.BoundingBox()
    box.top_left = vertex.Vertex(3.0, 4.0)
    box.bottom_right = vertex.Vertex(7.0, 9.0)

    # Verify the BoundingBox methods work correctly
    assert box.width() == 4.0, f"Expected width 4.0, got {box.width()}"
    assert box.height() == 5.0, f"Expected height 5.0, got {box.height()}"
    assert box.area() == 20.0, f"Expected area 20.0, got {box.area()}"

    # Get the top-left back and verify it's correct
    tl = box.get_top_left()
    assert tl.x == 3.0 and tl.y == 4.0, \
        f"Expected top_left (3.0, 4.0), got ({tl.x}, {tl.y})"

    print("  - BoundingBox methods work: PASS")
    print(f"  - width={box.width()}, height={box.height()}, area={box.area()}")

if __name__ == '__main__':
    print("Cross-Module Type Sharing Tests")
    print("=" * 40)

    print("\nTest 1: Cross-module type sharing")
    test_cross_module_type_sharing()

    print("\nTest 2: Multiple Vertex returns")
    test_multiple_vertex_returns()

    print("\nTest 3: BoundingBox methods with Vertex")
    test_bbox_methods()

    print("\n" + "=" * 40)
    print("ALL CROSS-MODULE TESTS PASSED!")
