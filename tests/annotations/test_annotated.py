#!/usr/bin/env python3
"""[[=exclude{}]] keeps a field out of the module and [[=readonly{}]] keeps it
unwritable, on every compiler the project supports.

This had no runtime test. tests/test_new_features/test_annotations_binding.cpp
checked that a binding with annotations compiles, and nothing checked what the
annotations did, so on GCC -- where the annotation lookup is spelled
annotations_of_with_type rather than annotation_of_type, and the header
answered "no P3394 support, cannot check annotations" -- every annotation was
ignored. A field marked [[=exclude{}]] was exported to Python under its own
name, and a field marked [[=readonly{}]] was writable. The project's own test
fixture for this is a password hash.
"""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))

import annotated

p = annotated.UserProfile()

print("Test 1: an excluded field is not an attribute...")
for hidden in ("password_hash", "internal_flag"):
    assert not hasattr(p, hidden), f"{hidden} is [[=exclude{{}}]] and reached Python"
print("  ✓ password_hash and internal_flag are absent")

print("Test 2: exclusion hides the field, it does not drop the data...")
p.set_password("not-a-real-hash")
assert p.peek_password() == "not-a-real-hash", "the C++ side lost the excluded field"
assert not hasattr(p, "password_hash")
print("  ✓ the field still exists in C++")

print("Test 3: setting an excluded name does not quietly succeed...")
try:
    p.password_hash = "overwritten"
except AttributeError:
    pass
else:
    raise AssertionError(
        "assigning to an excluded field was accepted; it either bound after "
        "all or the type accepts arbitrary attributes"
    )
assert p.peek_password() == "not-a-real-hash", "the excluded field was written"
print("  ✓ AttributeError, and the C++ value is untouched")

print("Test 4: the unannotated fields are bound normally...")
p.user_id = 7
p.username = "fran"
assert (p.user_id, p.username) == (7, "fran")
print("  ✓ user_id and username read and write")

print("Test 5: a readonly field reads...")
p.stamp("2026-10-15T00:00:00Z")
assert p.created_at == "2026-10-15T00:00:00Z", p.created_at
assert p.version == 1, p.version
print("  ✓ created_at and version are readable")

print("Test 6: a readonly field refuses to be written...")
for name, value in (("created_at", "1970-01-01T00:00:00Z"), ("version", 99)):
    try:
        setattr(p, name, value)
    except AttributeError:
        pass
    else:
        raise AssertionError(f"{name} is [[=readonly{{}}]] and accepted a write")
assert p.created_at == "2026-10-15T00:00:00Z", "created_at was written through"
assert p.version == 1, "version was written through"
print("  ✓ both refuse, and both keep their value")

print("Test 7: readonly tracks C++, it is not a frozen copy...")
p.bump_version()
assert p.version == 2, f"the getter returned a stale {p.version}"
print("  ✓ the getter reads the live field")

print("Test 8: the module exposes exactly the expected names...")
# Asserted as sets rather than counted: a count passes for the wrong reason as
# soon as one name is swapped for another, and says nothing about which.
assert {n for n in dir(p) if not n.startswith("_")} == {
    "user_id", "username", "created_at", "version",
    "set_password", "peek_password", "stamp", "bump_version",
}, sorted(n for n in dir(p) if not n.startswith("_"))
plain = annotated.PlainProfile()
assert {n for n in dir(plain) if not n.startswith("_")} == {"user_id", "username"}, \
    sorted(n for n in dir(plain) if not n.startswith("_"))
print("  ✓ four of six fields on UserProfile, two of two on PlainProfile")

print("\n✅ annotations behave the same on every supported compiler")
