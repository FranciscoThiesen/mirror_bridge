#!/usr/bin/env python3
"""The C++ object lives inside the Python object, so there is one allocation.

Placement new has to be paired with an explicit destructor call and never with
delete, and the payload must not be used for a type the wrapper was not sized
for. These are the two ways this goes wrong silently, so both are counted.

Run this under PYTHONMALLOC=debug to get guard bytes around every wrapper.
"""

import gc
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))

import wrapper_alloc as wa

print("Test 1: the payload really is inline...")
# PyObject_HEAD + T* + bool + PyObject* is 40 bytes; an inline double pushes
# the type past that, while an over-aligned member must not.
assert wa.Counted.__basicsize__ > 40, wa.Counted.__basicsize__
assert wa.Wide.__basicsize__ == 40, wa.Wide.__basicsize__
print(f"  ✓ Counted {wa.Counted.__basicsize__} bytes inline, "
      f"Wide {wa.Wide.__basicsize__} bytes stays out of line")

print("Test 2: an over-aligned type still works...")
w = wa.Wide()
assert w.first() == 0.0
del w
print("  ✓ alignas(64) member constructs and reads")

print("Test 3: every construction is destroyed exactly once...")
wa.Holder.reset()
before_live = wa.Holder.live()
objs = [wa.Counted() for _ in range(500)]
assert wa.Holder.live() == before_live + 500, wa.Holder.live()
del objs
gc.collect()
assert wa.Holder.live() == before_live, wa.Holder.live()
assert wa.Holder.destroyed() == 500, wa.Holder.destroyed()
print("  ✓ 500 constructed, 500 destroyed, live count back to start")

print("Test 4: a returned object is destroyed exactly once too...")
h = wa.Holder()
wa.Holder.reset()
base = wa.Holder.live()
for i in range(500):
    c = h.make(float(i))
    assert c.x == float(i)
    del c
gc.collect()
# make() builds a C++ temporary and the wrapper copy-constructs from it, so
# the useful invariant is the balance, not the count: a live count back at
# the baseline means nothing leaked and nothing was destroyed twice.
assert wa.Holder.live() == base, wa.Holder.live()
assert wa.Holder.destroyed() == 1000, wa.Holder.destroyed()
print("  ✓ 500 returns, every construction balanced by one destruction")

print("Test 5: a parameterised constructor lands in the inline slot...")
wa.Holder.reset()
c = wa.Counted(4.5)
assert c.x == 4.5
del c
gc.collect()
assert wa.Holder.destroyed() == 1, wa.Holder.destroyed()
print("  ✓ Counted(4.5) constructs and destroys once")

print("Test 6: a member view does not destroy the parent's member...")
# A view borrows storage it does not own, so dealloc must leave it alone.
h2 = wa.Holder()
wa.Holder.reset()
v = h2.c
v.x = 2.0
del v
gc.collect()
assert wa.Holder.destroyed() == 0, wa.Holder.destroyed()
assert h2.c.x == 2.0
print("  ✓ dropping a view destroys nothing")

print("Test 7: a Python subclass still works...")
class Sub(wa.Counted):
    def doubled(self):
        return self.x * 2

s = Sub()
s.x = 3.0
assert s.doubled() == 6.0
s.extra = "dict works"
assert s.extra == "dict works"
del s
gc.collect()
print("  ✓ subclass construction, attributes and __dict__")

print("Test 8: identity and values survive a round trip...")
h3 = wa.Holder()
h3.c.x = 7.25
assert h3.c.x == 7.25
assert h3.label == "holder"
print("  ✓ inline payload and member views coexist")

print("\nAll wrapper allocation tests passed!")
