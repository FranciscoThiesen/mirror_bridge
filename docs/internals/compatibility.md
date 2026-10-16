# Real-library compatibility

Auto-discovery is the thing that separates mirror_bridge from pybind11 and
nanobind: you point it at a header directory and get a module, instead of
writing a binding file. The only honest way to talk about that is to point it
at libraries nobody here wrote and publish what happens, including the
failures.

This page is regenerated from a scheduled run. Nothing in the corpus is
patched, vendored or special-cased — each library is cloned at a pinned tag and
the command is exactly:

```bash
mirror_bridge generate <include dir> --module <name> --lang python --stubs
```

## How to read it

**"Classes seen"** is what the reflection walk reported. It is the discovery
question on its own, and it is the number that matters for the comparison:
pybind11 and nanobind have no answer here, because the class list is something
their user writes by hand.

**"Module"** is the harder bar: the generated binding compiled, linked, and the
module imported. Discovery succeeding and the module failing is the common
case, and the reason is in the last column.

**Importing is not the bar.** A module can build, import, and still be useless.
entt produced one exporting `group_descriptor`, `null_t` and `tombstone_t` —
three internal tag types — and it sat in the table beside jsoncpp as an equal.
So each manifest entry declares `must_export`: the handful of names a caller of
that library reaches for first, taken from the library's own README and written
down before the module was measured, so the bar is not fitted to the result.

    "name": "box2d",
    "must_export": ["b2World", "b2Body"],

A module exposing none of them is `token-binding`. One missing some of them is
`partial-binding` — box2d exports `b2World` but not `b2Body`, so a world can be
created and nothing can move in it. Both count as failures, and both are more
useful rows than a green tick would have been, because they name the missing
class.

`expect` records what each library does today on a ladder — `bind` > `partial` >
`token` > nothing — and a run fails only when a library does *worse* than its
record. Raising the bar is not a regression: tinyxml2 builds exactly the module
it always built.

**"Why not, if not"** is the point of the table. A pass rate says how we are
doing. The cause says what to do next, and the `What is in the way` summary
below the matrix groups the corpus into a work queue — the next thing worth
fixing is the row with the most libraries on it.

A cause of `compile-error` or `unknown` is a gap in
`compatibility/run_compatibility.py`'s classifier, not a property of the
library. Those are the rows to look at first.

## The two shapes are not the same question

`shape: oo` is a library of classes with methods — jsoncpp, tinyxml2, pmp. These
are adoption targets: someone wants them callable from Python, and today they
would write a pybind11 project to get there. A failure in this group is work
worth doing.

`shape: template` is header-only generic code — fmt, glm, entt, magic_enum.
Nobody sensibly wants `std::formatter` specialisations exposed to Python
wholesale, so these are **robustness tests, not adoption targets**: the
question is whether the reflection pass survives a library built out of
template metaprogramming, or whether it blows the constexpr budget, recurses
forever, or crashes the compiler. "Discovery completed and said something
sensible" is a pass for this group even when no module is wanted.

The summary line reports the two separately for that reason. A single pass rate
over both numbers would be meaningless.

## What this does not claim

A library that binds is not thereby *well* bound. The matrix says a module was
produced and imported, with a usable `.pyi` beside it. It does not say every
class is ergonomic from Python, that container members round-trip the way you
want, or that the API is one a human would have designed. For that, see the
worked ports under `examples/`.

Times are wall-clock for one cold `generate` on the runner, including the
reflection pass over the whole translation unit. They are a usability number,
not a benchmark: for call-overhead measurements against pybind11, nanobind and
SWIG, see [benchmarks.md](benchmarks.md).

## Adding a library

One entry in `compatibility/libraries.json`. Nothing else changes:

```json
{
  "name": "yourlib",
  "repo": "https://github.com/org/yourlib",
  "ref": "v1.2.3",
  "headers": "include/yourlib",
  "include_dirs": ["include"],
  "module": "yourlib_compat",
  "shape": "oo",
  "expect": "unknown"
}
```

`shape` is `oo` for classes with methods or `template` for header-only generic
code; the summary reports the two separately because they fail for different
reasons and averaging them hides the signal. `expect` is `unknown` until a
library has actually bound once, after which setting it to `bind` makes the
scheduled run **fail** if it ever stops — that is how the matrix doubles as a
regression suite against real code.

