#!/usr/bin/env python3
"""Class-typed arguments are identity-checked at the Python boundary.

Before the check, every bound class shared one wrapper layout with no way to
tell them apart, so reading the wrapper of whatever object arrived was
"successful" for any object at all. Passing an int, a str, a dict or a list
where a class was expected segfaulted the interpreter, and passing a
different bound class returned a number computed from that class's bytes.

These tests pin both halves of the fix: wrong types raise TypeError and the
process survives, while the derived-to-base conversions that are supposed to
work still do, now including bases that are not at offset zero.
"""
import os
import sys

# ctest sets PYTHONPATH to the build tree; this covers a direct run too.
sys.path.append(os.path.join(os.path.dirname(__file__), "..", "..", "build"))
sys.path.append(os.path.join(os.path.dirname(__file__), "..", "..", "build_ci", "tests"))

import type_safety as ts

# Everything that is not the expected class. Each of these used to crash the
# interpreter outright, except None, which was already rejected.
WRONG = [
    ("int", 42),
    ("float", 3.5),
    ("str", "text"),
    ("dict", {"rate": 0.5}),
    ("list", [1, 2, 3]),
    ("tuple", (1, 2)),
    ("None", None),
    ("bool", True),
    ("bytes", b"raw"),
]


def assert_rejects(call, label):
    for name, value in WRONG:
        try:
            call(value)
        except TypeError:
            continue
        raise AssertionError(f"{label} accepted a {name}")


def test_free_function_rejects_wrong_types():
    assert_rejects(ts.take_curve_cref, "take_curve_cref(const Curve&)")
    assert_rejects(ts.take_curve_value, "take_curve_value(Curve)")
    assert_rejects(ts.take_label, "take_label(const Label&)")
    print("✓ free functions reject every wrong type")


def test_unrelated_bound_class_is_rejected():
    # The dangerous case: a real wrapper, so every null check passes, but the
    # wrong class. This used to read a std::string's bytes as a double.
    label, curve = ts.Label(), ts.Curve()
    for fn, arg, label_text in [
        (ts.take_curve_cref, label, "take_curve_cref(Label)"),
        (ts.take_label, curve, "take_label(Curve)"),
        (ts.take_first, curve, "take_first(Curve)"),
    ]:
        try:
            fn(arg)
            raise AssertionError(f"{label_text} accepted an unrelated class")
        except TypeError:
            pass
    print("✓ an unrelated bound class is rejected, not reinterpreted")


def test_correct_types_still_work():
    assert ts.take_curve_cref(ts.Curve()) == 0.1
    assert ts.take_curve_value(ts.Curve()) == 0.2
    assert ts.take_label(ts.Label()) == len(ts.Label().tag)
    print("✓ the correct type still converts")


def test_method_and_constructor_parameters():
    p = ts.Portfolio()
    assert p.book(ts.Curve()) == 0.05
    assert_rejects(p.book, "Portfolio.book(const Curve&)")
    assert_rejects(ts.Portfolio, "Portfolio(const Curve&)")
    try:
        p.book(ts.Label())
        raise AssertionError("Portfolio.book accepted a Label")
    except TypeError:
        pass
    print("✓ method and constructor parameters are checked")


def test_member_assignment_is_checked():
    p = ts.Portfolio()
    for name, value in WRONG:
        try:
            p.held = value
        except TypeError:
            continue
        raise AssertionError(f"Portfolio.held accepted a {name}")
    p.held = ts.Curve()
    assert p.held.rate == 0.05
    print("✓ class-typed member assignment is checked")


def test_overload_set_picks_the_right_one():
    # value(const Curve&) and value(const Label&). Without an identity check
    # the first overload swallowed everything.
    p = ts.Portfolio()
    assert p.value(ts.Curve()) == 0.05
    assert p.value(ts.Label()) == len(ts.Label().tag)
    try:
        p.value(42)
        raise AssertionError("Portfolio.value accepted an int")
    except TypeError:
        pass
    print("✓ overload resolution distinguishes the two classes")


