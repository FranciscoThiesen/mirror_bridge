#!/usr/bin/env python3
"""Test single-header Python binding"""

import sys
import os

# Add build directory to path (2 levels up from tests/single_header_test)
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))

import robot_py

print("=== Python Single-Header Test ===\n")

# Test 1: Create robot
print("Test 1: Creating robot...")
r = robot_py.Robot()
r.name = "RoboBot"
r.battery = 100.0
assert r.name == "RoboBot"
assert r.battery == 100.0
print(f"  ✓ Robot created: {r.name}, battery: {r.battery}%")

# Test 2: Move forward
print("\nTest 2: Moving forward...")
r.move_forward(50.0)
assert r.battery == 95.0
print(f"  ✓ Moved 50m, battery: {r.battery}%")

# Test 3: Get status
print("\nTest 3: Get status...")
status = r.get_status()
assert "RoboBot" in status
assert "95" in status
print(f"  ✓ Status: {status}")

# Test 4: Commands
print("\nTest 4: Add commands...")
r.commands = ["forward", "left", "forward"]
count = r.command_count()
assert count == 3
print(f"  ✓ Added {count} commands")

# Test 5: Reset
print("\nTest 5: Reset...")
r.reset()
assert r.battery == 100.0
assert r.command_count() == 0
print(f"  ✓ Reset: battery={r.battery}%, commands={r.command_count()}")

print("\n" + "="*40)
print("✓ All Python single-header tests passed!")
print("="*40)
