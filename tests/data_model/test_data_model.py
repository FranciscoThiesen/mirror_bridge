#!/usr/bin/env python3
"""Bound classes answer the Python data model: iteration, len(), `in`, and
pickling, each derived from the C++ type with no per-class glue.

Also pins the two things that must NOT change: a class whose size() is not an
element count keeps its truthiness and stays without len(), and a class that
cannot be reconstructed keeps refusing to pickle instead of losing a field.
"""

import copy
import gc
import os
import pickle
import sys

# build/tests is where CMake puts the module, and it goes last so that it is
# searched first: a stale build/<module>.so left by a CLI run would otherwise
# be the one imported, and the test would pass against the wrong artifact.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'build', 'tests'))

import data_model


def bag(*values):
    b = data_model.Bag()
    for v in values:
        b.add(v)
    return b


print("Test 1: begin()/end() drives iteration...")
b = bag(10, 20, 30)
assert list(b) == [10, 20, 30]
assert [x * 2 for x in b] == [20, 40, 60]
assert sum(b) == 60
assert tuple(b) == (10, 20, 30)
shelf = data_model.Shelf()
shelf.add("alpha")
shelf.add("beta")
assert list(shelf) == ["alpha", "beta"]
print("  ✓ list(), comprehension, sum() and a non-arithmetic element type")

print("Test 2: iteration works without size(), and re-iterating restarts...")
stream = data_model.Stream()
stream.first, stream.last = 3, 7
assert list(stream) == [3, 4, 5, 6]
assert list(stream) == [3, 4, 5, 6]
assert list(b) == [10, 20, 30]
print("  ✓ Stream (begin()/end() only) iterates, and iter() is not one-shot")

print("Test 3: a drained iterator stays drained...")
it = iter(bag(1, 2))
assert list(it) == [1, 2]
assert list(it) == []
assert next(it, "exhausted") == "exhausted"
print("  ✓ StopIteration is sticky")

print("Test 4: size() on a range gives len()...")
assert len(b) == 3
assert len(data_model.Bag()) == 0
assert len(shelf) == 2
print("  ✓ len() reads size()")

print("Test 5: `in` falls out of iteration...")
assert 20 in b
assert 99 not in b
assert "alpha" in shelf
assert "gamma" not in shelf
print("  ✓ membership searches the range")

print("Test 6: a class whose size() is not an element count is untouched...")
g = data_model.Gauge()
g.reading = 3.25
assert bool(g) is True, "truthiness of a non-range must not become size() != 0"
assert bool(data_model.Gauge()) is True
try:
    len(g)
    raise AssertionError("Gauge must not have acquired len()")
except TypeError:
    pass
try:
    iter(g)
    raise AssertionError("Gauge must not have acquired iteration")
except TypeError:
    pass
print("  ✓ no len(), no iteration, bool() still True")

print("Test 7: a range's truthiness is its emptiness...")
assert bool(bag(1)) is True
assert bool(data_model.Bag()) is False
assert not data_model.Bag()
print("  ✓ empty range is falsy, non-empty is truthy")

print("Test 8: pickling round-trips every reflected member...")
d = data_model.Doc()
d.title = "spec"
d.pages = 12
d.scores = [1.5, 2.5]
for protocol in range(pickle.HIGHEST_PROTOCOL + 1):
    back = pickle.loads(pickle.dumps(d, protocol))
    assert back.title == "spec", protocol
    assert back.pages == 12, protocol
    assert list(back.scores) == [1.5, 2.5], protocol
    assert back is not d
assert repr(pickle.loads(pickle.dumps(d))) == repr(d)
print("  ✓ identical through every pickle protocol")

print("Test 9: copy and deepcopy go through the same path...")
shallow = copy.copy(d)
deep = copy.deepcopy(d)
assert shallow.title == deep.title == "spec"
deep.pages = 99
assert d.pages == 12, "deepcopy must not alias the original"
print("  ✓ copy.copy and copy.deepcopy produce independent objects")

print("Test 10: pickling a container preserves its elements...")
restored = pickle.loads(pickle.dumps(bag(4, 5, 6)))
assert list(restored) == [4, 5, 6]
assert len(restored) == 3
print("  ✓ Bag survives a round trip")

