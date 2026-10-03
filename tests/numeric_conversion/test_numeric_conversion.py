#!/usr/bin/env python3
"""Integer and float arguments are range-checked, and a refusal is clean.

A Python int has no width. A C++ int does. Reading one into the other used to
be an unchecked cast, so 2**31 into an `int` arrived as -2147483648 and 2**200
arrived as -1 with an OverflowError left set on the thread — which then
surfaced at whatever unrelated call ran next.

Both halves are pinned here: an out-of-range value raises TypeError, and the
interpreter is left with no pending error afterwards. The in-range cases are
pinned too, because the conversion now reads CPython's compact integer
representation directly and the boundaries of that representation are exactly
where an off-by-one would hide.
"""
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "..", "..", "build"))
sys.path.append(os.path.join(os.path.dirname(__file__), "..", "..", "build_ci", "tests"))

import numeric_conversion as nc

n = nc.Numbers()
failures = []


def _try(call):
    """Returns the exception instead of raising, for assertions that want to
    inspect which error a refusal produced."""
    try:
        return call()
    except Exception as exc:
        return exc


def accepts(label, call, expected):
    try:
        got = call()
    except Exception as exc:
        failures.append(f"{label}: raised {type(exc).__name__}({exc}), expected {expected!r}")
        return
    if got != expected:
        failures.append(f"{label}: got {got!r}, expected {expected!r}")


def rejects(label, call):
    try:
        call()
    except TypeError:
        return
    except Exception as exc:
        failures.append(f"{label}: raised {type(exc).__name__}, expected TypeError")
        return
    failures.append(f"{label}: accepted a value it cannot represent")


# -- in range, including the exact bounds of each width ----------------------
accepts("int zero", lambda: n.take_int(0), 0)
accepts("int small", lambda: n.take_int(1234), 1234)
accepts("int negative", lambda: n.take_int(-1234), -1234)
accepts("int max", lambda: n.take_int(2**31 - 1), 2**31 - 1)
accepts("int min", lambda: n.take_int(-2**31), -2**31)
accepts("unsigned max", lambda: n.take_unsigned(2**32 - 1), 2**32 - 1)
accepts("short max", lambda: n.take_short(2**15 - 1), 2**15 - 1)
accepts("short min", lambda: n.take_short(-2**15), -2**15)
accepts("uchar max", lambda: n.take_uchar(255), 255)
accepts("int64 max", lambda: n.take_int64(2**63 - 1), 2**63 - 1)
accepts("int64 min", lambda: n.take_int64(-2**63), -2**63)
accepts("uint64 max", lambda: n.take_uint64(2**64 - 1), 2**64 - 1)

# One digit of CPython's representation holds 30 bits, so a value either side
# of that is where the fast path hands over to the general one.
accepts("one digit max", lambda: n.take_int64(2**30 - 1), 2**30 - 1)
accepts("two digits", lambda: n.take_int64(2**30), 2**30)
accepts("three digits", lambda: n.take_int64(2**61 + 7), 2**61 + 7)
accepts("nanosecond timestamp", lambda: n.take_int64(1759500000123456789), 1759500000123456789)
accepts("negative two digits", lambda: n.take_int64(-(2**30)), -(2**30))

# -- out of range -----------------------------------------------------------
rejects("int above max", lambda: n.take_int(2**31))
rejects("int below min", lambda: n.take_int(-2**31 - 1))
rejects("int astronomically large", lambda: n.take_int(2**200))
rejects("unsigned from negative", lambda: n.take_unsigned(-1))
rejects("unsigned above max", lambda: n.take_unsigned(2**32))
rejects("short above max", lambda: n.take_short(2**15))
rejects("uchar above max", lambda: n.take_uchar(256))
rejects("uchar from negative", lambda: n.take_uchar(-1))
rejects("int64 above max", lambda: n.take_int64(2**63))
rejects("uint64 above max", lambda: n.take_uint64(2**64))
rejects("uint64 from negative", lambda: n.take_uint64(-1))

