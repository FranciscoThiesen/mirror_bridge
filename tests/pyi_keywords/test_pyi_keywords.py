#!/usr/bin/env python3
"""A C++ name that is a Python keyword must not break the .pyi.

The runtime is unaffected either way -- CPython looks members up by string, so
`getattr(obj, "pass")` works regardless. What breaks is the *stub*: a type
checker rejects the whole file over one bad line, and `mirror_bridge api`
parses it with ast, so an unparseable stub makes a module impossible to gate.
"""

import ast
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))

import pyi_keywords

stubs = pyi_keywords.__mirror_bridge_stubs__()

print("Test 1: the stub parses...")
ast.parse(stubs)
print("  ✓ ast.parse accepts it")

print("Test 2: a keyword parameter is renamed and marked positional-only...")
# `lambda_` is not a keyword the module answers to, so claiming it could be
# passed by name would be a lie; PEP 570's `/` says the true thing.
assert "def scale(self, lambda_: float, /, vega: float) -> float" in stubs, stubs
print("  ✓ scale(self, lambda_: float, /, vega: float)")

print("Test 3: a parameter named `self` no longer collides with the receiver...")
assert "def shadow(self, self_: float, /, other: float) -> float" in stubs, stubs
print("  ✓ shadow(self, self_: float, /, other: float)")

print("Test 4: `/` stops at the last unnameable parameter...")
# `vega` and `other` stay keyword-capable: `/` applies only to what precedes it.
assert "def plain(self, first: float, second: float) -> float" in stubs, stubs
assert "/" not in [p.strip() for p in stubs.split("def plain(")[1].split(")")[0].split(",")], stubs
print("  ✓ a keyword-free signature is untouched")

print("Test 5: a static method gets the same treatment...")
assert "def fold(lambda_: float, /) -> float" in stubs, stubs
print("  ✓ fold(lambda_: float, /)")

print("Test 6: members whose own name is a keyword are omitted, with a reason...")
# There is no stub syntax for `def pass(...)` or an attribute called `lambda`.
assert "def pass" not in stubs, stubs
assert "\n    lambda:" not in stubs, stubs
assert 'method "pass" omitted' in stubs, stubs
assert 'attribute "lambda" omitted' in stubs, stubs
print("  ✓ both omitted with an explanatory note")

print("Test 7: the keyword-free surface is still fully described...")
assert "class Greeks:" in stubs, stubs
assert "delta: float" in stubs, stubs
print("  ✓ class and ordinary members present")

print("Test 8: everything is still reachable at runtime...")
g = pyi_keywords.Greeks()
assert g.scale(3.0, 4.0) == 12.0
assert g.plain(5.0, 2.0) == 3.0
assert getattr(g, "pass")(1.0, 2.0) == 3.0
assert getattr(g, "lambda") == 0.0
assert g.shadow(1.0, 2.0) == 3.0
# The runtime keyword is still the real C++ name, which is why the stub marks
# these positional-only rather than advertising the substituted name.
assert g.scale(**{"lambda": 3.0, "vega": 4.0}) == 12.0
print("  ✓ positional, getattr and **{} access all behave as before")

print("\nAll .pyi keyword tests passed!")
