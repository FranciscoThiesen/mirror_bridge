"""Bit-fields, anonymous unions, and a forward-declared type in the universe."""
import sys


def main():
    import c_api_shapes as cs

    # A bit-field is read by value, because no reference can bind to one.
    g = cs.Glyph()
    g.codepoint = 0x1F600
    g.visible = 1
    g.advance_x = 2.5
    assert g.codepoint == 0x1F600, g.codepoint
    assert g.visible == 1
    assert g.advance_x == 2.5

    # Narrow fields still truncate the way C++ does; the binding does not
    # pretend otherwise.
    g.colored = 1
    assert g.colored == 1

    # The named members beside an anonymous union survive; the union does not.
    p = cs.StoragePair()
    p.key = 11
    p.tag = 12
    assert (p.key, p.tag) == (11, 12)
    for aliased in ("val_i", "val_f", "val_p"):
        assert not hasattr(p, aliased), aliased

    # A class naming a forward-declared type declines -- its `Opaque*` member
    # has no converter -- and says so at import. What matters is that it costs
    # only itself: the module imports and everything beside it works.
    assert not hasattr(cs, "HoldsOpaque")
    assert not hasattr(cs, "Opaque")

    print("c_api_shapes: all assertions passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
