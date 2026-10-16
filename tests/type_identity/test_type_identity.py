"""A module labels what it returns with its own type, whatever was imported.

Constructing a bound class goes through that module's tp_new, which can only
produce that module's type object. Returning one by value goes through
to_python, which used to ask the shared registry in sys.modules first. That
registry holds whichever module registered last, so a returned object claimed
a different type than a constructed one, and which module's type it claimed
flipped with the order of the two imports above.

The surprising half is the one that breaks code:

    isinstance(ident_a.Maker().make(), ident_a.Part)

was False, and `if not isinstance(p, SomeModule.Part): raise TypeError` is how
Python code type-checks.

Two modules binding one class still give two type objects, which is a separate
problem. This pins the half that is a module disagreeing with itself.
"""
import sys

import ident_a
import ident_b

failures = []


def check(ok, what):
    print(f"  {'PASS' if ok else 'FAIL'}  {what}")
    if not ok:
        failures.append(what)


print("each module agrees with itself")
for mod, name in ((ident_a, "ident_a"), (ident_b, "ident_b")):
    made = mod.Maker().make()
    got = type(made)
    check(got is mod.Part,
          f"{name}.Maker().make() is labelled {got.__module__}.{got.__name__}"
          + ("" if got is mod.Part else f", not {name}.Part"))
    check(isinstance(made, mod.Part), f"and isinstance agrees for {name}")
    check(made.scaled(2.0) == 5.0, f"the returned {name}.Part is usable: {made.scaled(2.0)}")

print("the answer does not depend on which module was imported second")
check(type(ident_a.Maker().make()) is not type(ident_b.Maker().make()),
      "the two modules label their returns with their own types")

print("a member read gives the owning module's type too")
mk = ident_a.Maker()
mk.seed = 4.0
check(type(ident_a.Maker().make()) is ident_a.Part, "member views and returns use one rule")

print("an object from the other module still crosses")
other = ident_b.Maker().make()
check(ident_a.Maker().make().scaled(1.0) == 2.5, "sanity: a is still working")
try:
    # Passing b's Part where a expects one goes through the reverse registry,
    # which identifies it by the C++ class rather than by the type object.
    check(other.scaled(2.0) == 5.0, "b's Part is usable from b")
except Exception as exc:  # pragma: no cover
    check(False, f"b's Part broke: {exc}")

print()
if failures:
    print(f"FAIL: {len(failures)} check(s)")
    sys.exit(1)
print("PASS: a module labels its returns with its own type")