print("Test 11: a class that cannot be reconstructed refuses to pickle...")
frozen = data_model.Frozen()
try:
    pickle.dumps(frozen)
    raise AssertionError("a class with a const member must not claim to pickle")
except (TypeError, AttributeError):
    pass
print("  ✓ const member means no __reduce__, so dumps() raises")

print("Test 12: the class is findable by module and qualified name...")
assert data_model.Doc.__module__ == "data_model", data_model.Doc.__module__
assert data_model.Doc.__name__ == "Doc"
assert data_model.Doc.__qualname__ == "Doc"
assert getattr(sys.modules["data_model"], data_model.Doc.__qualname__) is data_model.Doc
print("  ✓ __module__ is the module, __name__ stays unqualified")

print("Test 13: a Python subclass pickles as itself, attributes and all...")


class TaggedDoc(data_model.Doc):
    pass


sub = TaggedDoc()
sub.title = "sub"
sub.note = "python-side attribute"
# A class defined in a test module round-trips only if pickle can import it,
# which it can here because __main__ is importable by name.
back = pickle.loads(pickle.dumps(sub))
assert type(back) is TaggedDoc
assert back.title == "sub"
assert back.note == "python-side attribute"
print("  ✓ subclass type, C++ members and instance __dict__ all survive")

print("Test 14: an iterator keeps its container alive...")
assert data_model.Tracked.live() == 0


def make_tracked():
    t = data_model.Tracked()
    for i in range(1000):
        t.add(i)
    return t


# The only reference to the Tracked is the one the iterator took: without it
# the C++ object would be deleted here and the walk below would read freed
# memory.
walker = iter(make_tracked())
gc.collect()
assert data_model.Tracked.live() == 1, "the iterator must hold the container"
assert sum(walker) == sum(range(1000))
del walker
gc.collect()
assert data_model.Tracked.live() == 0, "both must be gone once the iterator is"
print("  ✓ container outlives its last user reference, and is freed after")

print("Test 15: mutating a container mid-iteration raises instead of reading freed memory...")
victim = bag(*range(4))
try:
    for _ in victim:
        victim.add(99)
    raise AssertionError("growing the container during iteration must be refused")
except RuntimeError as exc:
    assert "changed size during iteration" in str(exc), exc
shrinker = bag(*range(2000))
try:
    for _ in shrinker:
        shrinker.clear()
    raise AssertionError("clearing the container during iteration must be refused")
except RuntimeError:
    pass
print("  ✓ RuntimeError, the same guard CPython's dict and set iterators use")

print("Test 16: the generated stub says what the binding does...")
stubs = data_model.__mirror_bridge_stubs__()
assert "overload, Iterator\n" in stubs, "Iterator must be imported"
assert "def __iter__(self) -> Iterator[int]: ..." in stubs, stubs
assert "def __iter__(self) -> Iterator[str]: ..." in stubs, stubs
assert "def __len__(self) -> int: ..." in stubs, stubs
gauge_block = stubs.split("class Gauge:")[1].split("\nclass ")[0]
assert "__len__" not in gauge_block and "__iter__" not in gauge_block, gauge_block
print("  ✓ __iter__/__len__ in the .pyi for ranges only")


print("Test 17: a range that ends at a sentinel iterates...")
# end() returns Sentinel, not Cursor. Storing it as the iterator type used to
# be a hard compile error, so a normal C++20 range failed the module build.
countdown = data_model.Countdown()
assert list(countdown) == [3, 2, 1], list(countdown)
assert list(countdown) == [3, 2, 1]
assert 2 in countdown and 9 not in countdown
assert sum(countdown) == 6
assert not hasattr(data_model.Countdown, "__len__"), "no size(), so no len()"
print("  ✓ begin() and end() of different types")

print("Test 18: a proxy-reference element crosses as its value type...")
# Flags::Cursor dereferences to BitRef and declares value_type = bool, the
# shape std::vector<bool>'s iterator has. Converting the proxy itself walked
# its members, so every element arrived as {'value': 1} instead of a bool.
flags = data_model.Flags()
for bit in (True, False, True):
    flags.add(bit)
assert list(flags) == [True, False, True], list(flags)
assert all(isinstance(bit, bool) for bit in flags), list(flags)
assert len(flags) == 3
assert True in flags and False in flags
print("  ✓ the element is the iterator's value_type, not its reference")

