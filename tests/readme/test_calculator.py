#!/usr/bin/env python3
"""Test Calculator example from README introduction"""

import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))

import cpp_calc

print("=== README Calculator Example ===\n")

# Exact code from README intro
calc = cpp_calc.Calculator()  # Default constructor ✓
calc.add(10)                  # Methods ✓
calc.subtract(3)              # Returns: 7.0
print(f"calc.value = {calc.value}")  # Direct member access ✓ (7.0)

# Verify results
assert calc.value == 7.0, f"Expected 7.0, got {calc.value}"

print("\n✓ README Calculator example works!")
