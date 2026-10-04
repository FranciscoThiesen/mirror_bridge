#!/usr/bin/env python3
"""A C++ exception becomes a Python exception, and constructor keywords bind.

Every path that can run user C++ has to catch. A method and a static method
already did; a free function and a constructor did not, and an exception from
either unwound past the interpreter and aborted the process with exit 134 —
no traceback, nothing catchable, the whole session gone.

The constructor half was quieter and worse. py_init looked only at the
positional tuple, so Cls(a=1, b=2) had nargs == 0, fell into the
default-construct branch and reported success. The caller got an object built
from none of their arguments and no indication of it.
"""
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "..", "..", "build"))
sys.path.append(os.path.join(os.path.dirname(__file__), "..", "..", "build_ci", "tests"))

import boundary_errors as be

failures = []


def expect(label, call, want):
    try:
        got = call()
    except Exception as exc:
        failures.append(f"{label}: raised {type(exc).__name__}({exc}), wanted {want!r}")
        return
    if got != want:
        failures.append(f"{label}: got {got!r}, wanted {want!r}")


def expect_raises(label, call, want_type):
    try:
        call()
    except want_type:
        return
    except Exception as exc:
        failures.append(f"{label}: raised {type(exc).__name__}, wanted {want_type.__name__}")
        return
    failures.append(f"{label}: did not raise, wanted {want_type.__name__}")


# -- a C++ throw reaches Python as an exception, from every path -------------
# The process surviving this file at all is the assertion that matters most.
t = be.Thrower()
expect_raises("method throws", lambda: t.method(-1), RuntimeError)
expect_raises("static method throws", lambda: be.Thrower.static_method(-1), Exception)
expect_raises("constructor throws", lambda: be.Thrower(-1), Exception)
expect_raises("free function throws", lambda: be.free_thrower(-1), RuntimeError)
expect_raises("non-std::exception throws", lambda: t.exotic(-1), RuntimeError)

# The happy path is unchanged by the try block around it.
expect("method returns", lambda: t.method(5), 5)
expect("static method returns", lambda: be.Thrower.static_method(5), 5)
expect("free function returns", lambda: be.free_thrower(5), 10)
expect("second free function", lambda: be.free_ok(5), 6)
expect("constructor accepts", lambda: be.Thrower(7).value, 7)

# A caught C++ exception must leave the interpreter usable, not just alive.
for _ in range(3):
    try:
        be.free_thrower(-1)
    except RuntimeError:
        pass
expect("usable after catching", lambda: be.free_thrower(21), 42)


# -- constructor arguments bind by position and by name ----------------------
expect("positional", lambda: (be.Point(1.0, 2.0).x, be.Point(1.0, 2.0).y), (1.0, 2.0))
expect("all keyword",
       lambda: (be.Point(x_in=1.0, y_in=2.0).x, be.Point(x_in=1.0, y_in=2.0).y), (1.0, 2.0))
expect("mixed", lambda: (be.Point(1.0, y_in=2.0).x, be.Point(1.0, y_in=2.0).y), (1.0, 2.0))
expect("keyword out of order",
       lambda: (be.Point(y_in=2.0, x_in=1.0).x, be.Point(y_in=2.0, x_in=1.0).y), (1.0, 2.0))

# Overload selection still works, and works through keywords.
expect("three-arg overload positional", lambda: be.Point(1.0, 2.0, 3.0).z, 3.0)
expect("three-arg overload keyword", lambda: be.Point(x_in=1.0, y_in=2.0, z_in=3.0).z, 3.0)
expect("two-arg overload leaves z zero", lambda: be.Point(1.0, 2.0).z, 0.0)

# The default constructor is the no-argument case, and only that case.
expect("default constructor", lambda: be.Point().x, -1.0)
expect("default-only class", lambda: be.DefaultOnly().tag, 42)

# -- what must be refused ----------------------------------------------------
# Each of these used to be accepted, silently producing a default object.
expect_raises("unknown keyword", lambda: be.Point(bogus=1.0), TypeError)
expect_raises("keyword duplicates positional", lambda: be.Point(1.0, x_in=2.0), TypeError)
expect_raises("too few arguments", lambda: be.Point(1.0), TypeError)
expect_raises("too many arguments", lambda: be.Point(1.0, 2.0, 3.0, 4.0), TypeError)
expect_raises("keyword on a class with no such parameter",
              lambda: be.DefaultOnly(tag=1), TypeError)
expect_raises("positional on a default-only class", lambda: be.DefaultOnly(1), TypeError)
expect_raises("wrong keyword type", lambda: be.Point(x_in="a", y_in="b"), TypeError)

if failures:
    print(f"FAIL ({len(failures)})")
    for f in failures:
        print("  " + f)
    sys.exit(1)
print("boundary errors: all assertions passed")
