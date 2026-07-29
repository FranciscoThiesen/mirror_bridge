#!/usr/bin/env python3
"""Every module exposes __mirror_bridge_stubs__() with reflection-exact
signatures: real C++ parameter names, exact member types, no runtime parsing."""

import sys
import os

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))

import byref

print("Test 1: modules expose __mirror_bridge_stubs__...")
assert hasattr(byref, '__mirror_bridge_stubs__')
stubs = byref.__mirror_bridge_stubs__()
assert isinstance(stubs, str) and stubs
print(f"  ✓ returned {len(stubs.splitlines())} lines")

print("Test 2: stub content is valid Python...")
import ast
ast.parse(stubs)
print("  ✓ ast.parse accepts it")

print("Test 3: classes, members, and REAL C++ parameter names present...")
assert "class Tracked:" in stubs, stubs
assert "class Mutator:" in stubs, stubs
assert "value: int" in stubs, stubs
# read(const Tracked& t) — the parameter is named 't' in the header, and the
# stub must carry that name (this is what runtime-parsing generators can't do)
assert "def read(self, t: Tracked) -> int" in stubs, stubs
assert "def bump(self, t: Tracked) -> None" in stubs, stubs
print("  ✓ real parameter names carried into stubs")

print("Test 4: static methods marked @staticmethod...")
assert "@staticmethod" in stubs, stubs
assert "def copies() -> int" in stubs, stubs
print("  ✓ statics rendered")

print("\nAll reflection-stub tests passed!")