def test_single_inheritance_still_converts():
    # Base is at offset 0, so this worked before the check. Regression guard.
    assert ts.take_base(ts.Derived()) == 7
    assert ts.take_base(ts.Base()) == 7
    print("✓ derived-to-base still converts (offset 0)")


def test_multiple_inheritance_is_now_correct():
    # Second is not at offset 0. Reusing the object's address read First's
    # bytes and produced a garbage double with no error.
    both = ts.Both()
    assert ts.take_first(both) == 11111, "first base"
    # take_first alone would pass without the fix (First is at offset 0);
    # these two are what make the test load-bearing.
    try:
        ts.take_first(ts.Second())
        raise AssertionError("take_first accepted an unrelated Second")
    except TypeError:
        pass
    assert ts.take_second(both) == 22222.0, "second base needs an offset adjustment"
    assert ts.take_second(ts.Second()) == 22222.0
    print("✓ conversion to a non-first base lands on the right subobject")


def test_virtual_base_conversion():
    # A virtual base has no compile-time offset, so reusing the derived
    # object's address returned nonsense. The recorded thunk resolves it at
    # runtime, which is the whole reason conversions go through one.
    assert ts.take_vbase(ts.VDiamond()) == 777
    assert ts.take_vbase(ts.VBase()) == 777
    print("✓ conversion to a virtual base resolves the offset at runtime")


def test_downcast_is_refused():
    # Derived-to-base is a conversion; base-to-derived is not. A Base has no
    # `extra` member, so reading one used to return whatever followed it.
    assert ts.take_derived(ts.Derived()) == 99
    try:
        ts.take_derived(ts.Base())
        raise AssertionError("take_derived accepted a Base")
    except TypeError:
        pass
    print("✓ a base passed where a derived class is expected is refused")


def test_pointer_storage_paths():
    # Abstract and non-copyable parameters alias the wrapper's object rather
    # than copying it, which is a second, separate unchecked cast.
    # Shape is abstract, so it is never bound and has no Python type object:
    # the conversion can only succeed through the recorded upcast.
    insp = ts.Inspector()
    assert insp.area_of(ts.Square()) == 9.0
    assert insp.id_of(ts.Handle()) == 1234
    assert_rejects(insp.area_of, "Inspector.area_of(const Shape&)")
    assert_rejects(insp.id_of, "Inspector.id_of(const Handle&)")
    try:
        insp.area_of(ts.Curve())
        raise AssertionError("Inspector.area_of accepted a Curve")
    except TypeError:
        pass
    # Pointer storage reaching a base that is not at offset 0: the aliased
    # pointer needs the same adjustment a copied value gets.
    assert insp.second_of(ts.Both()) == 22222.0
    assert insp.second_of(ts.Second()) == 22222.0
    print("✓ pointer-storage parameters are checked and offset-adjusted")


def test_container_elements_are_checked():
    assert ts.sum_curves([ts.Curve(), ts.Curve()]) == 0.1
    for bad in ([42], [ts.Label()], ["text"]):
        try:
            ts.sum_curves(bad)
            raise AssertionError(f"sum_curves accepted {bad!r}")
        except TypeError:
            pass
    print("✓ container elements go through the same check")


def test_conversion_operator_does_not_break_the_build():
    # A conversion function has no identifier. It used to pass the method
    # filter and then make identifier_of ill-formed, so the module did not
    # compile at all. If this import worked, the guard is in place.
    t = ts.Ticks()
    assert t.raw() == 250
    assert not hasattr(t, "operator double"), "the conversion function is skipped, not bound"
    print("✓ a class with a conversion operator still binds")


def test_python_subclass_is_still_accepted():
    # A Python subclass extends the wrapper at the tail, so the C++ object is
    # still where it was. This is the trampoline case and must keep working.
    try:
        class Tweaked(ts.Curve):
            pass
    except TypeError:
        print("- bound classes are not subclassable here; skipped")
        return
    sub = Tweaked()
    assert ts.take_curve_cref(sub) == 0.1
    print("✓ a Python subclass of a bound class is still accepted")


if __name__ == "__main__":
    for name, fn in sorted(globals().items()):
        if name.startswith("test_") and callable(fn):
            fn()
    print("All type safety tests passed")
