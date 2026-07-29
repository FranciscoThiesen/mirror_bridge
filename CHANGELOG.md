# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
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

### Fixed
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

