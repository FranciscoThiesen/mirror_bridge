# Returning a vector of structs without building a list

A method returning `std::vector<Point3>` hands Python a list of `Point3`
objects. That is one Python object and one `new Point3` per element. The bytes
were already contiguous and, for a struct whose members are all arithmetic,
they were already a complete description of the value, so almost all of that
work exists only to be undone by whoever wanted an array.

Measured, returning a `std::vector<Point3>` of n elements:

| n | list of objects | columnar view | |
|---|---|---|---|
| 10 | 833 ns | 40 ns | 21x |
| 100 | 9.0 µs | 0.07 µs | 122x |
| 1,000 | 87 µs | 0.36 µs | 243x |
| 10,000 | 878 µs | 4.6 µs | 192x |
| 100,000 | 9.0 ms | 45 µs | 201x |

The per-element cost is flat at 0.4 to 0.7 ns above n = 100, because what is
left is one `memcpy`. The list path is 83 to 90 ns per element at every size,
because what it does is per element.

And for the thing most callers actually wanted, at n = 10,000:

```python
# today
pts = cloud.get_points()                              #  878 µs
arr = np.array([(p.x, p.y, p.z) for p in pts])        # 2351 µs
                                                      # 3229 µs total

# with --columnar
pts = cloud.get_points()                              #    4.6 µs
arr = np.asarray(pts)                                 #    1.4 µs, no copy
                                                      #    6 µs total, 540x
```

## Turning it on

```bash
mirror_bridge generate src --module hot --lang python --columnar
```

or, compiling a binding by hand, `-DMIRROR_BRIDGE_COLUMNAR_VECTORS`.

It is off by default because it changes what a method returns, and that is a
decision for whoever consumes the module rather than for whoever wrote the
header. Nothing in the C++ needs editing either way, which is the point: the
team that owns the headers does not have to know.

## What you get back

A sequence that also describes its own memory:

```python
pts = cloud.get_points()

len(pts)          # as before
pts[0].x          # a Point3, as before
pts[-1].z         # as before
for p in pts: ... # as before
pts.tolist()      # exactly the list this used to be
np.asarray(pts)   # zero copy, dtype [('x','<f8'),('y','<f8'),('z','<f8')]
```

`pts[i]` copies one element out on access. So did the list: mutating a list
element never reached the C++ side, and it still does not.

The view owns the elements. A method returning `std::vector<Point3>` by value
has already made a copy, and the view moves that copy in rather than copying
again, so the memory is held until the view is collected but nothing is copied
twice. Getting this wrong cost 6x: an earlier version took the vector by value
into the conversion, which copied it a second time, and that second copy was
the whole of the per-element cost above n = 1000. An array built from it keeps
the view alive -- numpy sets `base` to the view -- so this is safe:

```python
arr = np.asarray(cloud.get_points())   # nothing holds the view, and that is fine
arr["x"].sum()                         # the array is its only owner
```

`np.asarray` gives a read-only array, because the buffer belongs to a C++
object. `np.asarray(pts).copy()` if you need to write.

Slicing, `match`, `copy`, `reversed`, `in`, `index`, `count` and
`isinstance(pts, collections.abc.Sequence)` all work too -- the type is
registered with the `Sequence` ABC, which is also what makes a structural
pattern match instead of silently falling through to the next case.

What breaks is code that requires the result to *be* a list:
`isinstance(pts, list)`, `pts + [x]`, `pts.append(...)`, `json.dumps(pts)`.
`pts.tolist()` is the way back, and it costs what the list used to cost
(864 µs at n = 10,000) only when something asks for it.

One quieter difference: indexing returns a fresh copy each time, so
`pts[0] is pts[0]` is `False` where a list gives `True`, and `pts[1] in pts`
is `False` unless the element type defines `__eq__`. A list held one object
per element and handed back the same one; the view has no object until you
ask for one. Pickling fails on either, because a bound class is not picklable.

## Which structs qualify

The conversion only changes for a struct that is honestly described by its own
bytes. Everything else returns a list exactly as before, so turning the flag
on cannot produce a wrong answer for a type it does not handle. The test is

