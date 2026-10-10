"""Free functions and methods whose parameters are declared `T&&`."""
import sys


def main():
    import rvalue_params as rp

    assert rp.consume_bool(True) is True
    assert rp.consume_bool(False) is False

    assert rp.consume_string("hi") == "hi!"
    assert rp.consume_vector([1, 2, 3]) == 3
    assert rp.consume_vector([]) == 0

    assert rp.mixed(7, "x", 2.5) == "x:7:2"

    # A method taking int&& goes through the same forward_arg branch.
    c = rp.Counter()
    c.add(5)
    c.add(4)
    assert c.total == 9, c.total

    print("rvalue_params: all assertions passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
