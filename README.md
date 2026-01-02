# Mirror Bridge

**Write C++ once, bind everywhere — automatically.**

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
[![Open in GitHub Codespaces](https://img.shields.io/badge/Codespaces-Open-blue?logo=github)](https://codespaces.new/FranciscoThiesen/mirror_bridge)

Generate Python, Lua, and JavaScript bindings from C++ using C++26 reflection. Zero boilerplate, zero runtime overhead, zero binding code.

> **Experimental**: Requires C++26 reflection (P2996) via [Bloomberg's clang-p2996](https://github.com/bloomberg/clang-p2996).

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

**Python** | **Lua** | **JavaScript**
```python
import calc
c = calc.Calculator()
c.add(10)
print(c.value)  # 10.0
```

## Try It Now

No installation required — run in your browser or with Docker:

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
| **Smart Pointers** | ✅ | ⚠️ | ⚠️ |
| **Exception Handling** | ✅ | ❌ | ❌ |
| **Cross-Module Types** | ✅ | ❌ | ❌ |
| **Zero-Copy Buffers** | ✅ | ❌ | ❌ |
| **Async/Await** | ✅ | ❌ | ✅ |
| **std::optional** | ✅ | ✅ | ✅ |

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

| Metric | Mirror Bridge | Traditional |
|--------|--------------|-------------|
| Binding code | **0 lines** | 18+ lines/class |
| Compile time (with PCH) | ~250ms | ~200ms |
| Runtime overhead | **Zero** | Minimal |

**[Benchmark Details →](docs/internals/benchmarks.md)**

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

- **Compiler**: Bloomberg clang-p2996 (provided via Docker)
- **Python**: 3.7+ | **Lua**: 5.4 | **Node.js**: 14+

## Contributing

See **[Contributing Guide](docs/guides/contributing.md)** for development setup and guidelines.

## License

Apache License 2.0 — See [LICENSE](LICENSE)

---

**[Documentation](docs/)** · **[Examples](examples/)** · **[Issues](https://github.com/FranciscoThiesen/mirror_bridge/issues)**
