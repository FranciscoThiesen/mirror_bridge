"""An enum reaches Python as an enum, not as a bare int with no names.

Values always crossed correctly. The types did not, so a module with a
Color parameter exposed 0, 1 and 2 with nothing in it saying which was
which, and the caller had to open the C++ header. enum.IntEnum is the
target because an IntEnum member IS an int: every existing conversion keeps
working unchanged, which is why this is additive rather than a break.
"""
import ast
import enum
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build', 'tests'))

import enums_test as mod

failures = []


def check(ok, what):
    print(f"  {'PASS' if ok else 'FAIL'}  {what}")
    if not ok:
        failures.append(what)


print("Test 1: the enum is an IntEnum with the names the C++ gave it")
check(issubclass(mod.Color, enum.IntEnum), f"Color is {mod.Color.__mro__[1].__name__}")
check([m.name for m in mod.Color] == ["Red", "Green", "Blue"], f"{list(mod.Color)}")
check(mod.Color.Red == 0 and mod.Color.Blue == 2, "values match the C++ enumerators")
check(mod.Color.__module__ == "enums_test",
      f"__module__ is the module that bound it, not the guess: {mod.Color.__module__}")

print("Test 2: values are whatever C++ says, not a renumbering")
check([(m.name, int(m)) for m in mod.Signal] ==
      [("Quit", -1), ("Idle", 0), ("Run", 7), ("Halt", 100)], f"{list(mod.Signal)}")

print("Test 3: an unscoped enum is bound the same way")
check([m.name for m in mod.Plain] == ["Alpha", "Beta", "Gamma"], f"{list(mod.Plain)}")

print("Test 4: a nested enum is bound under its unqualified name")
check([m.name for m in mod.Kind] == ["First", "Second"], f"{list(mod.Kind)}")

print("Test 5: an enumerator that is a Python keyword gets a usable name")
# `Permission.None` is a SyntaxError, not an AttributeError, so leaving the
# name alone would make the member unreachable from Python source entirely.
check([m.name for m in mod.Permission] == ["None_", "Read", "Write", "All"],
      f"{[m.name for m in mod.Permission]}")
check(mod.Permission.None_ == 0, "and it still has the C++ value")
check([m.name for m in mod.Awkward] == ["None__", "None_"],
      f"a clash with an existing None_ keeps going: {[m.name for m in mod.Awkward]}")
check(mod.Awkward.None__ == 0 and mod.Awkward.None_ == 1, "with the values unswapped")

print("Test 6: nothing about the int conversion changed")
check(mod.name_of(mod.Color.Green) == "green", "a member goes in where an int went in")
check(mod.name_of(1) == "green", "and a bare int still does")
check(mod.brighter(mod.Color.Red) == mod.Color.Green,
      f"a returned enum compares equal to the member: {mod.brighter(mod.Color.Red)}")
check(mod.permission_bits(mod.Permission.All) == 3, "the renamed enum passes too")

print("Test 7: enum-typed members read and write")
light = mod.Light()
check(light.colour == mod.Color.Red, f"default reads as Red: {light.colour}")
light.colour = mod.Color.Blue
check(light.get_colour() == mod.Color.Blue, "assignment reached C++")
check(light.is_red() is False, "and C++ sees the new value")
light.colour = 1
check(light.get_colour() == mod.Color.Green, "a bare int still assigns")
outer = mod.Outer()
check(outer.kind == mod.Kind.First, f"a nested enum member: {outer.kind}")

print("Test 8: the stub declares the enums and annotates with them")
stubs = mod.__mirror_bridge_stubs__()
ast.parse(stubs)
check(True, "the stub parses with the enum classes in it")
check("from enum import IntEnum as _IntEnum" in stubs, "the import is there")
check("class Color(_IntEnum):" in stubs, "and the class")
for want in ("    Red = 0", "    Green = 1", "    Blue = 2"):
    check(want in stubs, f"with its members: {want.strip()}")
check("    Quit = -1" in stubs, "a negative value survives the stub")
check("    None_ = 0" in stubs, "and the renamed member is what the stub declares")
check("None = 0" not in stubs, "never the keyword, which would not parse")

tree = ast.parse(stubs)
light_cls = next(n for n in ast.walk(tree) if isinstance(n, ast.ClassDef) and n.name == "Light")
anns = {n.target.id: ast.unparse(n.annotation)
        for n in light_cls.body if isinstance(n, ast.AnnAssign)}
check(anns.get("colour") == "Color", f"a member is annotated with the enum: {anns}")
rets = {n.name: ast.unparse(n.returns)
        for n in light_cls.body if isinstance(n, ast.FunctionDef)}
check(rets.get("get_colour") == "Color", f"and so is a return: {rets.get('get_colour')}")
# A free function's parameters are positional-only in the stub, so they are
# in posonlyargs rather than args.
funcs = {n.name: (ast.unparse((n.args.posonlyargs + n.args.args)[0].annotation),
                  ast.unparse(n.returns))
         for n in tree.body if isinstance(n, ast.FunctionDef)}
check(funcs.get("name_of") == ("Color", "str"), f"and a free function: {funcs.get('name_of')}")

print("Test 9: the enum classes come before the classes that name them")
check(stubs.index("class Color(_IntEnum):") < stubs.index("class Light:"),
      "so the file reads top to bottom")

print()
if failures:
    print(f"FAIL: {len(failures)} check(s)")
    sys.exit(1)
print("PASS: enums cross as enums")
