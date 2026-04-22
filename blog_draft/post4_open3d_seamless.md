# mirror_bridge vs Open3D: 25,262 hand-written binding lines → 71 auto-generated, at pybind11-level speed

![Real C++ point clouds, processed via mirror_bridge](visuals/mirror_bridge_open3d_demo.png)

Every point cloud above was computed in plain C++ and reached Python
through a binding I didn't write. No `.def()`, no trampoline classes,
no keyword-arg boilerplate. Just `bind_class<PointCloud>(m, "PointCloud")`.
Reflection fills in the rest at compile time.

|                                         | pybind11 | mirror_bridge                          |
|-----------------------------------------|---------:|----------------------------------------|
| Hand-written binding lines (Open3D geom)| 3,610    | **71**  (51× less, auto-generated)     |
| `get_center` on 1M points               | 0.85 ms  | **0.74 ms**  (parity, within noise)    |
| `size()` dispatch overhead              | 0.25 µs  | **0.13 µs**  (2× faster)               |
| `PointCloud(list_of_1M_points)` ctor    | 565 ms   | **17 ms**   (33× faster)               |

(Fair bench: same C++ source, same clang-p2996, same `-O3 -stdlib=libc++`,
only the binding framework differs.)

**TL;DR**

- 47 Open3D geometry classes bind with **0 hand-written lines**.
  `mirror_bridge generate` emits the whole 71-line module.
- Runtime is at pybind11 parity on compute-heavy calls. The wins show
  up where the binding layer is the entire cost: dispatch (2×) and
  list-to-vector constructors (33×).
- The mechanism is *inlining*. Reflection gives the C++ compiler a
  concrete member reference where pybind11 has only an opaque
  pointer, and the assembly confirms it.
- Apache 2.0. One `docker run` reproduces every number in under ten
  minutes.

---

## 1. pybind11: write every binding twice

Here's how Open3D binds one method, `voxel_down_sample_and_trace`
([pointcloud.cpp L70-L75][o3d-pybind]):

```cpp
.def("voxel_down_sample_and_trace",
     &PointCloud::VoxelDownSampleAndTrace,
     "Function to downsample using PointCloud::VoxelDownSample. Also "
     "records point cloud index before downsampling",
     "voxel_size"_a, "min_bound"_a, "max_bound"_a,
     "approximate_class"_a = false)
```

Every method lives twice: once in the class declaration, again here.
Every parameter name is re-typed. Every default is re-typed. Every
overload gets its own `.def()`.

The pybind11 layer for Open3D's geometry module alone is **3,610
lines**. The whole Python binding spans **25,262 lines across 124
files**.

## 2. mirror_bridge: write nothing

One command. Zero hand-written binding code:

```
$ mirror_bridge generate Open3D/cpp/open3d/geometry \
      --module open3d_full --lang python \
      -I Open3D/cpp -I /usr/include/eigen3

  ✓ Found: OrientedBoundingBox in BoundingVolume.h
  ✓ Found: AxisAlignedBoundingBox in BoundingVolume.h
  ✓ Found: HalfEdgeTriangleMesh::HalfEdge in HalfEdgeTriangleMesh.h
  ✓ Found: KDTreeSearchParam::SearchType in KDTreeSearchParam.h
  ✓ Found: TriangleMesh::Material::MaterialParameter in TriangleMesh.h
  ✓ Found: VoxelGrid::VoxelPoolingMode in VoxelGrid.h
     ... 47 classes in all ...
  ✓ Built: build/open3d_full.so
```