- trivially copyable, which rules out virtual functions, user-defined copy and
  move, and anything whose value is not simply its bytes
- standard layout, which is what makes a member's offset meaningful to a
  reader outside the program
- at least one member declared on the struct itself and visible from outside
  it, all of them named, none of them bit-fields
- every member an arithmetic type numpy can name

"Declared on the struct itself" is literal: reflection reports a type's direct
members, so a struct that inherits its data from a base reports none and
returns a list. So does a `class` whose members are all private. Both are
safe -- mixed access control is not standard layout, so there is no way to get
a description of half a struct -- but neither gets the fast path.

So `Point3 { double x, y, z; }` qualifies, and these do not: a struct with a
`std::string` or a pointer member, a bit-field, an empty struct, a struct
containing another struct, anything polymorphic. A nested struct would need a
nested dtype, which is a later job rather than a wrong answer now.

Padding is included rather than assumed away:

```cpp
struct Tick { double price; std::int32_t size; char side; };  // 13 used, 16 stored
```

```python
>>> np.asarray(ticks).dtype
dtype([('price', '<f8'), ('size', '<i4'), ('side', 'u1'), (' pad13', 'V3')])
>>> np.asarray(ticks).dtype.itemsize
16
```

The padding shows up as a field. It has to: `descr` has no way to say "skip
three bytes" other than an entry that occupies them, and without it numpy
computes an itemsize of 13. So `dtype.names` carries an entry you did not
declare, holding whatever the compiler left in the padding, which is
uninitialised memory. `pd.DataFrame(arr)` will show it as a column and
`np.savetxt` will refuse it. `np.lib.recfunctions.repack_fields(arr)` drops
the padding, at the cost of a copy. The pad names begin with a space so they
cannot collide with a member -- numpy names an unnamed entry `f<index>`, and a
struct with a member called `f1` then fails with "field 'f1' occurs more than
once".

That matters more than it looks. A descriptor that adds its members up and
forgets the three bytes of trailing padding reports 13, and then every element
after the first is read three bytes early. PEP 3118 format strings — what the
buffer protocol speaks — cannot express trailing padding at all, which is why
this goes through numpy's array interface instead.

A `char` member comes back as an integer column, because a `char` member
already comes back as an integer through the ordinary path (`t.side` is `66`,
not `b'B'`). The column agrees with the scalar rather than with intuition
about character types.

## It applies to members too

A `std::vector<Point3>` read as an attribute goes through the same conversion,
so it also comes back as a view:

```python
np.asarray(cloud.points)   # the member, not the method
```

That one is worth a second look. The array reports `owndata = False`, which
reads like a window onto the live C++ vector, and it is not: the conversion
copies, so the array is a snapshot taken when you read the attribute. Change
the C++ object afterwards and the array still shows the old values. A view
onto the live member would need borrowed lifetimes, which is a different
piece of work.

## Where the description comes from

```cpp
std::meta::offset_of(member).bytes   // where each member starts
std::meta::size_of(^^S)              // how big the struct is, padding included
std::meta::identifier_of(member)     // what each member is called
```

That is the whole of `__array_interface__`. Both supported compilers agree
with the language about all three — `tests/columnar/` checks every field's
offset against what the compiler chose, for four struct shapes including two
with internal and trailing padding.

This is the part that needs reflection. pybind11 wants
`PYBIND11_NUMPY_DTYPE(Point3, x, y, z)` written out for every struct and kept
in step with the header by hand; nanobind's `ndarray` has no record dtype at
all. Here the header is the only source, so a member added in C++ appears as a
column with nothing to update.

`__array_interface__` is a plain dict, so no numpy is imported, linked or
required to build a module. It is needed only by whoever wants an array out of
one.

## Related

- [Type conversion reference](../reference/type-conversion.md)
- [Replacing a hot Python function with C++](accelerating-python.md)
- [Zero-copy buffers](../ZERO_COPY_BUFFERS.md) — the same idea for a vector of
  plain numbers, where there are no columns to name
