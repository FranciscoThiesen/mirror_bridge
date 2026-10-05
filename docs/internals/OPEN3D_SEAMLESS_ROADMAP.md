# Roadmap to "seamless Open3D with mirror_bridge"

Honest assessment of where mirror_bridge stands for the Open3D use case,
ordered by user impact.

---

## What's done ✅ — all runtime-verified

### Core language surface

- **Compile parity**: all 38 Open3D geometry classes compile from one
  `bind_class<T>(m, "T")` each. Abstract bases (`Geometry`,
  `Geometry3D`, `MeshBase`, `OctreeNode`, `KDTreeSearchParam`),
  non-copyable types (`Line3D`, `Segment3D` via virtual bases),
  C-style arrays, nested classes (`HalfEdgeTriangleMesh::HalfEdge`,
  `TriangleMesh::Material::MaterialParameter`), smart-ptr-to-abstract
  members, and vector<bool> all handled without per-class code.

- **Inheritance** (P2996 `bases_of` walk): BFS with C++ name hiding.
  Derived classes expose every inherited method + field. Virtual
  overrides dispatch correctly through splicing — confirmed: an
  inherited `get_extent()` defined on `Geometry3D` that internally
  calls `get_max_bound()` reaches the derived class's override.

- **Trampolines** — two variants:
  - **Auto-trampoline** (`bind_class_auto<T>`): reflection-driven
    vtable swap. Enumerates virtuals, generates per-virtual dispatcher
    free functions whose signatures come entirely from reflection
    splicing (`[:return_type_of(M):]`, `[:type_of(params_of(M)[Is]):]...`),
    assembles a custom vtable, swaps the instance's vptr on
    construction. Python subclasses override C++ virtuals with ZERO
    glue. Itanium ABI (Linux/macOS).
  - **Explicit trampoline** (`bind_class<T, Trampoline>`): portable
    fallback for MSVC and users who prefer auditable glue.

- **Keyword arguments + default parameter values**: both derived from
  reflection. `identifier_of` gives parameter names,
  `has_default_argument` tells whether a default exists, and splicing
  `self.[:M:](a,b)` with fewer args than declared lets C++ fill
  defaults at the call site. Open3D's
  `pcd.estimate_normals(radius=.., max_nn=..)` style works directly.

- **Operator overloading**: reflection's `operator_of(info)` classifies
  each into Python slots. Supported: `+ - * / %` → `nb_*`, `+= -= *= /= %=`
  → `nb_inplace_*`, `== != < > <= >=` → `tp_richcompare`, `[]` →
  `mp_subscript`, `()` → `tp_call`, unary `+/-` → `nb_negative/positive`.
  Comparisons normalize to Py_True/Py_False.

- **`__repr__` from members**: iterates visible fields, formats as
  `ClassName(field=value, …)`. `repr(Vec3(1,2,3))` →
  `Vec3(x=1.0, y=2.0, z=3.0)`.

- **Polymorphic return wrapping**: when `to_python` handles a
  polymorphic type, `typeid(obj).name()` looks up the DYNAMIC type in
  the Python type registry. A `shared_ptr<Geometry>` that actually
  holds a `PointCloud` materializes as a PointCloud wrapper.

- **Exception mapping**: C++ std:: exceptions translate to the
  most-derived Python type via `dynamic_cast`:
  - `std::out_of_range` → `IndexError`
  - `std::invalid_argument` / `std::domain_error` → `ValueError`
  - `std::runtime_error` / `std::logic_error` → `RuntimeError`
  - `std::bad_alloc` → `MemoryError`

- **Nested class auto-discovery**: CLI tool's brace-depth parser
  emits `HalfEdgeTriangleMesh::HalfEdge`, `TriangleMesh::Material::MaterialParameter`,
  etc. with proper scope qualification.

### Real Open3D build ✅

- Open3D 0.19 **builds cleanly from source** with clang-p2996 + libc++
  via its own CMake (one sed-patch to fmt/format-inl.h for a
  FMT_STRING consteval issue). Produces `libOpen3D.so.0.19.0`.

---

## What needs more work ⚠️

### Runtime linking of libOpen3D.so — upstream issue