A brace-depth parser finds every class, struct and enum. Nested types
get qualified correctly (`HalfEdgeTriangleMesh::HalfEdge`, not a bare
`HalfEdge` colliding with another file's).

Dependencies are resolved by reflection, not by the parser.
`std::meta::bases_of(T)` gives inheritance edges. The enclosing scope
gives nesting. `parameters_of` and the reflected return type catch
every class referenced from a method signature. The generator
topologically sorts the result, so `AxisAlignedBoundingBox` lands
before `PointCloud` (which returns one from `get_aabb()`), and every
nested type lands after its enclosing scope.

The emitted module is short enough to read:

```cpp
MIRROR_BRIDGE_MODULE(open3d_full,
    mirror_bridge::bind_class<OrientedBoundingBox>(m, "OrientedBoundingBox");
    mirror_bridge::bind_class<AxisAlignedBoundingBox>(m, "AxisAlignedBoundingBox");
    mirror_bridge::bind_class<PointCloud>(m, "PointCloud");
    // ... 43 more bind_class lines ...
    mirror_bridge::bind_class<AggColorVoxel>(m, "AggColorVoxel");
)
```

**71 lines total.** Each `bind_class<T>` pulls constructors, methods,
fields, operators, kwargs, defaults, `__repr__`, subclassing,
inheritance and polymorphic returns straight from the header.

## 3. Runtime: pybind11 parity, with two clear wins

The binding layer is cheap. The fair test is to change *only* the
binding framework and measure: same `PointCloud` source, same
clang-p2996, same `-O3 -stdlib=libc++ -fPIC` ([source][asm-bench]):

| Operation                          | pybind11   | mirror_bridge | ratio                       |
|------------------------------------|-----------:|--------------:|-----------------------------|
| `get_center` on 1M points          | 0.85 ms    | 0.74 ms       | **parity** (within run-to-run noise)  |
| `size()` (near-empty dispatch)     | 0.25 µs    | 0.13 µs       | **2× faster**               |
| Constructor from 1M-point list     | 565 ms     | 17 ms         | **33× faster**              |

Three different regimes, three different answers:

- **Compute-heavy ops are already fast.** `get_center` is 1M
  fused-multiply-adds in SIMD. Binding-layer overhead rounds to zero
  compared to the math. mirror_bridge and pybind11 end up
  indistinguishable here.
- **Dispatch-dominated ops show the binding cost.** `size()` is a
  one-word load; the entire cost *is* the dispatcher. mirror_bridge's
  inlined dispatcher wins cleanly.
- **Container conversions are a different cost model.** pybind11's
  generic `list → std::vector<Eigen::Vector3d>` path runs through a
  type-caster per element. mirror_bridge's reflection-specialised
  path bulk-copies. Big win for data ingest.

### A note on the Open3D wheel benchmark

An earlier draft compared against Open3D 0.18's shipped wheel
(`pip install open3d`) and showed mirror_bridge winning on centroid
(2.2×), AABB (1.4×) and voxel downsample (1.4×). That table is
available ([bench source][bench]) but the comparison conflates three
factors:

1. Binding-layer overhead.
2. Compiler version (Open3D's wheel is built with clang 7, ours is
   clang 21).
3. Threading model (Open3D links libgomp and parallelises
   `voxel_down_sample`; mirror_bridge's demo C++ is single-threaded).

The fair-bench table above is the pure binding-layer number. The
Open3D comparison is a real-world "what does `pip install open3d`
give you vs. what does `mirror_bridge generate Open3D/` give you"
data point, not a pybind11-vs-mirror_bridge head-to-head.

## 4. Why dispatch is cheaper: proof in the assembly

The dispatch win (the `size()` row above) comes from a specific
mechanism. Reflection gives the C++ compiler something pybind11
can't: a concrete, splice-called member function at the dispatcher
site. The optimiser inlines the method body directly into the
binding.

All snippets below are clang-p2996 at `-O3`, linux-aarch64, libc++,
same `PointCloud::get_center()` source. Only the binding layer
changes ([source][asm-bench]).

**pybind11 crosses four function boundaries before the loop runs:**

```asm
; pybind11 cpp_function::initialize<...>::__invoke body
bl    pybind11::detail::type_caster_generic::type_caster_generic(...)  ; 1
bl    pybind11::detail::type_caster_generic::load_impl<...>(...)        ; 2
ldr   x9, [x8, x9]                  ; member-function-ptr resolution
blr   x9                            ; 3: indirect call → get_center()
bl    type_caster<Eigen::Matrix<...>>::cast_impl<...>(...)              ; 4
```

`get_center` stays a separate symbol in the binary. The dispatcher
calls it; it can't see it.

**mirror_bridge has no such boundary.** The reflection splice
`((*wrapper->cpp_object).[:member_func:])(...)` lets the compiler
inline the full method body into the dispatcher:

```asm
; mirror_bridge invoke_with_n_args<PointCloud, 0> body, -O3
ldr   x8, [x0, #8]
ldr   x8, [x8]
ldr   x9, [x8, #16]
ldp   x8, x9, [x9]
.LBB35_2:                           ; INLINED get_center loop
    ldr   q2, [x10]
    ldr   d3, [x10, #16]
    fadd  v1.2d, v1.2d, v2.2d       ; no function call
    fadd  d0, d0, d3                ; between Python and math
    b.ne  .LBB35_2
bl    PyList_New                    ; return marshalling:
bl    PyFloat_FromDouble            ; exactly four CPython calls,
bl    PyFloat_FromDouble            ; nothing else
bl    PyFloat_FromDouble
ret
```

The `PointCloud::get_center()` symbol doesn't exist in the
mirror_bridge object file. Verified:

```bash
$ grep -c 'PointCloud.*get_centerEv' bind_mb.s
0
$ grep -c 'PointCloud.*get_centerEv' bind_pybind.s
8
```

Why pybind11 can't inline: its dispatcher template holds a
`Ret (Class::*)(...)` pointer, opaque at instantiation time. The
reflection splice gives the compiler a concrete member info instead.

### Scope check: the whole object file

Same one-class binding, same flags:

| File            | `.s` size  | `bl`/`blr` instructions |
|-----------------|-----------:|------------------------:|
| `bind_mb.s`     | 240 KB     | 271                     |
| `bind_pybind.s` | 2,762 KB   | 4,420                   |

11.5× more assembly, 16× more calls. The overhead is pybind11's
`type_caster<...>` specialisations for every primitive it *might*
convert. Reflection-based dispatch doesn't emit what it doesn't need.

## 5. Build flags matter

| Build flags                          | Centroid | AABB         | Voxel    |
|--------------------------------------|----------|--------------|----------|
| `-O2`                                | 0.91 ms  | 2.28 ms      | 36.6 ms  |
| `-O3`                                | 0.93 ms  | 2.63 ms      | 34.7 ms  |
| `-O3 -march=native`                  | 0.71 ms  | 2.74 ms      | 34.4 ms  |
| `-O3 -march=native -ffast-math`      | 0.84 ms  | **1.32 ms**  | 37.1 ms  |

`-O3` over `-O2` is noise. `-march=native` buys 20-30% on Centroid
through chip-specific vector reductions. `-ffast-math` halves AABB
because NEON's min/max drops its NaN-propagation branch when finite
data is promised. Parallelism (OpenMP, TBB) stays the user's call.
Reflection can't tell which loops are safe to parallelise.

## 6. What the FFI still costs

The binding layer is cheap. Crossing the FFI is not:

| Operation                              | Cost     |
|----------------------------------------|----------|
| `PointCloud(list_of_1M_points)`        | ~18 ms   |
| `len(pcd.points)` (reads 1M points out)| ~270 ms  |

Handing a million points into C++ is a real 1M-vector copy. Reading
them back as a Python list re-materialises a million Python objects.

**Rule of thumb: keep data inside C++ as long as possible, and only
extract what you display.** Every speedup above holds because the
points live in the C++ `PointCloud`, and each operation is one method
call plus a scalar return.

A roadmapped follow-up exposes `.points` through the Python buffer
protocol so numpy can share the memory zero-copy.

## 7. Everything reflection can see is free

Auto-generated per class, no user input:

- **Constructors** as Python `__init__` overloads, resolved by arg
  count and type.
- **Methods**, direct and inherited via `bases_of` BFS walk.
- **Fields** as Python attributes (getter/setter).
- **Operators**: `+ - * / %  == != < > <= >=  += -= *= /= []  ()` and
  unary `+/-`. Routed via `std::meta::operator_of` to the matching
  Python slot.
- **Keyword arguments** from `std::meta::identifier_of` on each
  parameter info.
- **Default argument values** detected via `has_default_argument`.
- **`__repr__`** from visible members.
- **Exception mapping**: `std::out_of_range → IndexError`,
  `std::invalid_argument → ValueError`, `std::runtime_error →
  RuntimeError`, via `dynamic_cast`.
- **Polymorphic returns** resolved through `typeid` lookup.
- **Nested classes** qualified as `Parent::Child`.

### Auto-trampoline: Python subclasses override C++ virtuals for free

pybind11 lets Python subclasses override C++ virtuals, but the
plumbing isn't free. You have to write a *trampoline class*: a
hand-forwarded layer with one entry per virtual, each using
`PYBIND11_OVERRIDE(...)` to route back to Python if the subclass
defined an override. One class, one trampoline. Ten classes, ten.

mirror_bridge generates the trampoline from reflection.
`bind_class_auto<T>` enumerates T's virtual slots, synthesises a
per-slot dispatcher that routes into Python when the instance has an
override, and swaps T's vtable with a custom one at construction.
You write zero glue.

```python
class TaggedPointCloud(o3d.PointCloud):
    def GetCenter(self):
        return [999, 999, 999]   # reached by C++ too, via the vtable swap
```

No `PYBIND11_OVERRIDE`, no dispatch helper, no trampoline
boilerplate. (Caveat: works on Linux and macOS today; see §10.)

## 8. Running against real libOpen3D.so

This isn't a theoretical exercise. The binding loads and calls into
a real `libOpen3D.so` built from source. Getting there took **two
one-line CMake patches**:

1. **Forward `CMAKE_CXX_FLAGS` to ExternalProjects** so 3rdparty
   libraries (VTK, embree, zmq, ...) inherit `-stdlib=libc++` from
   the top-level build. Without it, the final `.so` mixes
   `std::__1::*` (libc++) and `std::__cxx11::*` (libstdc++) symbols
   and fails to dlopen.

2. **Wrap BoringSSL archives with `-Wl,--whole-archive`** when
   linking `libOpen3D.so`, or dlopen fails on `X509_INFO_free` (curl
   and zmq reach SSL through transitive calls the linker otherwise
   drops).

The diff lives at [`examples/open3d-full-port/patches`][fork] as a
single `.patch` file. macOS and Windows builds are unchanged.

With the patched fork, a real Open3D Python session
([source][runtime]):

```python
import real_open3d as o3d

pcd = o3d.PointCloud([[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]])
print(pcd.GetCenter())             # [0.25, 0.25, 0.25]
print(pcd.HasPoints())             # True

smaller = pcd.VoxelDownSample(voxel_size=0.5)   # kwargs + defaults
aabb    = pcd.GetAxisAlignedBoundingBox()        # polymorphic return
print(aabb.Volume())                              # 1.0

# Subclass Open3D with a Python override. No trampoline.
class TaggedPointCloud(o3d.PointCloud):
    def GetCenter(self):
        return [999, 999, 999]
```

Zero `.def` calls.

## 9. Try it yourself (10 minutes)

```bash
git clone https://github.com/FranciscoThiesen/mirror_bridge
cd mirror_bridge
./docker_build.sh

docker run -it -v $(pwd):/workspace -w /workspace mirror_bridge:latest
# --- inside the container ---

cd examples/open3d-comprehensive
./build_and_test.sh                 # 10 feature groups pass
python3 visual_demo.py              # renders mirror_bridge_open3d_demo.png
python3 bench_three_way.py          # the 3-way benchmark above

cd ../open3d-runtime
./build_and_test.sh                 # 10 tests against libOpen3D.so
```

Every table, every number, every image reproduces.

## 10. What it isn't yet

- **Compiler support.** Needs C++26 reflection (P2996). That means
  clang-p2996 today (pinned in the Docker image) or GCC trunk's
  15-series. **MSVC hasn't implemented P2996.** That's the real
  portability ceiling right now.

- **Auto-trampoline is Linux/macOS only** (Itanium ABI). On MSVC the
  portable `bind_class<T, Trampoline>` path still works: you write a
  small trampoline class that forwards each virtual to
  `dispatch_python<Ret>("name")`. Same behaviour, a few lines per
  virtual of hand-written glue.

- **Default argument *values*** aren't exposed by P2996R13, only
  their *presence*. You can skip trailing defaulted args in a kwargs
  call; you can't skip a middle one and supply a later one.

- **Docstrings.** P2996 doesn't expose Doxygen comments. Workarounds
  exist (`[[=mb::doc("...")]]` via P3394, or a parallel parser). Not
  shipping today.

## Star the repo, tell us what to bind next

mirror_bridge is Apache 2.0 at [FranciscoThiesen/mirror_bridge][repo].
It emits Python, Lua and JavaScript bindings today from the same C++
reflection input.

**If you've ever written a pybind11 `.def()` chain for a
hundred-method class and wished there was a better way, this is it.**

If the numbers surprised you, run the reproduction and try to break
them. If you hit something that doesn't work, file an issue. The
repo is small and feedback drives priorities.

⭐ **GitHub**: [github.com/FranciscoThiesen/mirror_bridge][repo]

---

*Thanks to the clang-p2996 team at Bloomberg and the P2996 author
community. None of this is possible without their reflection work.*

[repo]:       https://github.com/FranciscoThiesen/mirror_bridge
[bench]:      https://github.com/FranciscoThiesen/mirror_bridge/blob/main/examples/open3d-comprehensive/bench_three_way.py
[runtime]:    https://github.com/FranciscoThiesen/mirror_bridge/tree/main/examples/open3d-runtime
[fork]:       https://github.com/FranciscoThiesen/mirror_bridge/tree/main/examples/open3d-full-port/patches
[o3d-pybind]: https://github.com/isl-org/Open3D/blob/main/cpp/pybind/geometry/pointcloud.cpp
[asm-bench]:  https://github.com/FranciscoThiesen/mirror_bridge/tree/main/asm_study
