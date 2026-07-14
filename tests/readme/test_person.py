#!/usr/bin/env python3
"""Test Person example from README Quick Start section"""

import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))

import people

print("=== README Person Example ===\n")

# Exact code from README
p = people.Person("Alice", 30)
print(f"{p.name} was born in {p.birth_year(2024)}")

# Verify results
assert p.name == "Alice"
assert p.age == 30
assert p.birth_year(2024) == 1994

print("\n✓ README Person example works!")
