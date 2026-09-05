#!/usr/bin/env python3
"""Template instantiations planned by `mirror_bridge generate` (no hints in the header)."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "build"))

import geom

try:
    import numpy as np
except ImportError:   # numpy keys are a convenience, not a requirement
    np = None


def test_aliases_are_names():
    v = geom.Vec3f(1.0, 2.0, 3.0)
    assert type(v).__name__ == "Vec3f"
    assert abs(v.norm() - 14 ** 0.5) < 1e-6
    assert geom.Vec3d is geom.Vector3[float]
    assert geom.Vec3i is geom.Vector3["int"]
    # Python's int is a C long: with both Vector3<int> and Vector3<long>
    # bound, the builtin key picks the exact width.
    assert geom.Vector3[int] is geom.Vector3_long
    assert geom.Mat3f is geom.Matrix[float, 3]
    print("✓ aliases name their instantiations")


def test_family_subscript():
    fam = geom.Vector3
    assert "instantiations" in repr(fam) or "template" in repr(fam), repr(fam)
    assert set(fam.instantiations) >= {"float", "double", "int", "long", "bool"}
    assert len(fam) == len(fam.instantiations)
    assert fam["float"] is geom.Vec3f
    if np is not None:
        assert fam[np.float32] is geom.Vec3f
        assert fam[np.int64] is geom.Vector3_long
    try:
        fam[str]
        assert False, "Vector3[str] should not exist"
    except KeyError as e:
        assert "available" in str(e)
    print("✓ family subscript")


def test_ctad_like_construction():
    assert type(geom.Vector3(1.0, 2.0, 3.0)).__name__ == "Vec3d"
    assert type(geom.Vector3(1, 2, 3)).__name__ == "Vector3_long"
    b = geom.Vector3[bool]()
    b.x = True
    b.z = True
    assert b.count() == 2
    assert geom.count_true(b) == 2
    print("✓ CTAD-like construction")


def test_member_templates():
    v = geom.Vec3f(1.5, 2.5, 3.5)
    assert type(v.cast["int"]()).__name__ == "Vec3i"
    assert type(v.cast[int]()).__name__ == "Vector3_long"
    assert v.cast[int]().x == 1
    assert type(geom.Vec3f.cast[float](v)).__name__ == "Vec3d"
    try:
        geom.Vec3f.cast()
        assert False
    except TypeError:
        pass
    r = geom.Robot()
    r.ticks = geom.Vector3_long(4, 5, 6)
    assert type(r.ticks_as[float]()).__name__ == "Vec3d"
    assert r.ticks_as[float]().z == 6.0
    print("✓ member templates (class template and plain class)")


def test_function_families():
    assert geom.clamp(1.5, 0.0, 1.0) == 1.0
    assert set(geom.clamp.instantiations) == {"double", "float"}
    assert geom.twice(21) == 42
    assert geom.twice("ab") == "abab"
    if np is not None:
        assert geom.twice[np.int16](7) == 14
    assert geom.mix(2, 0.5) == 1.25
    assert geom.largest(geom.Vec3f(1.0, 9.0, 3.0)) == 9.0
    try:
        geom.clamp("a", 1, 2)
        assert False
    except TypeError as e:
        assert "available" in str(e)
    assert not hasattr(geom, "deref"), "T* parameter cannot be bound"
    print("✓ function families")


def test_free_functions_and_skips():
    v = geom.Vec3f(1.0, 2.0, 3.0)
    assert geom.dot3(v, v) == 14.0
    assert geom.answer() == 42, "algo.hpp has no classes but its functions are bound"
    assert not hasattr(geom, "labels"), "returns Vector3<std::string>, whose dot() does not compile"
    assert not hasattr(geom, "overloaded"), "overload sets are not bound (yet)"
    print("✓ free functions bound, unbindable ones skipped")


def test_baseline_and_containers():
    s = geom.Stack[str]()
    s.push("a")
    s.push("b")
    assert s.size() == 2 and s.pop() == "b"
    assert type(geom.Stack[int]()).__name__ == "Stack_long"
    # --instantiate adds to the baseline instead of replacing it
    assert set(geom.Stack.instantiations) == {"bool", "long", "double", "std::string", "float"}
    assert geom.twice["short"](7) == 14
    r = geom.Robot()
    r.path = [geom.Vec3f(1.0, 0.0, 0.0), geom.Vec3f(0.0, 1.0, 0.0)]
    assert len(r.path) == 2 and r.path[1].y == 1.0
    g = geom.Grid()
    g.cells = [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]
    assert g.sum() == 21.0
    assert g.cells[1][2] == 6.0
    print("✓ baseline instantiations and containers")


def test_stubs():
    pyi = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build", "geom.pyi")
    text = open(pyi).read()
    for needle in ("class Vec3f:", "class Vector3_long:", "position: Vec3d", "path: list[Vec3f]",
                   "def dot3(a: Vec3f, b: Vec3f) -> float", "def answer() -> int"):
        assert needle in text, needle
    print("✓ stubs name instantiations")


if __name__ == "__main__":
    for name, fn in sorted(globals().items()):
        if name.startswith("test_") and callable(fn):
            fn()
    print("All template instantiation tests passed")
