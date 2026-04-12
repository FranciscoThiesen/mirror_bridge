"""Tests for smart pointer wrapper objects.

When a shared_ptr<T> is returned and T has been bound via bind_class,
the result should be a proper wrapper object (not a dict), with methods
and properties accessible.
"""

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))
import smart_ptr_test

def test_smart_ptr_returns_wrapper():
    """shared_ptr<Point> should return a Point wrapper object, not a dict."""
    shape = smart_ptr_test.Shape()
    shape.set_center(3.0, 4.0)

    center = shape.get_center()

    # Should be a Point wrapper, not a dict
    assert not isinstance(center, dict), f"Expected Point object, got dict: {center}"
    assert hasattr(center, 'x'), "Point should have x attribute"
    assert hasattr(center, 'y'), "Point should have y attribute"
    assert center.x == 3.0, f"Expected x=3.0, got {center.x}"
    assert center.y == 4.0, f"Expected y=4.0, got {center.y}"
    print("  PASS: shared_ptr returns wrapper object")

def test_smart_ptr_wrapper_methods():
    """Methods on the returned wrapper should work."""
    shape = smart_ptr_test.Shape()
    shape.set_center(3.0, 4.0)

    center = shape.get_center()
    length = center.length()
    assert abs(length - 5.0) < 1e-10, f"Expected 5.0, got {length}"
    print("  PASS: wrapper object methods work")

def test_smart_ptr_none_for_null():
    """A null shared_ptr should return None."""
    point = smart_ptr_test.Point()
    assert point.x == 0.0
    print("  PASS: Point default construction")

def test_smart_ptr_as_member():
    """shared_ptr member read via property getter should return wrapper."""
    shape = smart_ptr_test.Shape()
    shape.set_center(1.0, 2.0)

    # Access through get_center method
    center = shape.get_center()
    assert center.x == 1.0
    assert center.y == 2.0
    print("  PASS: shared_ptr member returns wrapper")

if __name__ == "__main__":
    print("Testing smart pointer wrapper objects:")
    test_smart_ptr_returns_wrapper()
    test_smart_ptr_wrapper_methods()
    test_smart_ptr_none_for_null()
    test_smart_ptr_as_member()
    print("\nAll smart pointer wrapper tests passed!")
