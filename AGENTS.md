# AGENTS.md

`mirror_bridge` is a C++26 reflection-based binding generator: it inspects C++ structs/classes at
compile time (via P2996 reflection) and emits zero-overhead Python, Lua, and JavaScript bindings —
no hand-written binding code. **The one critical constraint: it requires Bloomberg's
[clang-p2996](https://github.com/bloomberg/clang-p2996) with libc++ and the `<meta>` header. Nothing
here compiles with a stock host compiler.** All binding compilation MUST happen inside the project's
Docker container (or a GitHub Codespace). Never attempt to compile a binding with the host's
`clang++`/`g++` — it will fail with "no member named 'meta'" or unknown `-freflection`. Editing,
searching, and reading code on the host is fine; compiling is not.

## Environment setup

The reflection compiler lives in the Docker image. Prefer pulling the pre-built image over building
from source (~45–90 min).

```bash
# Pull + tag the pre-built image (interactive helper, choose option 1)
./start_dev_container.sh

# Or pull directly
docker pull ghcr.io/franciscothiesen/mirror_bridge:latest
docker tag  ghcr.io/franciscothiesen/mirror_bridge:latest mirror_bridge:latest
```

`start_dev_container.sh` creates a persistent container named `mirror_bridge_dev` with the repo
mounted at `/workspace` (host edits are immediately visible inside, and vice versa).

To run a command inside the container **non-interactively** (scriptable for an agent):

```bash
# One-off, throwaway container with the repo mounted:
docker run --rm -v "$(pwd):/workspace" -w /workspace mirror_bridge:latest \
    bash -lc './tests/run_all_tests.sh'

# Or exec into the long-lived dev container if it's already running:
docker exec -w /workspace mirror_bridge_dev bash -lc 'tools/mirror_bridge version'
```

If a binary can't find libc++ at runtime, set `LD_LIBRARY_PATH=/usr/local/lib` (see failure modes).

## Common tasks

The CLI is the bash script `tools/mirror_bridge` (run it as `tools/mirror_bridge` or, from inside an
example dir, `../../tools/mirror_bridge`). All commands below assume you are inside the container.

**Generate bindings from headers (auto-discovery):**

```bash
# Single language
tools/mirror_bridge generate src/ --module my_module --lang python

# Choices for --lang: python | lua | js | all
tools/mirror_bridge generate src/ --module my_module --lang all

# Useful flags: -o/--output DIR (default build/), -k/--keep-generated,
# -v/--verbose, -f/--force, -I DIR (extra include dir), --pch [PATH]
tools/mirror_bridge generate src/ --module my_module --lang python -I include/ -v
```

`generate` scans every `.hpp`/`.h` under `src_dir`, binds all discovered struct/class definitions,
emits a `<module>_<lang>_binding.cpp`, compiles it, and (unless `-k`) deletes the generated `.cpp`.

**Build a precompiled header (3–6x faster builds):**

```bash
tools/mirror_bridge pch --output build/ --type release   # or --type debug
tools/mirror_bridge generate src/ --module my_module --lang python --pch   # auto-detects build/*.gch
```

**Build a hand-written binding file:**

```bash
tools/mirror_bridge build my_bindings.cpp --lang python -I src/ -o build/
```

**Run the full test suite:**

```bash
./tests/run_all_tests.sh
```

It builds every `*.cpp` binding under `tests/` and runs all `test_*.py`, `test_*.js`, `test_*.lua`,
and `test_*.sh`. V8 tests are non-blocking. The script sets `LD_LIBRARY_PATH`, `PYTHONPATH`, and
`LUA_CPATH` for you.

**Run a single test** (no per-test target exists; do it manually inside the container):

```bash
# 1. Build the binding the test imports (Python example):
clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -I. -Itests/<dir> -fPIC -shared $(python3-config --includes --ldflags) \
    tests/<dir>/<module>_binding.cpp -o build/<module>.so
# 2. Run it:
LD_LIBRARY_PATH=/usr/local/lib PYTHONPATH=build python3 tests/<dir>/test_<name>.py
```

**Regenerate after header changes / inspect the binding surface:**

```bash
tools/mirror_bridge generate src/ --module my_module --lang python -f   # force rebuild
tools/mirror_bridge diff src/                 # show added/removed members vs. last snapshot
tools/mirror_bridge diff src/ --update        # update snapshot non-interactively (CI-friendly)
tools/mirror_bridge watch src/ --module my_module --lang python   # poll + auto-recompile
```

**One-off compile of a binding (the canonical command the CLI runs under the hood):**

```bash
# Python:
clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -I. -I<src_dir> -I<project_root> -fPIC -shared -O3 -DNDEBUG \
    -Wno-deprecated-declarations $(python3-config --includes) \
    binding.cpp -o build/my_module.so $(python3-config --ldflags)

# Lua:  add  -I/usr/include/lua5.4 -llua5.4   and output .so
# JS:   add  -I/usr/include/node               and output .node
```

The four flags that must always be present: `-std=c++2c -freflection -freflection-latest
-stdlib=libc++`. Include `mirror_bridge.hpp` (Python), `lua/mirror_bridge_lua.hpp`, or
`javascript/mirror_bridge_javascript.hpp` from the binding source.

## Controlling what gets bound

Auto-discovery binds **every** struct/class found in the scanned headers by default. Opt out via:

- `// MIRROR_BRIDGE_SKIP` on the line **immediately before** a class → skips that class.
- `// MIRROR_BRIDGE_SKIP_FILE` anywhere in a header → skips the whole file.
- P3394 field annotations (require `-freflection-latest`; ignored on compilers without P3394):

```cpp
#include "python/mirror_bridge_annotations.hpp"
using mirror_bridge::exclude;
using mirror_bridge::readonly;

struct UserProfile {
    int user_id;                               // bound read/write
    [[=exclude{}]]  std::string password_hash; // not bound at all
    [[=readonly{}]] std::string created_at;    // bound read-only
};
```

Template classes are never auto-bound (the discovery parser skips them); instantiate them with a
`using` alias first, then bind the alias.

## When things fail

- `static_assert failed: "<T> contains members with types that mirror_bridge cannot convert"` →
  a member has an unsupported type. Mark it `[[=exclude{}]]`, or add a custom converter. Supported:
  arithmetic, `std::string`/`string_view`/`const char*`, containers, smart pointers,
  `std::optional`, `std::expected`, enums, nested bindable classes.
- `no member named 'meta' in namespace 'std'` / unknown `-freflection*` → wrong compiler. You're on
  the host, not in the container. Use the Docker image.
- `libc++.so.1: cannot open shared object file` → run with `LD_LIBRARY_PATH=/usr/local/lib`
  (the test runner also uses the arch-specific `/usr/local/lib/<arch>-unknown-linux-gnu` paths).
- libstdc++ ABI symbols (`std::__cxx11::*`) unresolved when linking/loading against an external C++
  library built with libstdc++ → preload it: `LD_PRELOAD=libstdc++.so.6 python3 your_test.py`.
  libc++ (`std::__1::`) and libstdc++ (`std::__cxx11::`) use distinct symbol namespaces and coexist
  in one process. See `examples/open3d-runtime/build_and_test.sh`.

## Repo conventions

From `CLAUDE.md` — follow these exactly:

- **Authorship**: NEVER add yourself (the agent) as a commit author or `Co-Authored-By` trailer.
  You are assisting, not authoring.
- **Commit cadence**: commit every meaningful, self-contained set of changes.
- **Validate locally**: test changes inside the container before relying on CI — keeps the loop tight.
- **Comments**: do NOT add comments that restate self-documenting code or merely repeat a function
  name. DO add insightful comments for non-obvious / convoluted logic.
- **Style/philosophy**: C++26, concept-driven design (see the `Arithmetic`/`StringLike`/`Container`/
  `SmartPointer`/`EnumType`/`Bindable`/`NestedBindable` concepts in `core/mirror_bridge_core.hpp`),
  simplicity over cleverness, zero runtime overhead (all binding logic resolved at compile time).
- **License**: Apache 2.0.

## Repo map

| Path | Contents |
|------|----------|
| `core/` | Language-agnostic reflection core (`mirror_bridge_core.hpp`): type concepts, validation, registry. |
| `python/` | Python C-API binding headers: `mirror_bridge_python.hpp`, annotations, options, stubgen, buffer, eigen. |
| `lua/` | Lua 5.4 C-API binding headers (`mirror_bridge_lua.hpp`, PCH header). |
| `javascript/` | Node N-API + embedded V8 binding headers (`mirror_bridge_javascript.hpp`, `mirror_bridge_v8.hpp`). |
| `rust/` | Rust codegen header (`mirror_bridge_rust.hpp`), beta. |
| `single_header/` | Amalgamated single-header builds of each language backend. |
| `tools/` | The `mirror_bridge` CLI (bash). The only user-facing entry point. |
| `tests/` | Test suite; `run_all_tests.sh` is the entry point. Per-language subdirs + shell CLI tests. |
| `examples/` | Progressive examples `01-hello-world` … `11-expected`, plus `open3d-*` real-world ports. |
| `docs/` | Onboarding (`getting-started/`), `guides/`, `reference/` (cli, type-conversion), `internals/`. |
| `benchmarks/` | Performance + compile-time benchmarks vs. pybind11/nanobind/swig. |
| `scripts/` | Python helpers: `discover_symbols.py`, `generate_stubs.py`. |
| `mirror_bridge.hpp` | Top-level umbrella header for Python bindings (included by generated code). |
| `mirror_bridge_pch.hpp` | Header compiled into the precompiled header by `mirror_bridge pch`. |
| `start_dev_container.sh` | Pull/build image, create/attach the persistent `mirror_bridge_dev` container. |
