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

    print("std_bases: all assertions passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
