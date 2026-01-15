# Mirror Bridge Documentation

Welcome to the Mirror Bridge documentation. This guide covers everything from getting started to advanced usage patterns.

## Quick Navigation

### Try It Now
- **[Interactive Playground](playground.md)** - Godbolt examples, Codespaces, Docker

### Getting Started
- **[Quick Start](getting-started/quickstart.md)** - Get running in 5 minutes
- **[Installation](getting-started/installation.md)** - Detailed setup instructions
- **[First Binding](getting-started/first-binding.md)** - Create your first C++ binding

### Guides
- **[Workflow Guide](guides/workflow.md)** - Recommended development workflow with PCH
- **[PCH Optimization](guides/pch-optimization.md)** - 3-6x faster compilation with precompiled headers
- **[Multi-Language Support](guides/multi-language.md)** - Python, Lua, and JavaScript bindings
- **[Single-Header Distribution](guides/single-header.md)** - Simplified integration with amalgamated headers
- **[Contributing](guides/contributing.md)** - Development guide and contribution workflow

### Reference
- **[CLI Reference](reference/cli.md)** - Command-line tool documentation
- **[API Reference](reference/api.md)** - C++ API and concepts
- **[Configuration](reference/configuration.md)** - Config file format and options
- **[Type Conversion](reference/type-conversion.md)** - C++ to language type mappings

### Internals
- **[Architecture](internals/architecture.md)** - System design and component overview
- **[Features Matrix](internals/features.md)** - Verified features across all languages
- **[Benchmarks](internals/benchmarks.md)** - Performance analysis and results

## Overview

Mirror Bridge is a **header-only C++ library** that uses C++26 reflection (P2996) to automatically generate bindings for:

| Language | Status | Auto-Discovery Tool |
|----------|--------|---------------------|
| Python   | Stable | `mirror_bridge generate --lang python` |
| Lua      | Stable | `mirror_bridge generate --lang lua` |
| JavaScript (Node.js) | Stable | `mirror_bridge generate --lang js` |

### Key Features

- **Zero Boilerplate** - No manual binding code required
- **Multi-Language** - Single C++ codebase, multiple language bindings
- **Compile-Time** - All binding code generated at compile-time
- **Auto-Discovery** - Automatically finds and binds all classes
- **Type Safety** - Full type conversion with compile-time checks

### Supported C++ Features

| Feature | Status |
|---------|--------|
| Data members | Automatic getters/setters |
| Methods (any parameters) | Full variadic support |
| Constructors | Default and parameterized |
| Method overloading | Automatic name mangling |
| Smart pointers | `unique_ptr`, `shared_ptr` |
| Containers | `vector`, `array`, etc. |
| Nested classes | Recursive handling |
| Enums | Automatic conversion |
| Inheritance | Reflected from base classes |
| Exceptions | Language-native exceptions |

## Requirements

- **Compiler**: Bloomberg clang-p2996 (C++26 reflection)
- **Python**: 3.7+ (for Python bindings)
- **Lua**: 5.4 (for Lua bindings)
- **Node.js**: 14+ (for JavaScript bindings)
- **Platform**: Linux, macOS (via Docker)

## Getting Help

- **Issues**: [GitHub Issues](https://github.com/FranciscoThiesen/mirror_bridge/issues)
- **Discussions**: [GitHub Discussions](https://github.com/FranciscoThiesen/mirror_bridge/discussions)

## License

Apache License 2.0 - See [LICENSE](../LICENSE) for details.
