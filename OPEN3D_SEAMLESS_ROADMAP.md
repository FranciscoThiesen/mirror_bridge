# Roadmap to "seamless Open3D with mirror_bridge"

Honest assessment of what the `bind_class<PointCloud>(m, "PointCloud")`
promise still needs to deliver — and how other binding libraries solve
each piece when you can't rely purely on C++26 reflection.

---

## Where we are today

Already done, runtime-verified, in main:

- ✅ Compile — all 38 Open3D geometry classes bind from a single
  `bind_class<T>` call each. Abstract bases, non-copyable classes,
  nested classes, C-arrays, smart-ptr-to-abstract, vector<bool>, Eigen.
- ✅ Inheritance — P2996 `bases_of` walk exposes inherited methods
  with C++ name hiding. Virtual overrides dispatch correctly.
- ✅ Trampolines — both explicit (`TrampolineBase<T>`) for portability
  and auto-vtable-swap (`bind_class_auto<T>`) for zero-glue magic on
  Itanium ABI.
- ✅ Nested class auto-discovery — `HalfEdgeTriangleMesh::HalfEdge`
  and friends get qualified by the CLI tool's brace-depth parser.
- ✅ Keyword arguments + default parameter values — reflection
  (`identifier_of`, `has_default_argument`) + splice-call with fewer
  args than declared lets C++ fill defaults at the call site.
- ✅ Build parity — libOpen3D.so compiles cleanly with clang-p2996 +
  libc++ via Open3D's own CMake (one sed-patch to fmt/format-inl.h
  for a FMT_STRING consteval issue).

---

## What "seamless" still needs

Ordered by impact on a real Open3D user's first-five-minutes experience.

### 1. Operator overloading (HIGH, doable)

**Why:** `pcd1 + pcd2` merges point clouds, `obb.volume() * scale`,
`aabb == other`. Current mirror_bridge silently skips `operator+`,
`operator==` etc. because `is_operator_function(m)` filters them out.

**Strategy:** reflection has `operator_of(info) -> operators` which
returns the kind. For each supported operator, generate a Python slot
handler:

| C++ operator    | Python slot               |
|-----------------|---------------------------|
| `operator+`     | `nb_add` / `__add__`      |
| `operator==`    | `tp_richcompare` + Py_EQ  |
| `operator[]`    | `mp_subscript`            |
| `operator()`    | `tp_call`                 |

Implementation: extend `MemberFunctionCache` to also harvest operator
infos. For each, emit a PyMethodDef or fill a tp_as_number / tp_as_sequence
slot. pybind11 does this with `.def(py::self + py::self)`; we can do it
automatically.

**Effort:** ~300 lines, one session.

### 2. __repr__ from members (HIGH, trivial)

**Why:** `print(pcd)` currently shows `<PointCloud object at 0xffff...>`.
Open3D users expect `"PointCloud with 1234 points."`.

**Strategy:** two paths.
- If the class declares `operator<<(std::ostream&, T)`, detect it via
  reflection and call it into a stringstream for `__repr__`.
- Otherwise, generate a repr from member names + values:
  `"PointCloud(points=[...], colors=[...], ...)"`. Reflection already
  has member names and to_python conversion — just format them.

**Effort:** ~100 lines.

### 3. Operator-like Python protocols (MEDIUM, doable)

**Why:** `len(pcd)`, `pcd[i]`, `for p in pcd`. Python users reach for
these constantly.

**Strategy:** detect idiomatic C++ shapes via reflection and map them:
- `size() -> size_t` (or `std::size_t size()`) → `__len__`.
- `operator[](size_t)` → `__getitem__` + `__iter__` (synthesized).
- `begin()` / `end()` returning iterators → `__iter__`.

For classes like `PointCloud` that have a `points_` vector member, also
synthesize iteration over that member as an opt-in.

**Effort:** ~200 lines plus tests.

### 4. Docstrings (MEDIUM, limited by P2996)

**Why:** `help(pcd.voxel_down_sample)` should explain what the method
does. Open3D's pybind layer pipes through their own docstring database.

**Limitation:** P2996R13 doesn't expose comments or doc-comments. We
can't read `///` or `/** */` content via reflection.

**Strategy (2 options):**
- **Generator-side:** the `mirror_bridge generate` CLI parses headers
  and extracts doxygen blocks, emits a `doc_table[] = { {"method_name",
  "doc text"}, … }` alongside each class. Runtime code reads this
  table and fills `PyMethodDef::ml_doc`. Borrows from pybind11's
  `docstring_generation.py` approach.
- **Annotations:** use C++26 `[[=mirror_bridge::doc("…")]]` attributes
  on methods, read via P3394 annotation reflection. Works inline.

**Effort:** generator-side is 1-2 days; annotations-based is cleaner
but requires a convention.

### 5. Free functions and module-level APIs (MEDIUM, partially done)

**Why:** Open3D's public API has dozens of free functions —
`open3d.io.read_point_cloud(path)`, `open3d.geometry.create_sphere()`.
mirror_bridge has some free-function discovery in `mirror_bridge_auto`
but it's not end-to-end verified for Open3D's patterns.

**Strategy:** extend the existing `discover_functions` logic in
`scripts/discover_symbols.py` to emit `m.def("read_point_cloud", &io::ReadPointCloud)`
entries. Reuse the same dispatcher machinery (kwargs, defaults, overload
resolution) we just built for methods.

**Effort:** ~200 lines. Mostly wiring.

