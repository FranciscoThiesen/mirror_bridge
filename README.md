# Mirror Bridge

**Write C++ once, bind everywhere — automatically.**

[![Tests](https://github.com/FranciscoThiesen/mirror_bridge/actions/workflows/test.yml/badge.svg?branch=main)](https://github.com/FranciscoThiesen/mirror_bridge/actions/workflows/test.yml)
[![Release](https://img.shields.io/github/v/release/FranciscoThiesen/mirror_bridge)](https://github.com/FranciscoThiesen/mirror_bridge/releases)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)

Generate Python, Lua, and JavaScript bindings from C++ using C++26 reflection. Zero boilerplate, zero runtime overhead, zero binding code.

![mirror_bridge demo: one command generates Python, Lua and JavaScript modules](docs/assets/mirror_bridge_demo.svg)

> C++26 reflection ships in **stock GCC 16.1+** (`g++ -std=c++26 -freflection`); [Bloomberg's clang-p2996](https://github.com/bloomberg/clang-p2996) also works. The CLI auto-detects whichever is present. Reflection was voted into C++26 in June 2025 — this is standard C++, not a fork-only experiment.

## One C++ Class, Three Languages

```cpp
struct Calculator {
    double value = 0.0;
    double add(double x) { return value += x; }
    double subtract(double x) { return value -= x; }
};
```

```bash
mirror_bridge generate src/ --module calc --lang all
```

That command is the entire binding process — no `.def()`, no macros in your class, no wrapper files. The same `Calculator` is now native in all three languages:

**Python**
```python
import calc
c = calc.Calculator()
c.add(10)
print(c.value)   # 10.0
```

**Lua**
```lua
package.cpath = "build/lua/?.so;" .. package.cpath
local calc = require("calc")
local c = calc.Calculator()
c:add(10)
print(c.value)   -- 10.0
```

**JavaScript (Node.js)**
```javascript
const calc = require('./build/calc.node');
const c = new calc.Calculator();
c.add(10);
console.log(c.value);   // 10.0
```

## Proof on a Real Library: Open3D

We ported [Open3D](https://github.com/isl-org/Open3D)'s geometry module — the same API surface that takes **25,262 lines of hand-written pybind11** upstream — with **71 generated lines and one command** ([full case study](https://chico.dev/Mirror-Bridge-Open3D-71-Lines/), [runnable example](examples/open3d-port/)):

```bash
cd examples/open3d-port
mirror_bridge generate src/ --module open3d_geometry --lang all -I /usr/include/eigen3
```

And it's not a slower convenience layer: on the identical C++ compiled with identical flags, ingesting a 1M-point cloud from Python (`list of [x,y,z]` → `std::vector<Eigen::Vector3d>`) runs **42x faster than the pybind11 binding** (12.2ms vs 509ms), because the reflection-generated converter is specialized at compile time instead of dispatching through generic type casters ([fair-comparison methodology](asm_study/)).

## Try It Now

No installation required — run in your browser or with Docker:

**[Interactive Playground](docs/playground.md)** - Godbolt examples showing C++26 reflection in action

**GitHub Codespaces** (browser-based, ~2 min):

[![Open in GitHub Codespaces](https://github.com/codespaces/badge.svg)](https://codespaces.new/FranciscoThiesen/mirror_bridge)

**Docker** (local, one command):

```bash
docker run -it --rm -v $(pwd):/workspace ghcr.io/franciscothiesen/mirror_bridge:latest
```

Then inside the container:
```bash
cd examples/01-hello-world
../../tools/mirror_bridge generate src/ --module greeter --lang python
python3 test_greeter.py
```

## Quick Start

```bash
# 1. Get the environment
./start_dev_container.sh  # Choose option 1 for pre-built image

# 2. Verify
./tests/run_all_tests.sh

# 3. Try an example
cd examples/01-hello-world
../../tools/mirror_bridge generate src/ --module greeter --lang python
python3 test_greeter.py
```

**[Full Quick Start Guide →](docs/getting-started/quickstart.md)**

## Features

### Language Support

| Language | Status | Auto-Discovery |
|----------|--------|----------------|
| Python | Stable | `--lang python` |
| Lua | Stable | `--lang lua` |
| JavaScript (Node.js) | Stable | `--lang js` |
| Rust | Beta | Code generation |

### Feature Matrix

| Feature | Python | Lua | JavaScript |
|---------|:------:|:---:|:----------:|
| **Data Members** | ✅ | ✅ | ✅ |
| **Methods (any arity)** | ✅ | ✅ | ✅ |
| **Constructors** | ✅ | ✅ | ✅ |
| **Containers (vector/array)** | ✅ | ✅ | ✅ |
| **Maps (map/unordered_map)** | ✅ | ✅ | ✅ |
| **Sets (set/unordered_set)** | ✅ | ✅ | ✅ |
| **Tuples (tuple/pair)** | ✅ | ✅ | ✅ |
| **Variants (std::variant)** | ✅ | ✅ | ✅ |
| **Nested Objects** | ✅ | ✅ | ✅ |
| **Enums** | ✅ | ✅ | ✅ |
| **Inheritance** | ✅ | ✅ | ✅ |
| **Method Overloading** | ✅ | ⚠️ | ⚠️ |
| **Smart Pointers** | ✅ | ✅ | ✅ |
| **Exception Handling** | ✅ | ✅ | ✅ |
| **Cross-Module Types** | ✅ | ❌ | ❌ |
| **Zero-Copy Buffers** | ✅ | ❌ | ❌ |
| **Async/Await** | ✅ | ❌ | ✅ |
| **std::optional** | ✅ | ✅ | ✅ |
| **std::expected** | ✅ | ✅ | ✅ |

✅ Full support  ⚠️ Partial support  ❌ Not supported

### Additional Features

- **Precompiled headers**: 3-6x faster builds
- **Auto-discovery**: Zero binding code required
- **Type stubs**: `.pyi` generation for Python IDE support

## Documentation

| Topic | Description |
|-------|-------------|
| **[Getting Started](docs/getting-started/)** | Quick start, installation, first binding |
| **[Guides](docs/guides/)** | Workflow, PCH, multi-language, contributing |
| **[Reference](docs/reference/)** | CLI, API, configuration, type conversion |
| **[Architecture](docs/internals/architecture.md)** | System design and internals |
| **[Examples](examples/)** | Progressive examples from hello-world to production |

## CLI Usage

```bash
# Auto-discover and compile
mirror_bridge generate src/ --module my_mod --lang python

# With precompiled header (faster)
mirror_bridge pch --output build/ --type release
mirror_bridge generate src/ --module my_mod --lang python --pch

# Multiple languages
mirror_bridge generate src/ --module my_mod --lang all
```

**[Full CLI Reference →](docs/reference/cli.md)**

## Performance

From the CI-regenerated benchmark table (same C++ class, same compiler, `-O3` for every framework; only the binding layer differs — absolute times shift with runner hardware, compare the ratios):

| Operation | Mirror Bridge | pybind11 | nanobind | vs pybind11 |
|-----------|--------------|----------|----------|-------------|
| **Method dispatch** (null call) | 73ns | 190ns | 70ns | **2.6x faster** |
| **Object construction** | 89ns | 376ns | 111ns | **4.2x faster** |
| **Vector return** | 316ns | 914ns | 2,650ns | **2.9x faster** (8.4x vs nanobind) |
| **Point-cloud ingest** (1M × `[x,y,z]` → `vector<Eigen::Vector3d>`) | 12.2ms | 509ms | — | **42x faster** |
| **Binding code** | 0 lines | 18+ lines/class | 18+ lines/class | **Zero boilerplate** |

Honest summary: 2-4x faster than pybind11 across the board, parity with nanobind on scalar dispatch (nanobind wins some by ~10%), and far ahead of both on container traffic. Compile time is ~3.4x faster than pybind11 (567ms vs 1,938ms, no PCH for either); nanobind compiles faster still — Mirror Bridge only matches it with `--pch` (194ms). The full table is **[regenerated monthly by CI](docs/internals/benchmarks.md)** so it tracks the current code, with methodology and SWIG numbers included.

## For AI Agents

mirror_bridge is built to be driven by coding agents as well as humans:

- **[AGENTS.md](AGENTS.md)**: operating manual for agents working in this repo
- **`--json` everywhere**: `generate`, `diff`, and `doctor` emit exactly one JSON object on stdout, with actionable suggestions on failure
- **MCP server**: gives any MCP-capable agent `generate_bindings`, `doctor`, and `check_binding_drift` as tool calls. Until the PyPI release lands, build it locally (`cd packaging/pip && ./build_wheel.sh`, then `pip install "$(ls dist/*.whl)[mcp]"` and `claude mcp add mirror_bridge -- mirror-bridge-mcp`)
- **[Claude Code skill](integrations/claude-code/)**: drop-in `bind-cpp` skill teaching the full generate → fix → verify loop
- **[llms.txt](llms.txt)** / **[llms-full.txt](llms-full.txt)**: docs index and full corpus for LLM consumption

## Project Structure

```
mirror_bridge/
├── core/                    # Language-agnostic reflection
├── python/                  # Python C API bindings
├── lua/                     # Lua C API bindings
├── javascript/              # Node.js N-API bindings
├── tools/mirror_bridge      # Unified CLI
├── docs/                    # Documentation
├── examples/                # Progressive examples
└── tests/                   # Test suite
```

## Requirements

- **Compiler**: a C++26 reflection compiler — stock **GCC 16+** (`-freflection`) or **Bloomberg clang-p2996**. Both are provided via Docker; the CLI auto-detects which is installed (override with `MB_CXX`).
- **Python**: 3.7+ | **Lua**: 5.4 | **Node.js**: 14+

> **Note**: P3394 field annotations (`[[=exclude{}]]`, `[[=readonly{}]]`) currently require clang-p2996; GCC builds ignore them (all members bound) until GCC ships P3394.

## Contributing

See **[Contributing Guide](docs/guides/contributing.md)** for development setup and guidelines.

## License

Apache License 2.0 — See [LICENSE](LICENSE)

---

**[Documentation](docs/)** · **[Examples](examples/)** · **[Issues](https://github.com/FranciscoThiesen/mirror_bridge/issues)**
