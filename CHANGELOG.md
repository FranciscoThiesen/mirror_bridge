# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Methods, static methods and free functions are called through the vectorcall protocol.** CPython hands a fastcall entry point a flat array of borrowed references; the previous `METH_VARARGS` entry point made it allocate an argument tuple and a keyword dict per call and tear both down. At zero arguments the two conventions already measured the same, which is what identified the tuple rather than the design as the cost: the gap grew about 2ns per argument. Keyword calls gained most, 87ns to 33ns.
- **Scalar conversion no longer leaves the header in the common case.** An exact Python float is one macro load, and an int small enough for CPython's compact representation is a load and a sign flip, read directly below 3.12 and through the public `PyUnstable_Long_*` accessors from 3.12 on. The public readers are calls into libpython that cannot be inlined, and on an 80,000-element list that call was the whole difference between the integer and floating-point ingest paths. Anything the fast path declines — a wider int, the stable ABI, PyPy — falls through to the public API.
- **Lists and tuples of exact scalars convert through a bulk tier** that needs neither the per-element size re-read nor the `from_python` dispatch the general tier needs. A non-exact element is not an error: the tier stops and the general one resumes at that index.
- **Trading workload benchmarks** (`benchmarks/runtime/shared/trading_bench.hpp`): a price-level order book, Black-Scholes over a contract vector, and an EWMA crossover driven at every chunk size from 1 to 262,144. Each framework is timed in its own interpreter, fastest of nine runs with the collector off, because sharing a heap made the third-timed framework read up to 30% slow.
- **Generated modules are built with `-fvisibility=hidden`.** A module was exporting every template instantiation the reflection pass produced, 170 of them on the benchmark class. Through the CLI the module goes from 109,336 to 92,512 bytes, a 15.4% cut, almost all of it symbol tables and relocations. Lua's `luaopen_` entry point is now marked with a new `MIRROR_BRIDGE_EXPORT`; `PyMODINIT_FUNC` and `NAPI_MODULE` already carried the attribute.
- **`mirror_bridge api`: gate a module's Python surface on a committed stub.** Generating a module is the easy half of running this in production; the hard half is the Python team finding out when the C++ team changes something. The generated `.pyi` is the one artifact that cannot drift from the binding — it is filled in during `bind_class` from the same reflection data — and it is byte-identical between runs and between clang-p2996 and GCC 16, so it can be committed and compared like a lockfile. `api --update` writes the baseline, `api --check` fails CI when a name disappears or changes signature, and additions pass because nobody's import breaks when a method appears. The report classifies the diff rather than printing it, so a C++ pull request shows which names the Python team is about to lose. New guide: `docs/guides/independent-teams.md`.
- **GIL release policy**: `bind_class<T>(...).release_gil()` (or `mirror_bridge generate --release-gil`) drops the GIL while C++ method bodies run, so long native calls stop blocking other Python threads — 4 threads × 150ms native sleep now finish in ~150ms instead of ~600ms (regression-tested in `tests/gil_release/`). Safe by construction: `std::function` callbacks and virtual-override trampolines re-acquire the GIL themselves, so no per-method signature reasoning is needed. Zero overhead when not enabled.
- **Free-threaded CPython declaration**: compiling a module with `-DMB_FREE_THREADED` on Python 3.13+ declares `Py_MOD_GIL_NOT_USED`, preventing a mirror_bridge module from silently re-enabling the GIL process-wide on free-threaded builds (opt-in, mirroring nanobind's `FREE_THREADED` flag). New guide: `docs/guides/threading.md`.
- `bind_class` now returns a fluent `BoundClass<T>` handle (implicitly converts to `PyTypeObject*`, so existing code is unaffected) — the home for per-class binding options like `release_gil()`.
- **Reflection-exact `.pyi` stubs**: every module now exposes `__mirror_bridge_stubs__()`, populated automatically during `bind_class` from the same reflection data the binding is generated from — exact member types, constructor overloads (`@overload`), real C++ parameter names, `@staticmethod` markers, Eigen/container/map type hints. `mirror_bridge generate --stubs` writes `<module>.pyi` next to the built module. No runtime parsing: stubs cannot drift from the binding.
- **Header-to-wheel scaffolding**: `mirror_bridge init <name> --wheel` emits pyproject.toml (scikit-build-core), a CMake build consuming mirror_bridge via find_package/FetchContent, an explicit binding TU, and a wheel-building GitHub workflow. Validated end-to-end: `pip wheel . && pip install dist/*.whl && python -c "import <name>"` works from a fresh scaffold.
- **Stock GCC 16+ support**: mirror_bridge now compiles with upstream GCC's C++26 reflection (`g++ -std=c++26 -freflection`), not just Bloomberg's clang-p2996 fork. The CLI, `mirror_bridge_build`, doctor, and test runner auto-detect the available reflection compiler (override with `MB_CXX`).
- `mirror_bridge init <name>` scaffolds a ready-to-build project: example header, per-language smoke tests, README, and a GitHub Actions workflow that builds and tests the bindings in the reflection container
- **Bulk container ingest**: `from_python` for numeric containers accepts any contiguous buffer (`array.array`, NumPy arrays, `memoryview`) via a single memcpy — the ingest mirror of the existing `array.array` return path
- List/tuple fast paths in container and Eigen conversion (borrowed-reference access, `PyFloat_AS_DOUBLE` for exact floats): the 1M-point `list[[x,y,z]] -> vector<Eigen::Vector3d>` ingest went from 18.4ms to 12.2ms (28x -> 42x vs pybind11 on the fair benchmark)
- **By-reference argument passing**: lvalue-reference parameters of bound classes (`const T&`, `T&`) now alias the Python-held object instead of copying it — zero copies for `const T&`, and `T&` mutations are visible from Python (matching pybind11 semantics); regression-tested with an instrumented copy counter
- `find_package(mirror_bridge)` now delivers the same `mirror_bridge_python_module()` / `_lua_module()` / `_js_module()` helper API as add_subdirectory/FetchContent (helpers moved to an installed `MirrorBridgeModules.cmake`); helpers gained an `OUTPUT_DIRECTORY` argument and lazy dependency lookup
- CMake integration guide (`docs/guides/cmake.md`)

### Changed
- Pack expansions over reflected parameter types use alias templates (`method_param_t`, `static_method_param_t`, `*_constructor_param_t`, `virtual_param_t`) instead of inline splices, so the parameter pack is visible to both GCC and clang.
- `PyTypeObject`/`BufferView` initializers designate `.ob_base` so GCC accepts them alongside the other designated fields.
- CLI Lua modules are emitted to `<output>/lua/<module>.so` — Python and Lua both produced `<module>.so`, so `generate --lang all` silently overwrote one with the other
- `ctest` now registers exactly the tests whose modules the CMake build produces (bash-harness-only tests are excluded and remain covered by CI's bash jobs); test files prefer freshly built modules over stale artifacts in `build/`
- Benchmark documentation consolidated: `docs/internals/benchmarks.md` (CI-regenerated monthly) is the single canonical results page; stale/contradictory results docs removed, and the runner no longer records zero-filled placeholders for frameworks that failed to build
- CMake compiler check and package config understand GCC 16+ (previously warned that only clang was supported)
- Removed the unused `ClassMetadata`/`Registry` pair from `core/` — the Python backend's registry (the only one ever used) is the single implementation

### Added (real-world header hardening — found by binding Clipper2 cold)
- **const data members bind as read-only attributes** (readable, assignment raises `AttributeError`, `@property` in stubs) instead of failing to compile
- **CLI-generated bindings skip unbindable classes with a per-class reason** (`bind_class_when_bindable`) instead of failing the whole module: "31 discovered, 12 bound, 15 skipped (reasons)" — hand-written `bind_class` keeps the hard `static_assert`
- **Methods/operators with unconvertible return types (raw pointers, iterators) are skipped** like param-unbindable ones, for named methods, static methods, number slots, rich-compare, and subscript
- **Constructors reflection can see but Python can't call (protected/private) are filtered** via `is_constructible_v`
- **Auto-discovery tracks access specifiers**: private/protected nested classes are no longer emitted (binding them is ill-formed)
- `--link-args` on `generate` for libraries that aren't header-only (`--link-args "/path/libFoo.a"`)

### Fixed
- **A write through a nested data member was silently discarded.** `o.p.x = 7.0` left `o.p.x` at `0.0` with no error, because the getter handed back a wrapper around a fresh heap copy and the write landed on the copy, which the expression then dropped. A non-static data member is interior to the wrapped object by construction, so the getter now returns a view onto the owner's storage holding a strong reference to the owner. That is settled at compile time from the reflected member list: unlike a method return it needs no return value policy from the author and no runtime instance lookup, which is the part pybind11 and nanobind cannot copy, since they receive a member pointer and nothing else. Reading a member no longer copies at all, measured 1000 copy constructions to 0 with an instrumented copy constructor. Method returns are deliberately unchanged -- interiority cannot be established for them, and the inference that was proposed fails in both directions -- and container elements are deliberately excluded, because a view into a `std::vector` dangles the moment it reallocates. Covered by `tests/member_views/`.
- **Cross-module type sharing is now keyed on the wrapper layout as well as the type.** The forward registry hands out one `PyTypeObject` per C++ class process-wide and `tp_basicsize` comes from whichever module registered it first, so two modules built against different revisions of the header disagreed about how big a wrapper is and the newer one would write its trailing fields past the end of an allocation the older one sized. That is a heap overflow rather than a wrong answer, and it was reachable as soon as the wrapper grew a field. Every cross-module key now carries a layout tag, so a mismatched pair simply does not recognise each other's objects and the failure is a `TypeError` at the boundary. Matching builds share types exactly as before. Because the resulting conversion failure names a class whose name looks correct, a module that finds another layout already loaded now says so once at import, as a `RuntimeWarning` naming both layouts, rather than leaving the `TypeError` to be puzzled over.
- **A C++ exception thrown from a bound free function or from a constructor aborted the process.** It unwound past the interpreter and `std::terminate` ended the session with exit 134: no traceback, nothing catchable. Instance methods and static methods were already wrapped; these two paths were not, and a constructor that validates its arguments and throws is ordinary C++.
- **Constructing with keyword arguments silently produced a default object.** `py_init` read `PyTuple_Size(args)` and never looked at `kwds`, so `Cls(a=1, b=2)` had `nargs == 0`, fell into the default-construct branch and reported success — the caller got an object built from none of their values and no error. Keyword arguments now resolve against the constructor's reflected parameter names, so mixed and out-of-order calls work and an unknown name is refused rather than ignored.
- **Integer arguments are range-checked.** The old path was an unchecked cast, so `2**31` into a C++ `int` arrived as `-2147483648`, and `2**200` arrived as `-1` with an `OverflowError` left set on the thread to be reported later by whatever unrelated call next entered CPython. Out of range is now a `TypeError` at the boundary with nothing left pending.
- **A failed call says why.** Four different mistakes previously produced the same line, naming neither the bad keyword, the arity wanted, nor the parameters that exist. Where exactly one overload competed for the call, the message now reads `scale() got an unexpected keyword argument 'bogus' (takes factor, times)` or `pv() takes 2 to 3 argument(s) (cash, periods, spread), got 1`. With a real overload set the generic line is kept, because naming one signature would mean picking an arbitrary member of the set. The error path also got 17% quicker.
- **`generate` now says what it could not bind.** A build that printed `✓ Successfully built` could still be missing names the user wrote, with the only notice being one line of stderr eight lines earlier, or nothing at all. One report after the banner now names them, grouped by reason and capped: enum types, free functions dropped for being overloaded, and free functions that share a name with a function template. The same records reach `--json` as a typed `unbound` field, since `status` was `"ok"` and `errors` was `[]` for all of them.
- **Enums are no longer discovered as classes.** The discovery scan matched the token `class` inside `enum class Side`, so every scoped enum was reported as a class to bind and then skipped at import with a message that never said "enum"; a plain `enum Color` was invisible at every stage. Validated across 620 headers in this repo: 94 names removed, none added, every one of them an enum. Generated stubs also annotate enum members and parameters as `int` instead of naming the C++ enum type, which the module never defines and which made the whole `.pyi` unusable to a type checker. `examples/09-enums` documents what does and does not work rather than promising an API that was never implemented.
- **Two classes with the same unqualified name are caught before codegen.** A module binds every class by its simple name, so `lib::Config` and `lib::detail::Config` made the compiler reject a generated file the CLI then deleted. `generate` now names both declaration sites and explains why `MIRROR_BRIDGE_SKIP` on one does not help.
- **`mirror_bridge diff` refuses to record a baseline that would pass forever.** Headers in subdirectories, or any header containing the text `MIRROR_BRIDGE_SKIP`, could leave the snapshot empty, after which `--check` passed even once every class was deleted. That is now an error (exit 3) naming the cause and pointing at the generated stub as a check that cannot drift. An unknown flag is rejected (exit 1) instead of being taken as the source directory, which is what the scaffolded README's own `diff src/ --module <name>` did. `--help` now states what `diff` does and does not see.
- **Class-typed arguments are identity-checked at the Python boundary.** Every bound class shares one wrapper layout, and conversion read the wrapper out of whatever `PyObject` arrived after only a null check, so a wrong type was reinterpreted rather than rejected: passing an `int`, `str`, `dict` or `list` where a bound class was expected **segfaulted the interpreter**, and passing a *different* bound class returned a plausible number computed from that object's bytes. Both now raise `TypeError`. Conversion to a base class is also offset-adjusted, so passing a derived object where a base that is not at offset 0 is expected (`struct Swap : Observable, Priceable`) reaches the right subobject instead of returning garbage. Covered by `tests/type_safety/`.
  - The same gate now also covers the paths that reached around the conversion functions: binary operator slots (`5 + vec` entered the slot with `5` as `self` and segfaulted), the member-template descriptor (`Cls.tmpl.__get__(5)[T]()`), and `mp_subscript`/`tp_call`, which leaked `NotImplemented` to the caller instead of raising. A `shared_ptr`/`unique_ptr` to an abstract pointee reported a successful conversion while leaving the pointer empty, so the callee dereferenced null; it now fails conversion.
  - Classes with a conversion function (`explicit operator double() const`) failed the *entire module build*, because a conversion function is not an operator function and reached `identifier_of`. Same cause, same fix, in `core/mirror_bridge_plan.hpp`: one namespace-scope `operator+` ended the planner's consteval evaluation, which silently cost the module every free function and every template instantiation while still reporting success.
  - **Behaviour change**: code that relied on one bound class silently converting into an unrelated one now raises `TypeError`. Derived-to-base conversions (including virtual and non-first bases), Python subclasses, and the same class bound by two modules all keep working. Not convertible any more: a derived class whose module has not been imported, and a base C++ itself would refuse at that call site (inaccessible, or ambiguous through non-virtual multiple inheritance).
- A `noexcept` free function failed the whole module build instead of being bound: `noexcept` is part of the function type since C++17 and `FunctionTraits` had no specialization for it, so even the "skip what cannot be bound" guard could not fire (it instantiates the traits itself).
- Single-header `mirror_bridge_python.hpp` could not compile standalone: it still `#include`d `python/mirror_bridge_stubgen.hpp`, which the amalgamation never inlined. The stubgen header is now spliced in, `amalgamate.sh` fails if any local include survives, and a new `single_header_standalone` ctest compiles the header with no repo include path (#12).
- `const char*` constructor/method parameters were dereferenced as if they were pointer-holder storage, producing garbage or failing to compile — pointer-typed *values* are now distinguished from pointer-holder storage
- Virtual-override dispatch (`dispatch_python`/`has_python_override`) now acquires the GIL itself, making overridden virtuals safe to call from pure C++ threads that never held the GIL (previously undefined behavior)
- Constructor calls with arguments no constructor accepts now raise `TypeError` instead of silently returning a default-constructed object
- The Python single header shipped raw `#include` lines for `mirror_bridge_annotations.hpp` and `mirror_bridge_eigen.hpp`, so it could not compile standalone; the amalgamation now inlines both at their include site
- `from_python_pointer` accepted `None` as a "successful" null pointer that was then unconditionally dereferenced; `None` now fails conversion (TypeError / next overload)
- `T&` parameters of copyable bound classes mutated a temporary copy, silently dropping the mutation
- CMake helper functions' positional-source parsing leaked `INCLUDE_DIRS` values into the source list
- `mirror_bridge version` claimed Bloomberg clang-p2996 was required; it now reports both supported compilers and which one auto-detection selected

### Notes
- P3394 field annotations (`[[=exclude{}]]`, `[[=readonly{}]]`) remain clang-p2996 only; under GCC they are ignored (all members bound) until GCC implements P3394. The annotation-specific tests are skipped on GCC.

## [0.3.0] - 2026-06-05

### Added
- `--json` machine-readable output for `generate`, `diff`, and `doctor`: stdout carries exactly one JSON object (status, discovered classes, built outputs, and per-language errors with actionable suggestions); human-readable progress goes to stderr
- `mirror_bridge doctor` subcommand integrating the diagnostic tool into the unified CLI
- `mirror_bridge diff --check` CI gate: exits non-zero when the binding surface drifted from the committed snapshot, without ever modifying it
- CMake package support: `find_package(mirror_bridge)` and FetchContent/`add_subdirectory` consumption via the exported `mirror_bridge::mirror_bridge` target, which now carries the reflection compile flags; version compatibility file; header installs preserve directory structure; as a dependency, mirror_bridge no longer injects global compile flags into the parent project
- `AGENTS.md` operating manual for AI coding agents and `llms.txt`/`llms-full.txt` documentation bundles (generated by `tools/gen_llms_txt.sh`)
- Error catalog (`docs/reference/errors.md`): every known failure mode with exact symptom, cause, and fix
- pip-installable `mirror-bridge` package (`packaging/pip/`): wraps the Docker toolchain so `pip install mirror-bridge` gives a working `mirror_bridge` CLI anywhere, plus `mirror_bridge shell` for an interactive container
- MCP server (`pip install 'mirror-bridge[mcp]'`, run `mirror-bridge-mcp`): exposes `generate_bindings`, `doctor`, and `check_binding_drift` as native tool calls for AI agents
- Claude Code integration (`integrations/claude-code/`): drop-in `bind-cpp` skill teaching the generate → fix → verify loop
- Living benchmarks: a monthly CI workflow reruns the runtime suite and regenerates the table in `docs/internals/benchmarks.md`
- API stability contract (`docs/reference/stability.md`): which surfaces are stable pre-1.0 and how versioning works
- Smart pointer regression tests for Lua and JavaScript (members, params, returns, nil reset, abstract pointees)

### Fixed
- Class-type smart pointers never compiled in the Lua and JavaScript backends (overload declaration order defeated two-phase lookup); they now work with the same deep-copy semantics as Python
- `bool` returned/accepted as a number in Lua and JavaScript; it now maps to native booleans in both directions
- Methods taking `std::unique_ptr<T>` by value failed to compile (parameter storage tried to copy a move-only type); smart pointer parameters now use value storage and are moved into the call
- Test harness named modules after the source filename instead of the `MIRROR_BRIDGE_*MODULE` declaration, producing unloadable modules whenever the two differed (the root cause of the long-red Tests workflow)
- `generate` no longer reports success when a stale `.so` from a previous run exists but the current compile failed
- `generate --lang all` now attempts every language and reports all failures instead of aborting on the first one
- Amalgamation stripped `#define MIRROR_BRIDGE_VALIDATE(T)` from the single headers, leaving a dangling macro body that broke every single-header consumer
- Rust FFI binding generation: `generate_bindings<T>()` produces extern "C" wrappers, C headers, and safe Rust wrapper types with Drop, Send, Sync, and idiomatic getters/setters
- `mirror_bridge watch` command for live reload during development: watches headers for changes and auto-recompiles bindings
- `mirror_bridge diff` command to show binding surface changes since last build, catching accidental ABI breaks
- Bulk array transfer for numeric vectors: `vector<float>`, `vector<double>`, `vector<int>`, etc. are now returned as `array.array` objects via single memcpy (~10-50x faster than element-by-element list construction)
- String interning for member names in Python dict conversion via `PyUnicode_InternFromString`
- Exception handling for Lua (`luaL_error`) and JavaScript (`napi_throw_error`) bindings: C++ exceptions are now caught and propagated as native errors in all three languages
- Compile-time binding validation: `bind_class<T>` now produces clear `static_assert` messages when a class contains unconvertible member types, instead of cryptic template errors
- `MIRROR_BRIDGE_VALIDATE(T)` macro for explicit validation outside `bind_class`
- `std::expected<T, E>` type conversion for Python (ValueError on error), Lua (idiomatic value, err multi-return), and JavaScript (throw Error on error)
- GitHub Codespaces support (`.devcontainer/devcontainer.json`) for instant browser-based development
- `std::optional<T>` type conversion for Python, Lua, and JavaScript
- Async/await support: `std::future<T>` → Python awaitable, JavaScript Promise
- GitHub issue and PR templates for better contribution experience
- Feature matrix table in README showing per-language feature support

### Changed
- Improved devcontainer configuration with proper C++2c reflection flags

## [0.2.0] - 2025-12-01

### Added
- P3394 annotation support for field-level binding control (`[[=exclude{}]]`, `[[=readonly{}]]`)
- Zero-copy buffer protocol for NumPy integration (10M times faster for large data)
- `.pyi` stub generation for Python IDE autocomplete
- Multi-language Mandelbrot demo showcasing Python, Lua, and JavaScript bindings
- `mirror_bridge_doctor` diagnostic tool
- Examples 06-10: callbacks, static/constexpr, free functions, enums, smart pointers
- Migration guide from pybind11
- Comprehensive architecture documentation

### Changed
- Replaced method name mangling with pybind11-style overload dispatch
- Improved method parameter forwarding for move-only and reference types

### Fixed
- GCC compatibility for P3394 annotations
- V8 SetNativeDataProperty API compatibility
- Fold expression empty pack handling

## [0.1.0] - 2025-11-12

### Added
- Initial release with C++26 reflection-based binding generation
- Python bindings via Python C API
- Lua bindings via Lua C API
- JavaScript bindings via Node.js N-API
- Auto-discovery of classes for zero-boilerplate binding
- Container support: `std::vector`, `std::array`
- Smart pointer support: `std::unique_ptr`, `std::shared_ptr`
- Enum support with automatic integer conversion
- Nested object support with dict/table/object conversion
- Method overloading with automatic dispatch
- Static method and constexpr member binding
- Free function binding
- Precompiled header support for 3-6x faster builds
- CLI tools: `mirror_bridge`, `mirror_bridge_auto`, `mirror_bridge_pch_build`
- Docker environment with Bloomberg clang-p2996
- Comprehensive test suite for all languages
- Production-quality examples (image processing, n-body simulation)

### Performance
- 3-5x faster runtime than pybind11
- Zero runtime overhead through compile-time binding generation
- Batch processing API for avoiding Python/C++ boundary overhead

[Unreleased]: https://github.com/FranciscoThiesen/mirror_bridge/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/FranciscoThiesen/mirror_bridge/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/FranciscoThiesen/mirror_bridge/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/FranciscoThiesen/mirror_bridge/releases/tag/v0.1.0