print("Test 19: a class whose state cannot carry every member refuses to pickle...")
# Each of these would dump and load into an object that looked right in repr()
# while holding a default-constructed value for the member Python cannot see.
secretive = data_model.Secretive()
secretive.visible = 9
secretive.set_hidden(77)
try:
    pickle.dumps(secretive)
    raise AssertionError("a private member cannot be restored, so dumps must raise")
except (TypeError, AttributeError):
    pass
derived = data_model.Derived()
derived.own = 2
try:
    pickle.dumps(derived)
    raise AssertionError("a base class's members are not in the state, so dumps must raise")
except (TypeError, AttributeError):
    pass
assert "__reduce__" not in type(secretive).__dict__
assert "__reduce__" not in type(derived).__dict__
# And the classes that can round-trip still do.
assert pickle.loads(pickle.dumps(bag(4, 5))) is not None
assert list(pickle.loads(pickle.dumps(bag(4, 5)))) == [4, 5]
print("  ✓ a private member and an inherited member both refuse, like a const one")

print("Test 20: a size() that cannot be a Python length says so...")
# items.size() - 1 on an empty container is how an unsigned count underflows.
# Handed to CPython as a negative length it produces only
# "SystemError: returned NULL without setting an exception".
underflowed = data_model.Underflowed()
for expression in (lambda: len(underflowed), lambda: bool(underflowed),
                   lambda: list(underflowed)):
    try:
        expression()
        raise AssertionError("a size() past PY_SSIZE_T_MAX must not be reported as a length")
    except OverflowError as exc:
        assert "cannot be a Python length" in str(exc), exc
print("  ✓ OverflowError naming the class and the value, not SystemError")

print("Test 21: a reallocation that keeps size() identical is caught...")
# The element-count guard sees nothing here: reserve() and swapping in a fresh
# copy both leave size() alone while moving the buffer, which left the cursor
# and the end iterator pointing into a freed block.
for mutation in ("reallocate", "grow_capacity"):
    sneaky = data_model.Sneaky()
    for i in range(64):
        sneaky.add(i)
    seen = []
    try:
        for value in sneaky:
            seen.append(value)
            if len(seen) == 2:
                getattr(sneaky, mutation)()
        raise AssertionError(f"{mutation} moved the storage and was not caught: {seen[:8]}")
    except RuntimeError as exc:
        assert "reallocated its storage during iteration" in str(exc), exc
# An untouched range still iterates to the end.
intact = data_model.Sneaky()
for i in range(8):
    intact.add(i)
assert list(intact) == list(range(8))
print("  ✓ RuntimeError rather than a read of freed memory")

print("Test 22: operator== without a hash leaves the class unhashable...")
# CPython's inherit_slots copies tp_hash from the base only when
# tp_richcompare is inherited too, so a class with operator== ends up with
# __hash__ of None. That is self-consistent — a == b never coexists with
# hash(a) != hash(b) — but it is what takes the class out of a set.
first, second = data_model.Point(), data_model.Point()
first.x, first.y = 1, 2
second.x, second.y = 1, 2
assert first == second
assert data_model.Point.__hash__ is None, data_model.Point.__hash__
for unhashable_use in (lambda: hash(first), lambda: {first}, lambda: {first: 1}):
    try:
        unhashable_use()
        raise AssertionError("a class with operator== must not be hashable")
    except TypeError as exc:
        assert "unhashable" in str(exc), exc
assert first in [second], "== still finds it in a list"
# A class with no operator== keeps the identity hash.
assert data_model.Doc.__hash__ is not None
assert len({data_model.Doc(), data_model.Doc()}) == 2
print("  ✓ unhashable with operator==, identity-hashed without it")

print("Test 23: bool() of a range without size() is not emptiness...")
# Truthiness comes from the length slot, and the length slot needs size(), so
# an empty Stream is truthy while an empty Bag is falsy. Documented asymmetry.
empty_stream = data_model.Stream()
empty_stream.first = empty_stream.last = 0
assert list(empty_stream) == []
assert bool(empty_stream) is True, "no size() means no length slot, so still truthy"
assert bool(data_model.Bag()) is False
print("  ✓ the asymmetry the reference documents is the one that holds")

print("\nAll Python data model tests passed!")
