#!/usr/bin/env python3
"""An awkward class or parameter must cost itself, not the module.

Every shape here came from a real library in the compatibility corpus, where
one of them took every other class down with it:

  Undestroyable  tinyxml2::XMLElement -- private destructor, so no wrapper
                 can free it. Cost the module all 15 classes.
  Unfillable     Json::Value -- looks like a container (value_type, begin(),
                 size()) and nothing can be put into it. A static_assert deep
                 in the conversion layer cost jsoncpp all 23 classes, for one
                 method parameter.
  NoDefaultBag   cxxopts::KeyValue -- a container element with no default
                 constructor, which the fill loop cannot build.
"""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))

import declinable as d

print("Test 1: the module exists at all...")
# This is the whole point. Before, any one of the shapes above failed the
# build and there was no module to import.
assert hasattr(d, "Plain"), dir(d)
print("  ✓ imported")

print("Test 2: an ordinary class is unaffected...")
p = d.Plain()
assert p.twice() == 2.0, p.twice()
p.x = 3.0
assert p.twice() == 6.0
print("  ✓ Plain binds and works")

print("Test 3: a class whose destructor is inaccessible is skipped...")
# Skipped, not bound-and-broken: a wrapper that cannot free its object would
# leak or crash on collection.
assert not hasattr(d, "Undestroyable"), "Undestroyable should have been declined"
print("  ✓ declined, and the module still built")

print("Test 4: a class keeps the methods that do convert...")
c = d.Consumer()
assert c.usable(4.0) == 8.0, c.usable(4.0)
print("  ✓ Consumer.usable works alongside two unconvertible methods")

print("Test 5: the unconvertible methods refuse at the call, not the build...")
# Absent or present-and-raising are both acceptable outcomes; what is not
# acceptable is the module failing to build. Which one happens is pinned by
# Test 6, where the module says so out loud.
for name in ("takes_unfillable", "takes_nodefault"):
    fn = getattr(c, name, None)
    if fn is None:
        print(f"  ✓ {name} declined at bind time")
        continue
    try:
        fn([1, 2, 3])
        raise AssertionError(f"{name} accepted a list it cannot convert")
    except TypeError:
        print(f"  ✓ {name} raises TypeError on a value it cannot convert")

print("Test 6: a void* parameter is declined, the class keeps its other methods...")
# box2d's b2BlockAllocator::Free(void*, int). void* was waved through the
# parameter gate as an "opaque handle" with no conversion behind it.
a = d.Allocator()
assert a.usable(5) == 5, a.usable(5)
rel = getattr(a, "release", None)
if rel is None:
    print("  ✓ release(void*, int) declined at bind time")
else:
    try:
        rel(0, 1)
        raise AssertionError("release accepted a value it cannot convert")
    except TypeError:
        print("  ✓ release raises rather than failing the build")

print("Test 7: a private nested type does not cost the class that holds it...")
# The planner reaches Pool<16>::Block through signature closure and cannot
# name it in the generated binding. That must not take UsesPool with it.
u = d.UsesPool()
assert u.cap() == 16, u.cap()
print("  ✓ UsesPool binds with a private nested type in its reach")

print("Test 8: a span parameter costs itself and nothing else...")
# Indexing a std::span<const double> compiles and assigning through it does
# not, so a guard that asked only whether the element was reachable
# instantiated the assignment and failed the build inside the conversion
# layer. The module is the evidence: it exists, and SpanTaker kept the method
# that does convert.
st = d.SpanTaker()
assert st.usable(2.0) == 3.0, st.usable(2.0)
total = getattr(st, "total", None)
if total is None:
    print("  ✓ the span method was declined at bind time")
else:
    try:
        total([1.0, 2.0, 3.0])
        raise AssertionError("total accepted a span it cannot fill")
    except TypeError:
        print("  ✓ total raises TypeError rather than failing the build")

print("Test 9: a declined method says so, when asked...")
# A declined class and a declined free function were each refused by name on
# stderr. A declined method was refused in silence: not on stderr, not in
# --json, not in the stub, and dir() just does not list it, so a caller could
# not tell "I wrote the C++ wrong" from "that method does not exist".
#
# Behind an environment variable because this runs at import and a large
# library declines hundreds of methods. The CLI sets it for its own
# verification import, which is how the names reach the unbound report and
# the --json `declined` list.
import os
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))
build = os.path.join(here, "..", "..", "build")
code = "import sys; sys.path.insert(0, %r); import declinable" % build

quiet = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True,
                       env={k: v for k, v in os.environ.items()
                            if k != "MIRROR_BRIDGE_REPORT_DECLINED"})
assert "skipped method" not in quiet.stderr, \
    "a plain import should not wear the declined-method list:\n" + quiet.stderr

loud = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True,
                      env=dict(os.environ, MIRROR_BRIDGE_REPORT_DECLINED="1"))
assert "skipped method" in loud.stderr, \
    "MIRROR_BRIDGE_REPORT_DECLINED=1 reported no declined method:\n" + loud.stderr

# Consumer's two unconvertible methods are the ones this fixture exists for.
for name in ("takes_unfillable", "takes_nodefault"):
    if getattr(d.Consumer(), name, None) is None:
        assert "Consumer::%s" % name in loud.stderr, \
            "%s was declined and not named:\n%s" % (name, loud.stderr)
        assert "no conversion" in loud.stderr, \
            "named the method and not the reason:\n" + loud.stderr
print("  ✓ quiet by default, and names the method and the reason when asked")

print("\nAll declinable-shape tests passed!")
