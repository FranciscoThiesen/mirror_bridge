"""C++ default arguments on constructors, the way methods already had them."""
import sys


def main():
    import ctor_defaults as cd

    c1 = cd.Conn("/db")
    assert (c1.path, c1.flags, c1.timeout, c1.vfs) == ("/db", 7, 0, "(none)"), vars(c1)

    c2 = cd.Conn("/db", 2)
    assert (c2.flags, c2.timeout, c2.vfs) == (2, 0, "(none)")

    c4 = cd.Conn("/db", 2, 5, "unix")
    assert (c4.flags, c4.timeout, c4.vfs) == (2, 5, "unix")

    m = cd.Mix(1)
    assert (m.a, m.b, m.c) == (1, 2, 3)
    assert (cd.Mix(1, 9).a, cd.Mix(1, 9).b, cd.Mix(1, 9).c) == (1, 9, 3)

    # A trailing keyword is fine; the C++ defaults fill the rest.
    mk = cd.Mix(1, b=9)
    assert (mk.a, mk.b, mk.c) == (1, 9, 3), (mk.a, mk.b, mk.c)

    # Skipping a defaulted parameter in the middle is refused, because filling
    # the gap would need the default's *value* and reflection exposes only
    # whether a default exists. Methods answer the same way, which is the
    # point: the two paths agree.
    for bad_kwargs in ({"c": 9}, {"z": 1}):
        try:
            cd.Mix(1, **bad_kwargs)
        except TypeError:
            pass
        else:
            raise AssertionError("Mix(1, **%r) should not construct" % bad_kwargs)
    try:
        cd.Mix(1).sum(1, z=9)
    except TypeError:
        pass
    else:
        raise AssertionError("sum(1, z=9) should raise, as the constructor does")

    assert cd.Mix(1).sum(1) == 321
    assert cd.Mix(1).sum(1, 2) == 303

    # Too few arguments is still an error, not a default-filled call.
    for bad in ((), (1, 2, 3, 4)):
        try:
            cd.Mix(*bad)
        except TypeError:
            pass
        else:
            raise AssertionError("Mix%r should not construct" % (bad,))

    s = cd.Strict(3, 4)
    assert (s.x, s.y) == (3, 4)
    try:
        cd.Strict(3)
    except TypeError:
        pass
    else:
        raise AssertionError("Strict(3) should not construct")

    # The stub has to agree with the runtime. A .pyi that demands every
    # parameter makes a type checker reject calls the module accepts.
    stubs = cd.__mirror_bridge_stubs__()
    import ast
    ast.parse(stubs)
    assert "def __init__(self, p: str, f: int = ..., t: int = ..., v: str = ...) -> None" in stubs, stubs
    assert "def sum(self, x: int, y: int = ..., z: int = ...) -> int" in stubs, stubs
    # Strict has no defaults, so nothing is marked optional.
    assert "def __init__(self, x: int, y: int) -> None" in stubs, stubs

    print("ctor_defaults: all assertions passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
