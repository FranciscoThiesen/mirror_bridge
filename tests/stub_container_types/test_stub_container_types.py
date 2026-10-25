"""The .pyi says what the runtime actually hands back.

A std::vector<double> crosses as an array.array, not a list. The stub said
list[float] anyway, which is worse than saying nothing: a type checker then
approves `result + [1.0]` and `json.dumps(result)` and the agent reading the
stub to write the call has no way to find out.

The central test here is a round trip. For every zero-argument accessor on
the fixture, read the return annotation out of the generated stub and assert
the runtime value is an instance of what it claims. That catches any future
divergence between type_hint and to_python without caring which type it is.

The accessors around it are chosen to separate mirroring the typecode table
from guessing: long double and signed char are numeric vectors with no
typecode, so they are lists, and a stub that says "numeric vector means
array" gets them wrong.
"""
import array
from collections.abc import MutableSequence
import ast
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build', 'tests'))

import stub_container_types as mod

failures = []


def check(ok, what):
    print(f"  {'PASS' if ok else 'FAIL'}  {what}")
    if not ok:
        failures.append(what)


STUBS = mod.__mirror_bridge_stubs__()
tree = ast.parse(STUBS)
print(f"Test 1: the stub parses ({len(STUBS.splitlines())} lines)")
check(True, "ast.parse accepts it")

cls = next(n for n in ast.walk(tree) if isinstance(n, ast.ClassDef) and n.name == "Shapes")
returns = {}
params = {}
props = {}
for node in cls.body:
    if isinstance(node, ast.FunctionDef):
        decorators = [ast.unparse(d) for d in node.decorator_list]
        if "property" in decorators:
            props.setdefault(node.name, {})["get"] = ast.unparse(node.returns)
        elif any(d.endswith(".setter") for d in decorators):
            props.setdefault(node.name, {})["set"] = ast.unparse(node.args.args[1].annotation)
        else:
            returns[node.name] = ast.unparse(node.returns)
            params[node.name] = [ast.unparse(a.annotation) for a in node.args.args[1:]]
    elif isinstance(node, ast.AnnAssign):
        props.setdefault(node.target.id, {})["bare"] = ast.unparse(node.annotation)

print("Test 2: the typecode table is mirrored, not approximated")
EXPECTED = {
    "doubles": "_array[float]",
    "floats": "_array[float]",
    "ints": "_array[int]",
    "shorts": "_array[int]",
    # numeric vectors the array module has no typecode for
    "long_doubles": "list[float]",
    "signed_chars": "list[int]",
    # excluded from the bulk path by name
    "bools": "list[bool]",
    "blob": "bytes",
    # not a std::vector
    "deque_doubles": "list[float]",
    # std::array has its own to_python overload and it builds a list
    "fixed": "list[float]",
    "fixed_bytes": "list[int]",
    # char is arithmetic and signed, so it crosses as int
    "chars": "list[int]",
    "first_tag": "int",
    "strings": "list[str]",
    # only the inner vector takes the bulk path
    "grid": "list[_array[float]]",
}
for name, want in EXPECTED.items():
    check(returns.get(name) == want,
          f"{name}() -> {returns.get(name)}" + ("" if returns.get(name) == want else f", wanted {want}"))

print("Test 3: every annotation matches the object the call actually returns")
# This is the one that keeps the two in step whatever is added later.
SPELLING = {
    "_array[float]": array.array, "_array[int]": array.array,
    "list[float]": list, "list[int]": list, "list[bool]": list,
    "list[str]": list, "list[_array[float]]": list,
    "bytes": bytes, "int": int, "float": float,
}
obj = mod.Shapes()
for name, declared in sorted(returns.items()):
    if name.startswith("__") or params.get(name):
        continue
    want = SPELLING.get(declared)
    if want is None:
        check(False, f"{name}() declares {declared}, which this test has no mapping for")
        continue
    got = getattr(obj, name)()
    check(isinstance(got, want),
          f"{name}() declares {declared} and returned {type(got).__name__}")

print("Test 4: a parameter is not narrowed to array.array")
check(params.get("total") == ["list[float]"], f"total takes {params.get('total')}")
check("_array[" not in str(params.get("total")), "and not array.array, which would refuse a list")
check(params.get("blob_size") == ["bytes | list[int]"], f"blob_size takes {params.get('blob_size')}")

print("Test 5: and the runtime really does accept all of those")
check(obj.total([1.0, 2.0]) == 3.0, "a list")
check(obj.total((1.0, 2.0)) == 3.0, "a tuple")
check(obj.total(array.array("d", [1.0, 2.0])) == 3.0, "an array.array, which is what a return gives")
try:
    obj.total({1.0, 2.0})
    check(False, "a set was accepted, so Sequence would have been the right word after all")
except TypeError:
    check(True, "a set is refused, which is why the hint is not Iterable")
try:
    obj.total(b"ab")
    check(False, "bytes was accepted")
except TypeError:
    check(True, "bytes is refused, which is why the hint is not Sequence[float]")

print("Test 6: a member that reads and writes differently says both")
check(props.get("xs", {}).get("get") == "_MutableSequence[float]",
      f"xs reads as {props.get('xs')}")
check(props.get("xs", {}).get("set") == "list[float]", "and accepts a list")
check(props.get("gain", {}).get("bare") == "float",
      f"while a plain member stays a plain annotation: {props.get('gain')}")
check(props.get("tag", {}).get("bare") == "int", f"and a char member is an int: {props.get('tag')}")
# A member read is a live view, so it is a MutableSequence and not the
# array.array a method return of the same C++ type still gives. The stub
# says MutableSequence for exactly this reason, and isinstance has to agree
# with it or the annotation is the same kind of lie this file exists to stop.
check(isinstance(obj.xs, MutableSequence),
      f"reading xs really gives a MutableSequence, not {type(obj.xs).__name__}")
check(not isinstance(obj.xs, array.array),
      "a member read is not the array.array a method return gives")
check(isinstance(obj.doubles(), array.array),
      f"while a method return still gives {type(obj.doubles()).__name__}")
obj.xs = [7.0, 8.0]
check(list(obj.xs) == [7.0, 8.0], "and assigning a list really works")
obj.xs.append(9.0)
check(list(obj.xs) == [7.0, 8.0, 9.0], "and appending through the view reaches C++")
check(isinstance(obj.tag, int), f"and tag really is an int: {obj.tag!r}")

print()
if failures:
    print(f"FAIL: {len(failures)} check(s)")
    sys.exit(1)
print("PASS: the stub and the runtime agree")