Run it locally with:

```bash
python3 compatibility/run_compatibility.py --only yourlib
python3 compatibility/format_matrix.py
```

<!-- COMPAT:BEGIN (generated by compatibility/format_matrix.py - do not edit by hand) -->
_Generated 2026-10-16 at commit `75c400452ac0` on GitHub Actions ubuntu-latest (4 cores, containerized)._

**7 of 32 libraries bind usefully** — 7 of 22 class-based, 0 of 10 header-only generic. Every library is pinned to the tag shown and nothing here is written or patched by us: the command is `mirror_bridge generate <include dir> --module <name> --lang python`, run against an unmodified checkout.

"Usefully" is a measured bar, not a judgement call. Each entry in the corpus manifest carries a `must_export` list: the handful of names a caller of that library reaches for first, taken from its own README. A module that imports but exposes none of them is **binds nothing usable**, and one missing some of them is **binds, missing core API** — both are failures here. Without that bar, a module exporting entt's three internal tag types counted the same as one exporting jsoncpp's `Value`, `Reader` and `StreamWriterBuilder`.

Right now **6 of the modules that build are not usable**: box2d is missing b2Body; tinyxml2 is missing XMLElement; entt is missing registry; magic_enum is missing enum_name; nanoflann is missing KDTreeSingleIndexAdaptor; nlohmann-json is missing json.

Discovery ran through reflection on **16 of 32**; the rest fell back to the text scan, which the CLI reports as it happens. A `text-scan` row means reflection could not read those headers, so its class list is the old heuristic's and the failure cause belongs to that, not to binding.

