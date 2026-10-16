"""Tests for bulk array transfer optimization.

Numeric vectors (float, double, int, etc.) should be returned as
array.array objects via single memcpy instead of element-by-element
Python list construction.
"""

import sys
import os
import array

sys.path.insert(0, os.path.dirname(__file__))
import bulk_test

def test_float_array():
    gen = bulk_test.DataGenerator()
    gen.size = 100
    result = gen.get_floats()

    assert isinstance(result, array.array), f"Expected array.array, got {type(result)}"
    assert result.typecode == 'f', f"Expected typecode 'f', got '{result.typecode}'"
    assert len(result) == 100
    assert abs(result[0] - 0.0) < 1e-6
    assert abs(result[1] - 0.5) < 1e-6
    assert abs(result[10] - 5.0) < 1e-6
    print("  PASS: float array bulk transfer")

def test_double_array():
    gen = bulk_test.DataGenerator()
    gen.size = 100
    result = gen.get_doubles()

    assert isinstance(result, array.array), f"Expected array.array, got {type(result)}"
    assert result.typecode == 'd', f"Expected typecode 'd', got '{result.typecode}'"
    assert len(result) == 100
    import math
    assert abs(result[0] - math.sin(0.0)) < 1e-10
    assert abs(result[50] - math.sin(0.5)) < 1e-10
    print("  PASS: double array bulk transfer")

def test_int_array():
    gen = bulk_test.DataGenerator()
    gen.size = 100
    result = gen.get_ints()

    assert isinstance(result, array.array), f"Expected array.array, got {type(result)}"
    assert result.typecode == 'i', f"Expected typecode 'i', got '{result.typecode}'"
    assert len(result) == 100
    assert result[0] == 0
    assert result[5] == 25
    assert result[10] == 100
    print("  PASS: int array bulk transfer")

def test_short_array():
    gen = bulk_test.DataGenerator()
    gen.size = 50
    result = gen.get_shorts()

    assert isinstance(result, array.array), f"Expected array.array, got {type(result)}"
    assert result.typecode == 'h', f"Expected typecode 'h', got '{result.typecode}'"
    assert len(result) == 50
    assert result[0] == 0
    assert result[10] == 10
    print("  PASS: short array bulk transfer")

def test_string_list():
    """String vectors should still return as regular Python lists."""
    gen = bulk_test.DataGenerator()
    result = gen.get_strings()

    assert isinstance(result, list), f"Expected list, got {type(result)}"
    assert len(result) == 5
    assert result[0] == "item_0"
    assert result[4] == "item_4"
    print("  PASS: string list (element-by-element)")

def test_large_array_correctness():
    """Verify bulk transfer works correctly for large arrays."""
    gen = bulk_test.DataGenerator()
    gen.size = 10000
    result = gen.get_floats()

    assert len(result) == 10000
    assert abs(result[9999] - 9999.0 * 0.5) < 1e-3
    print("  PASS: large array correctness")

# MIRROR_BRIDGE_ARRAY_BORROW_BYTES in python/mirror_bridge_python.hpp. A return
# smaller than this copies through an intermediate bytes object; one at least
# this big borrows the container's storage into a memoryview and lets
# array.frombytes copy once. Two code paths, so every typecode has to be
# exercised on both sides of the line -- every test above this point happens to
# sit on the small side.
BORROW_BYTES = 1024

ACCESSORS = [
    # name, typecode, bytes per element, expected value at index i
    ("get_floats",  "f", 4, lambda i: i * 0.5),
    ("get_doubles", "d", 8, lambda i: __import__("math").sin(i * 0.01)),
    ("get_ints",    "i", 4, lambda i: i * i),
    ("get_shorts",  "h", 2, lambda i: i % 1000),
]


def _check(gen, name, typecode, expect, n):
    result = getattr(gen, name)()
    assert isinstance(result, array.array), f"{name}({n}) gave {type(result)}"
    assert result.typecode == typecode, f"{name}({n}) typecode {result.typecode}"
    assert len(result) == n, f"{name}({n}) returned {len(result)} elements"
    for i in (0, 1, n // 2, n - 1):
        if i < 0 or i >= n:
            continue
        want = expect(i)
        got = result[i]
        if isinstance(want, float):
            assert abs(got - want) < 1e-5, f"{name}({n})[{i}] = {got}, want {want}"
        else:
            assert got == want, f"{name}({n})[{i}] = {got}, want {want}"
    return result


def test_both_paths_agree_for_every_typecode():
    """The small and large paths must produce the same values and typecode."""
    gen = bulk_test.DataGenerator()
    for name, typecode, width, expect in ACCESSORS:
        below = max(1, (BORROW_BYTES // width) - 1)
        at = BORROW_BYTES // width
        above = (BORROW_BYTES // width) * 4

        for n in (below, at, above):
            gen.size = n
            _check(gen, name, typecode, expect, n)

        # Same length on both sides of nothing: a buffer exactly at the
        # threshold takes the borrowing path, one byte under it does not, and
        # the elements they share must match.
        gen.size = at
        borrowed = _check(gen, name, typecode, expect, at)
        gen.size = below
        copied = _check(gen, name, typecode, expect, below)
        assert list(copied) == list(borrowed)[:below], \
            f"{name} disagrees across the threshold"
    print("  PASS: both conversion paths agree for f, d, i and h")


def test_empty_and_single_element():
    """A zero-length vector has no storage to borrow, and must not crash."""
    gen = bulk_test.DataGenerator()
    gen.size = 0
    for name, typecode, _, _ in ACCESSORS:
        result = getattr(gen, name)()
        assert isinstance(result, array.array), f"empty {name} gave {type(result)}"
        assert len(result) == 0, f"empty {name} returned {len(result)}"
        assert result.typecode == typecode
    gen.size = 1
    assert len(gen.get_doubles()) == 1
    print("  PASS: zero-length and single-element returns")


def test_a_borrowed_return_outlives_the_call():
    """The memoryview borrows C++ storage that dies when the call returns.

    If the array ever held the view rather than copying out of it, this would
    be reading freed memory. Force the C++ side to reuse that memory before
    looking at the result.
    """
    import math
    gen = bulk_test.DataGenerator()
    gen.size = 4096
    first = gen.get_doubles()
    snapshot = list(first)
    for _ in range(20):
        gen.get_doubles()
        gen.get_floats()
    assert list(first) == snapshot, "a borrowed return changed after the fact"
    assert abs(first[100] - math.sin(1.0)) < 1e-10
    print("  PASS: a borrowed return is a copy, not a window")


if __name__ == "__main__":
    print("Testing bulk array transfer:")
    test_float_array()
    test_double_array()
    test_int_array()
    test_short_array()
    test_string_list()
    test_large_array_correctness()
    test_both_paths_agree_for_every_typecode()
    test_empty_and_single_element()
    test_a_borrowed_return_outlives_the_call()
    print("\nAll bulk transfer tests passed!")
