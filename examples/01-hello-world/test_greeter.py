#!/usr/bin/env python3
"""Test the Greeter class binding."""

import sys
sys.path.insert(0, 'build')

import greeter

def test_basic():
    g = greeter.Greeter()

    # Check default values
    assert g.name == "World", f"Expected 'World', got '{g.name}'"
    assert g.greeting_count == 0, f"Expected 0, got {g.greeting_count}"

    # Call method
    message = g.greet()
    assert message == "Hello, World!", f"Expected 'Hello, World!', got '{message}'"
    assert g.greeting_count == 1, f"Expected 1, got {g.greeting_count}"

    print("✓ Basic test passed")

def test_custom_name():
    g = greeter.Greeter()
    g.name = "Mirror Bridge"

    message = g.greet()
    assert message == "Hello, Mirror Bridge!", f"Expected 'Hello, Mirror Bridge!', got '{message}'"

    print("✓ Custom name test passed")

def test_reset():
    g = greeter.Greeter()
    g.greet()
    g.greet()
    g.greet()

    assert g.greeting_count == 3, f"Expected 3, got {g.greeting_count}"

    g.reset()
    assert g.greeting_count == 0, f"Expected 0 after reset, got {g.greeting_count}"

    print("✓ Reset test passed")

if __name__ == "__main__":
    test_basic()
    test_custom_name()
    test_reset()
    print("\n✓ All tests passed!")