| Library | Version | Shape | Discovery | Classes seen | Module | Time | Why not, if not |
|---|---|---|---|---|---|---|---|
| [clipper2](https://github.com/AngusJohnson/Clipper2) | `Clipper2_1.4.0` | oo | reflection | 20 | binds (780 KB) | 244.5s |  |
| [cxxopts](https://github.com/jarro2783/cxxopts) | `v3.2.0` | oo | reflection | 27 | binds (2.1 MB) | 113.7s |  |
| [jsoncpp](https://github.com/open-source-parsers/jsoncpp) | `1.9.5` | oo | reflection | 23 | binds (1.2 MB) | 88.0s |  |
| [pmp-library](https://github.com/pmp-library/pmp-library) | `3.0.0` | oo | text-scan | 32 | binds (2.2 MB) | 103.5s |  |
| [pugixml](https://github.com/zeux/pugixml) | `v1.14` | oo | reflection | 19 | binds (1.9 MB) | 62.6s |  |
| [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp) | `3.3.2` | oo | reflection | 9 | binds (1.7 MB) | 70.8s |  |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | `v2.0.0rc13` | oo | text-scan | 26 | binds (1.0 MB) | 41.8s |  |
| [box2d](https://github.com/erincatto/box2d) | `v2.4.1` | oo | reflection | 82 | binds, missing core API | 194.8s | the module is missing API a caller needs: b2Body |
| [tinyxml2](https://github.com/leethomason/tinyxml2) | `10.0.0` | oo | reflection | 15 | binds, missing core API | 77.6s | the module is missing API a caller needs: XMLElement |
| [entt](https://github.com/skypjack/entt) | `v3.13.2` | template | reflection | 3 | binds nothing usable | 92.0s | the module imports but exposes none of the API a caller needs: registry |
| [magic_enum](https://github.com/Neargye/magic_enum) | `v0.9.6` | template | reflection | 5 | binds nothing usable | 84.6s | the module imports but exposes none of the API a caller needs: enum_name |
| [nanoflann](https://github.com/jlblancoc/nanoflann) | `v1.6.2` | template | reflection | 10 | binds nothing usable | 65.5s | the module imports but exposes none of the API a caller needs: KDTreeSingleIndexAdaptor |
| [nlohmann-json](https://github.com/nlohmann/json) | `v3.11.3` | template | reflection | 20 | binds nothing usable | 33.7s | the module imports but exposes none of the API a caller needs: json |
| [abseil](https://github.com/abseil/abseil-cpp) | `20260817.0` | oo | text-scan | 375 | no module | 152.6s | reflection could not read these headers |
| [asio](https://github.com/chriskohlhoff/asio) | `asio-1-38-2` | template | text-scan | 318 | no module | 196.3s | reflection could not read these headers |
| [doctest](https://github.com/doctest/doctest) | `v2.4.11` | template | text-scan | 80 | no module | 30.0s | reflection could not read these headers |
| [eigen](https://gitlab.com/libeigen/eigen) | `3.4.0` | template | text-scan | 47 | no module | 39.9s | reflection could not read these headers |
| [glm](https://github.com/g-truc/glm) | `1.0.1` | template | text-scan | 3 | no module | 61.4s | reflection could not read these headers |
| [opencv](https://github.com/opencv/opencv) | `4.14.0` | oo | text-scan | 158 | no module | 226.1s | reflection could not read these headers |
| [range-v3](https://github.com/ericniebler/range-v3) | `0.12.0` | template | text-scan | 333 | no module | 140.3s | reflection could not read these headers |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | `v0.18.1` | oo | reflection | 13 | no module | 258.6s | the generated binding names a private nested type |
| [imgui](https://github.com/ocornut/imgui) | `v1.92.9b` | oo | reflection | 43 | no module | 242.6s | a method parameter has no conversion from Python |
| [CLI11](https://github.com/CLIUtils/CLI11) | `v2.4.2` | oo | text-scan | 53 | no module | 48.3s | these headers do not compile under this reflection compiler at all, with no mirror_bridge involved — `no member named 'wstring_convert' in namespace 'std'` |
| [fmt](https://github.com/fmtlib/fmt) | `10.2.1` | template | text-scan | 54 | no module | 58.1s | these headers do not compile under this reflection compiler at all, with no mirror_bridge involved — `call to consteval function 'fmt::basic_format_string<char, fmt::basic_` |
| [rapidjson](https://github.com/Tencent/rapidjson) | `v1.1.0` | oo | text-scan | 10 | no module | 52.4s | these headers do not compile under this reflection compiler at all, with no mirror_bridge involved — `cannot assign to non-static data member 'length' with const-qualified ` |
| [simdjson](https://github.com/simdjson/simdjson) | `v3.10.1` | oo | text-scan | 58 | no module | 230.3s | these headers do not compile under this reflection compiler at all, with no mirror_bridge involved — `"NEON intrinsics not available with the soft-float ABI. Please use -mf` |
| [spdlog](https://github.com/gabime/spdlog) | `v1.14.1` | oo | text-scan | 107 | no module | 14.6s | these headers do not compile under this reflection compiler at all, with no mirror_bridge involved — `call to consteval function 'fmt::basic_format_string<char, fmt::basic_` |
| [toml11](https://github.com/ToruNiina/toml11) | `v3.8.1` | oo | text-scan | 31 | no module | 73.7s | these headers do not compile under this reflection compiler at all, with no mirror_bridge involved — `no member named 'terminate' in namespace 'std'` |
| [fast_float](https://github.com/fastfloat/fast_float) | `v6.1.6` | oo | reflection | 3 | no module | 29.0s | **compile-error** — unclassified compile failure |
| [googletest](https://github.com/google/googletest) | `v1.18.0` | oo | reflection | 78 | no module | 161.0s | **compile-error** — unclassified compile failure |
| [yaml-cpp](https://github.com/jbeder/yaml-cpp) | `0.8.0` | oo | reflection | 12 | no module | 133.5s | **compile-error** — unclassified compile failure |
| [open3d](https://github.com/isl-org/Open3D) | `v0.20.0` | oo | ? | - | timed out | 300.0s | generate did not finish within the per-library budget |

### What is in the way

| Cause | Libraries | Count |
|---|---|---|
| `reflection-tu-failed` | abseil, asio, doctest, eigen, glm, opencv, range-v3 | 7 |
| `library-needs-newer-compiler` | CLI11, fmt, rapidjson, simdjson, spdlog, toml11 | 6 |
| `token-binding` | entt, magic_enum, nanoflann, nlohmann-json | 4 |
| `compile-error` | fast_float, googletest, yaml-cpp | 3 |
| `partial-binding` | box2d, tinyxml2 | 2 |
| `private-nested-type` | cpp-httplib | 1 |
| `no-converter-for-parameter` | imgui | 1 |
| `timeout` | open3d | 1 |

<!-- COMPAT:END -->
