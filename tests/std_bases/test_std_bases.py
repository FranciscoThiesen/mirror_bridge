"""Methods inherited from a standard-library base are not bound."""
import sys


def main():
    import std_bases as sb

    c = sb.Customize()
    assert c.own_method() == 7

    # std::pair::swap and friends are not part of the bound surface. Binding
    # them is what stopped the module compiling at all.
    for name in ("swap", "first", "second"):
        assert not hasattr(c, name), name

    # A user-defined base still contributes its methods.
    d = sb.Derived()
    assert d.own() == 2
    assert d.inherited() == 1

    # A non-template std base is real API and stays. The rule is about
    # standard-library *templates*, whose constraints a reflected call does
    # not satisfy; std::runtime_error has no such problem and what() is the
    # reason a class derives from it at all.
    e = sb.AppError()
    assert e.code() == 42, e.code()
    assert hasattr(e, "what"), "what() was dropped from a std::runtime_error subclass"
    assert e.what() == "boom", e.what()

    # Only ::std is reserved. A user namespace that happens to be called std
    # keeps its methods; an unanchored name match silently took them away.
    u = sb.UsesLookalikeBase()
    assert u.own() == 12, u.own()
    assert hasattr(u, "helper_method"), \
        "a user namespace named std is not the standard library"
    assert u.helper_method() == 11, u.helper_method()

    print("std_bases: all assertions passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
