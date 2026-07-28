# Roadmap

Where Mirror Bridge is heading. Items are ordered by intent, not promise —
this is a small project and the fastest way to move something up is to
[contribute](CONTRIBUTING.md) or open an issue making the case.

## Now (active)

- **Free-threaded Python, safe by default.** Free-threading is officially
  supported since Python 3.14, and one unported extension silently re-enables
  the GIL for the whole process. Reflection lets us do what runtime binding
  libraries cannot: classify each bound method (const vs mutating) at compile
  time, release the GIL around eligible C++ calls automatically, and declare
  `Py_MOD_GIL_NOT_USED` on free-threaded builds.
- **Exact `.pyi` stubs from reflection.** Stub generators that parse runtime
  objects are fragile by their own maintainers' admission. Reflection knows
  every signature, type, and default — stubs should be complete and
  mypy-clean with zero parsing.
- **Header-to-wheel scaffolding.** `mirror_bridge init --wheel`: from bare
  C++ headers to a pip-installable, cibuildwheel-ready repository
  (pyproject.toml + scikit-build-core + CI matrix) in one command.
- **PyPI release** of the `mirror-bridge` CLI wrapper and MCP server.

## Next

- **Lua/JS parity pushes**: full method overloading, cross-module types, and
  wiring Lua/JS runtime numbers into the monthly living-benchmarks loop.
- **First external adopter**: land a real library port (in progress; see
  `examples/open3d-port/` for the methodology).
- **Compile-time work**: close the no-PCH gap to nanobind (567ms vs 165ms on
  the simple benchmark today; PCH already reaches 194ms).
- **Discovery robustness**: replace the regex/brace scanner with a
  reflection-driven or libclang-backed discovery pass for gnarly headers.

## Later / exploratory

- **Rust backend maturation** — today it's a prototype (zero-parameter
  methods only); the goal is a `-sys` crate + safe wrapper generation.
- **WASM target** sharing the JS backend's conversion layer.
- **Reflection-driven serialization** (JSON/MessagePack) from the same
  `bind_class` surface.
- **Hotspot scout**: profile a Python codebase, identify C++-worthy
  functions, and scaffold the port + binding automatically.

## Non-goals (for now)

- Supporting pre-reflection compilers (C++17/20). That's pybind11/nanobind's
  ground and they're excellent at it; Mirror Bridge exists to show what the
  post-reflection world looks like.
- Windows/MSVC support before MSVC ships P2996.

## Recently shipped

See [CHANGELOG.md](CHANGELOG.md). Highlights: stock GCC 16.1 support,
by-reference argument passing, buffer-protocol bulk ingest (42x pybind11 on
point-cloud ingest), `mirror_bridge init`, installable CMake helper API,
`ctest` as a trustworthy signal.