# -- wrong type, not merely wrong magnitude ---------------------------------
rejects("int from float", lambda: n.take_int(1.5))
rejects("int from str", lambda: n.take_int("7"))
rejects("int from None", lambda: n.take_int(None))
rejects("int from list", lambda: n.take_int([1]))
rejects("double from str", lambda: n.take_double("1.5"))
rejects("double from None", lambda: n.take_double(None))


class MyInt(int):
    pass


accepts("int subclass", lambda: n.take_int(MyInt(99)), 99)
accepts("int subclass out of range is refused",
        lambda: isinstance(_try(lambda: n.take_int(MyInt(2**40))), TypeError), True)

# -- floats -----------------------------------------------------------------
accepts("double exact", lambda: n.take_double(1.5), 1.5)
accepts("double from int", lambda: n.take_double(3), 3.0)
accepts("double negative zero", lambda: n.take_double(-0.0), -0.0)
accepts("float exact", lambda: n.take_float(0.5), 0.5)
accepts("double inf", lambda: n.take_double(float("inf")), float("inf"))

# -- bool takes any int, which is the C++ rule, not the int rule ------------
accepts("bool True", lambda: n.take_bool(True), True)
accepts("bool False", lambda: n.take_bool(False), False)
accepts("bool from 5", lambda: n.take_bool(5), True)
accepts("bool from 0", lambda: n.take_bool(0), False)
rejects("bool from str", lambda: n.take_bool("yes"))

# -- containers take the bulk path, so they are checked separately ----------
accepts("int list", lambda: n.sum_ints([1, 2, 3, 4, 5]), 15)
accepts("int tuple", lambda: n.sum_ints((7, 8, 9)), 24)
accepts("int list empty", lambda: n.sum_ints([]), 0)
accepts("int list negative", lambda: n.sum_ints([-1, -2]), -3)
accepts("int64 list wide", lambda: n.sum_int64([2**40, 2**41]), 2**40 + 2**41)
accepts("double list", lambda: n.sum_doubles([0.5, 0.25]), 0.75)
accepts("double list from ints", lambda: n.sum_doubles([1, 2]), 3.0)
accepts("unsigned list", lambda: n.count([1, 2, 3]), 3)
rejects("int list with overflow", lambda: n.sum_ints([1, 2**40]))
rejects("int list with a str", lambda: n.sum_ints([1, "x"]))
rejects("int list with a float", lambda: n.sum_ints([1, 2.5]))
rejects("unsigned list with negative", lambda: n.count([1, -2]))
rejects("double list with a str", lambda: n.sum_doubles([1.0, "x"]))

# A conversion that fails partway must fail, not silently truncate.
accepts("overflow mid-list does not truncate",
        lambda: isinstance(_try(lambda: n.sum_ints([1, 2, 2**40, 4])), TypeError), True)

# -- fields go through the same conversion as arguments ---------------------
def set_get(attr, value):
    setattr(n, attr, value)
    return getattr(n, attr)


accepts("field int in range", lambda: set_get("i", 77), 77)
accepts("field uchar in range", lambda: set_get("uc", 200), 200)
accepts("field double", lambda: set_get("d", 2.5), 2.5)
rejects("field int out of range", lambda: set_get("i", 2**31))
rejects("field short out of range", lambda: set_get("sh", 2**20))
rejects("field uchar out of range", lambda: set_get("uc", 300))
rejects("field unsigned from negative", lambda: set_get("u", -1))

# -- a refusal must not leave an error behind -------------------------------
# This is the half that is invisible in normal use: an OverflowError raised by
# the conversion and never cleared stays on the thread and is reported by the
# next call to enter CPython, which may be in completely unrelated code.
for label, bad in (("huge int", lambda: n.take_int(2**200)),
                   ("negative unsigned", lambda: n.take_unsigned(-1)),
                   ("overflowing list", lambda: n.sum_ints([2**40]))):
    try:
        bad()
    except TypeError:
        pass
    except Exception as exc:
        failures.append(f"{label}: raised {type(exc).__name__}, expected TypeError")
    accepts(f"no error pending after {label}", lambda: n.take_int(5), 5)


if failures:
    print(f"FAIL ({len(failures)})")
    for f in failures:
        print("  " + f)
    sys.exit(1)
print("numeric conversion: all assertions passed")