### 6. Enum types (MEDIUM, likely works already)

**Why:** Open3D uses enums like `SimplificationContraction::Average`,
`KDTreeSearchParam::SearchType::Knn`. Python expects them as class
attributes: `o3d.geometry.SimplificationContraction.Average`.

**Status:** `enumerators_of` is in P2996. We should auto-generate a
Python class with members for each enumerator. Need to verify current
state — may already partially work for top-level enums.

**Effort:** ~100 lines.

### 7. Polymorphic return types (HIGH, subtle)

**Why:** `pcd.cluster_dbscan()` returns `std::vector<int>`. Works today.
But `geometry.io.read_triangle_mesh()` returns `std::shared_ptr<Geometry>`
— the runtime type is `TriangleMesh`. Python should receive a wrapper
of the actual derived class, not the base.

**Strategy:** in `to_python<shared_ptr<Base>>`, use `typeid(*ptr).name()`
at runtime to look up the registered derived type. Create a wrapper of
that type instead of Base. pybind11 solves this with holder types and
typeid-keyed lookup. We already have the Python-based type registry
(`get_python_type_registry`); extend `to_python<SmartPointer T>` to
use `typeid(*T)` instead of `typeid(Base)`.

**Effort:** ~150 lines.

### 8. Exception mapping (MEDIUM)

**Why:** Open3D throws `std::runtime_error` with structured messages
for `read_point_cloud` failures, `out_of_range` for invalid indices,
etc. We currently catch all and raise `RuntimeError`. Python users
expect `IOError`, `IndexError`, etc. for their normal code paths.

**Strategy:** wrap the try/catch with a ladder of `catch(const std::X&)`
for common std exceptions, mapping each to the appropriate Python
exception class. Let user classes register custom mappings.

**Effort:** ~80 lines.

### 9. Python type stubs (.pyi) (MEDIUM)

**Why:** IDE autocomplete, mypy/pyright. Without stubs, users can't
introspect the API surface statically.

**Status:** `scripts/generate_stubs.py` exists. Need to verify it
matches current binding output (parameter names, defaults, types).

**Effort:** verification + polish, ~1 day.

### 10. Runtime Open3D linking (BLOCKED, not mirror_bridge)

**Why:** our 38-class binding compiles cleanly but loading fails on
`undefined symbol: X509_INFO_free` — Open3D's own CMake link recipe
lists BoringSSL archives without `-Wl,--whole-archive`, and
`-fvisibility=hidden` combined with the `libOpen3D.map` version script
drops the SSL symbols from the final .so.

**Strategy:** re-link libOpen3D.so with `-Wl,--whole-archive` around
the BoringSSL archives, or patch Open3D's CMakeLists.txt upstream.
This is Open3D build-system work, not mirror_bridge.

**Effort:** N/A — upstream fix.

### 11. Lifecycle for Python-held C++ objects (MEDIUM, advanced)

**Why:** when Python stores a wrapper whose C++ object is held by
another C++ object (e.g., `aabb.GetCenter()` returning `Eigen::Vector3d&`
into a wrapper that the C++ parent still owns), reference counting
gets tricky. pybind11 uses `py::return_value_policy` to parameterize
this explicitly.

**Strategy:** detect return-by-reference vs return-by-value vs
return-by-shared_ptr via reflection. For reference returns, either
copy eagerly (safe, default) or add a `[[=hold_parent]]` annotation.

**Effort:** ~200 lines, careful design.

---

## What pybind11 / nanobind do that we could borrow

- **pybind11** builds trampolines per-class via `PYBIND11_OVERRIDE`
  macros, uses shared_ptr holders for polymorphic lifetime, supports
  `.def(py::init<...>)` with keyword+default, and has a rich type-caster
  system for custom types. Their docstring support comes from
  user-supplied strings in `.def()`.
- **nanobind** drops shared_ptr holders in favor of intrusive pointers,
  generates less code per binding, supports the same API as pybind11
  but smaller/faster. They use `nb::arg("name") = default` exactly the
  same way.
- **scikit-build-core** standardizes building Python extensions with
  CMake. For distribution.

**Biggest architectural borrow:** nanobind's intrusive-pointer holder
would let us support polymorphic returns without the typeid lookup hop.
A T that wants to be bound adds a small `intrusive_counter` base —
then all shared_ptr returns become "wrap this T*" with known type.

**Smallest architectural borrow:** pybind11's keyword-arg syntax
(`"name"_a = default`) is what we just re-implemented via reflection.
Theirs is manual; ours is automatic. Score one for reflection.

---

## Recommended order of attack

1. **Operator overloading** — unlocks the most common user-facing
   disappointment ("why can't I add two PointClouds?"). One session.
2. **__repr__ from members** — instantly makes the library feel alive.
   Half a session.
3. **Polymorphic returns (typeid lookup)** — fixes a real correctness
   issue. One session.
4. **Enum types verification + free functions end-to-end** — completes
   the Open3D surface. One session each.
5. **Exception mapping** — quality-of-life. Half a session.
6. **Docstrings via generator** — closes the help() / IDE gap.
   One-two sessions.

After these, mirror_bridge matches the user-observable surface of an
Open3D pybind layer with zero per-class binding code and — unique to us
— full Python-override capability via `bind_class_auto`.

The runtime Open3D link (#10) is an upstream Open3D build-system issue
that any clang-p2996 user would hit. We've documented the one-line fix
but actually shipping it needs an Open3D PR.
