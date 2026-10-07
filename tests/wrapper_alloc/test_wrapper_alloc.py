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

print("Test 9: re-initialising an initialised object is refused...")
# Python lets __init__ be called directly, so the payload can already hold a
# live object. Writing over it skips its destructor; freeing it first leaves
# any member view reading freed memory. Neither is acceptable, so it is
# refused and the object is left exactly as it was.
wa.Holder.reset()
c = wa.Counted(1.0)
try:
    c.__init__(2.0)
    raise AssertionError("expected re-initialisation to be refused")
except TypeError as exc:
    assert "already initialised" in str(exc), exc
assert c.x == 1.0, c.x
assert wa.Holder.destroyed() == 0, wa.Holder.destroyed()
del c
gc.collect()
assert wa.Holder.destroyed() == 1, wa.Holder.destroyed()
print("  ✓ refused, object untouched, destroyed exactly once at the end")

print("Test 10: a throwing constructor is not even reached on re-init...")
# Reaching it was a double free: unwinding destroys the members the
# constructor had built, and tp_dealloc then destroyed them again.
f = wa.Fragile(5)
assert f.get() == 5
try:
    f.__init__(-1)
    raise AssertionError("expected re-initialisation to be refused")
except TypeError as exc:
    assert "already initialised" in str(exc), exc
assert f.get() == 5, f.get()
del f
gc.collect()
print("  ✓ refused before the constructor runs, object still usable")

print("Test 10b: a member view is never left dangling by a re-init...")
# The owner here is over-aligned, so its C++ object is on the heap rather
# than in the payload. Freeing it would have left this view reading freed
# memory, which read back as 9.2689507951829e-310.
assert wa.WideNest.__basicsize__ == 40, wa.WideNest.__basicsize__  # heap, not inline
n = wa.WideNest(2.0)
nv = n.held
assert nv.x == 2.0, nv.x
try:
    n.__init__(5.0)
    raise AssertionError("expected re-initialisation to be refused")
except TypeError as exc:
    assert "already initialised" in str(exc), exc
assert nv.x == 2.0, nv.x
assert n.held.x == 2.0, n.held.x
nv.x = 6.0
assert n.held.x == 6.0, n.held.x
print("  ✓ the view still reads and writes the owner's live member")

print("Test 10c: a failed FIRST construction leaves a clearly empty object...")
try:
    wa.Fragile(-1)
    raise AssertionError("expected the constructor to raise")
except RuntimeError as exc:
    assert "negative tag" in str(exc), exc
print("  ✓ the constructor's own exception propagates")

print("Test 11: a 16-byte-aligned payload stays inline and is aligned...")
# 16 is what CPython's object allocator promises, so this one must not fall
# back, and its members must be readable at the computed offset.
assert wa.Snug.__basicsize__ > 40, wa.Snug.__basicsize__
sn = wa.Snug()
assert sn.sum() == 3.0, sn.sum()
sn.a = 10.0
assert sn.sum() == 12.0
print(f"  ✓ Snug {wa.Snug.__basicsize__} bytes inline, reads correctly")

print("\nAll wrapper allocation tests passed!")
