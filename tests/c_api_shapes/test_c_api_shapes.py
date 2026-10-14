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

    # A null `const char*` reads as None rather than taking the interpreter
    # with it. Setting and reading back still works.
    lab = cs.Label()
    assert lab.id == 1
    assert lab.text is None, repr(lab.text)
    lab.text = "hello"
    assert lab.text == "hello"

    # A `const char*` return crosses as a str, and the null a C API uses for
    # "absent" crosses as None. This is tinyxml2's Attribute() shape.
    at = cs.Attributes()
    assert at.c_str() == "text", repr(at.c_str())
    assert at.attribute("tag") == "node", repr(at.attribute("tag"))
    assert at.attribute("missing") is None, repr(at.attribute("missing"))
    assert at.tag == "node", repr(at.tag)

    # `char*` is the caller's to free, so it is not bound: copying it and
    # walking away would leak, and the class keeps its other methods.
    assert not hasattr(at, "owned"), "a char* return should not be bound"

    print("c_api_shapes: all assertions passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