**Status**: libOpen3D.so BUILDS cleanly (we've verified) but FAILS to
`dlopen` due to two upstream Open3D build-system issues:

1. **BoringSSL archives** (`libssl.a`, `libcrypto.a`) are linked into
   libOpen3D.so without `-Wl,--whole-archive`. When curl/zmq reference
   SSL symbols that aren't directly reachable from Open3D's own .o
   files, they get dropped from the final .so. Runtime dlopen then
   fails on `X509_INFO_free` etc.

2. **3rdparty ABI mismatch**: Open3D's CMake downloads & builds VTK,
   embree, UVAtlas, zmq with each project's own CXXFLAGS — which
   default to libstdc++. Our libOpen3D.so is libc++. Mixing
   `std::__1::*` and `std::__cxx11::*` symbols in one .so triggers
   undefined-symbol failures on `basic_ostringstream`, `std::thread`,
   etc.

**Fix paths** (all upstream Open3D work, not mirror_bridge):
- Patch Open3D's top-level link recipe to wrap BoringSSL with
  `--whole-archive`.
- Patch each 3rdparty ExternalProject to forward `CMAKE_CXX_FLAGS`
  with `-stdlib=libc++`.
- OR: add `BUILD_TENSOR=OFF` / `BUILD_IO_RPC=OFF` options to Open3D
  that let users carve out the problem modules.

**Workaround implemented today**: `examples/open3d-comprehensive` builds
a parallel Open3D-shape geometry library with the same class hierarchy
(Geometry → Geometry3D → AABB / PointCloud), exercising all
mirror_bridge features against realistic real-Open3D-matching code. 10
feature groups all pass runtime — Python subclass overrides, Eigen
roundtrip, kwargs, operators, repr, inheritance.

### Remaining polish — all doable

- **Python protocols**: `__len__` from `size()`, `__getitem__` already
  works via `operator[]`, `__iter__` from `begin()`/`end()`. ~200
  lines, self-contained.

- **Docstrings**: P2996R13 doesn't expose doc-comments. Options:
  (a) generator-side doxygen parser that emits a doc table, (b)
  C++26 `[[=doc("…")]]` annotations via P3394. ~1 day.

- **Proper shared_ptr holder**: today's polymorphic-return support
  gives the correct Python type but copies the C++ object (slicing).
  A side-map keyed on `cpp_object*` with shared_ptr<void> anchors
  would preserve polymorphism through the wrapper's lifetime. ~150
  lines, borrowed from nanobind's intrusive-ptr approach.

- **Python type stubs (.pyi)**: existing `scripts/generate_stubs.py`
  needs polish to reflect parameter names, defaults, overloads.

---

## Feature matrix — mirror_bridge vs pybind11

| Feature                           | pybind11                | mirror_bridge                          |
|-----------------------------------|-------------------------|----------------------------------------|
| Simple class binding              | `class_<T>(m, "T")`     | `bind_class<T>(m, "T")`               |
| Inherited methods                 | Manual via `.def`       | **Auto via `bases_of`**               |
| Abstract base support             | Yes                     | Yes                                    |
| Python subclass → override C++    | `PYBIND11_OVERRIDE`     | **`bind_class_auto<T>` — zero glue**   |
| Keyword arguments                 | `"name"_a`              | **Auto from `identifier_of`**          |
| Default argument values           | `"name"_a = default`    | **Auto from `has_default_argument`**   |
| Operator overloading              | `py::self + py::self`   | **Auto from `operator_of`**            |
| `__repr__` from members           | Manual lambda           | **Auto from reflection**               |
| Exception translation             | Manual `register_type`  | **Auto via dynamic_cast**              |
| Polymorphic returns               | Shared_ptr holder       | **Auto via `typeid` lookup**           |
| Nested class discovery            | Manual                  | **Auto via brace-depth parser**        |
| Docstrings                        | String arg to `.def`    | ⚠ P2996 gap — needs annotations       |
| Trampolines for virtuals          | User-written macro      | **Auto via vtable swap (Linux/macOS)** |

mirror_bridge's pitch: same surface, **zero glue code per class**.

---

## Strategies worth borrowing

- **nanobind's intrusive-pointer holder**: drop shared_ptr sidemap in
  favor of a small `intrusive_counter` base, making polymorphic
  ownership explicit. Next step after today's typeid-lookup approach.

- **scikit-build-core**: standardizes CMake-based Python extension
  distribution. Natural fit when mirror_bridge ships as a pip-installable
  binding toolkit.

- **pybind11's doc pipeline**: they bake strings into `.def()` calls.
  We can do the same via a compile-time annotation — `[[=mb::doc("...")]]`
  on methods, read via P3394 annotation reflection.

---

## Recommended next steps

1. **Python type stubs**: polish the existing generator to emit proper
   `.pyi` files with parameter names, defaults, overload sets.
2. **`__len__` / `__iter__`** from reflection-detected `size()` +
   `operator[]`. Half a session.
3. **Shared_ptr holder refactor** (nanobind-style intrusive ptr).
   One session.
4. **Open3D CMake upstream PR**: fix `--whole-archive` on BoringSSL +
   libc++ propagation to 3rdparty. Unlocks real libOpen3D.so dlopen.

After these, "mirror_bridge binding for real Open3D" is drop-in
deployable. The C++-side reflection features already match pybind11's
feature surface; the remaining work is infrastructure.
