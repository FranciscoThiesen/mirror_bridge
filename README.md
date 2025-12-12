# Mirror Bridge

**C++ to Multi-Language Bindings via C++26 Reflection — Zero Boilerplate**

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)

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

| Feature | Status |
|---------|--------|
| Python bindings | Stable |
| Lua bindings | Stable |
| JavaScript (Node.js) | Stable |
| Auto-discovery | `mirror_bridge generate src/` |
| Precompiled headers | 3-6x faster builds |

**Automatically Bound:**
- Data members (properties)
- Methods (any parameters)
- Constructors
- Containers (`vector`, `array`)
- Nested objects
- Smart pointers
- Enums
- Exceptions

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
