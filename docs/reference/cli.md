# CLI Reference

Mirror Bridge provides a unified command-line interface for all binding operations.

## Overview

```bash
mirror_bridge <command> [options]
```

## Commands

### `generate` - Generate Bindings

Auto-discover and compile bindings from C++ headers.

```bash
mirror_bridge generate <src_dir> --module <name> --lang <language> [options]
```

**Required Arguments:**

| Argument | Description |
|----------|-------------|
| `<src_dir>` | Directory containing C++ headers |
| `--module NAME` | Output module name |
| `--lang LANG` | Target language: `python`, `lua`, `js`, or `all` |

**Options:**

| Option | Description |
|--------|-------------|
| `-o, --output DIR` | Output directory (default: `build/`) |
| `-k, --keep-generated` | Keep generated binding `.cpp` file |
| `-v, --verbose` | Show compilation output |
| `--pch [PATH]` | Use precompiled header (auto-detect or specify path) |
| `-f, --force` | Force rebuild even if sources haven't changed |
| `-I DIR` | Add include directory |

**Examples:**

```bash
# Basic Python binding
mirror_bridge generate src/ --module my_module --lang python

# Lua with verbose output
mirror_bridge generate src/ --module my_module --lang lua -v

# JavaScript with custom output directory
mirror_bridge generate src/ --module my_module --lang js -o dist/

# All languages at once
mirror_bridge generate src/ --module my_module --lang all

# With precompiled header (auto-detect)
mirror_bridge generate src/ --module my_module --lang python --pch

# With explicit PCH path
mirror_bridge generate src/ --module my_module --lang python --pch build/pch.gch

# Force rebuild
mirror_bridge generate src/ --module my_module --lang python -f
```

### `pch` - Build Precompiled Header

Create a precompiled header for faster compilation.

```bash
mirror_bridge pch --output <dir> --type <type> [options]
```

**Required Arguments:**

| Argument | Description |
|----------|-------------|
| `--output DIR` | Output directory for PCH file |
| `--type TYPE` | Build type: `release` or `debug` |

**Options:**

| Option | Description |
|--------|-------------|
| `--lang LANG` | Target language (default: `python`) |
| `-v, --verbose` | Show compilation output |

**Examples:**

```bash
# Release PCH for Python
mirror_bridge pch --output build/ --type release

# Debug PCH
mirror_bridge pch --output build/ --type debug

# PCH for Lua
mirror_bridge pch --output build/ --type release --lang lua
```

### `build` - Manual Compilation

Compile a manually-written binding file.

```bash
mirror_bridge build <binding.cpp> [options]
```

**Required Arguments:**

| Argument | Description |
|----------|-------------|
| `<binding.cpp>` | Binding source file |

**Options:**

| Option | Description |
|--------|-------------|
| `-o, --output DIR` | Output directory (default: `build/`) |
| `-I DIR` | Add include directory |
| `-v, --verbose` | Show compilation output |
| `--pch [PATH]` | Use precompiled header |
| `--lang LANG` | Target language (default: `python`) |

**Examples:**

```bash
# Basic compilation
mirror_bridge build my_bindings.cpp

# With include directories
mirror_bridge build my_bindings.cpp -I src/ -I include/

# With PCH
mirror_bridge build my_bindings.cpp --pch build/pch.gch
```

### `config` - Config File Generation

Generate bindings from a configuration file.

```bash
mirror_bridge config <config.mirror> [options]
```

**Required Arguments:**

| Argument | Description |
|----------|-------------|
| `<config.mirror>` | Configuration file |

**Options:**

| Option | Description |
|--------|-------------|
| `-o, --output DIR` | Output directory (default: `build/`) |
| `-k, --keep-generated` | Keep generated binding file |
| `-v, --verbose` | Show compilation output |
| `--pch [PATH]` | Use precompiled header |

**Config File Format:**

```
# Module name (required)
module: my_module

# Include directories (optional, comma-separated)
include_dirs: src/, include/

# Class bindings
# Format: ClassName: header_file [as PythonName]
Calculator: calculator.hpp
Vector3: vector3.hpp
math::Complex: complex.hpp as Complex
```

**Examples:**

```bash
# Generate from config
mirror_bridge config bindings.mirror

# With verbose output
mirror_bridge config bindings.mirror -v
```

### `diff` - Binding Surface Diff

Compare current class definitions against a stored snapshot to detect added/removed members and methods.

```bash
mirror_bridge diff <src_dir> [options]
```

**Required Arguments:**

| Argument | Description |
|----------|-------------|
| `<src_dir>` | Directory containing C++ headers |

**Options:**

| Option | Description |
|--------|-------------|
| `-o, --output DIR` | Build directory for storing snapshots (default: `build/`) |
| `-u, --update` | Update the snapshot without prompting (CI-friendly) |

The first run creates an initial snapshot. Subsequent runs compare against it and show added/removed binding surface elements.

**Examples:**

```bash
# Check for changes
mirror_bridge diff src/

# Non-interactive update (for CI pipelines)
mirror_bridge diff src/ --update

# Custom snapshot location
mirror_bridge diff src/ --output build/
```

### `watch` - Live Reload

Watch header files for changes and automatically recompile bindings.

```bash
mirror_bridge watch <src_dir> --module <name> [options]
```

**Required Arguments:**

| Argument | Description |
|----------|-------------|
| `<src_dir>` | Directory containing C++ headers |
| `--module NAME` | Output module name |

**Options:**

| Option | Description |
|--------|-------------|
| `--lang LANG` | Target language (default: `python`) |
| `-o, --output DIR` | Output directory (default: `build/`) |
| `-i, --interval SEC` | Poll interval in seconds (default: `2`) |
| `--pch` | Use precompiled header for faster builds |

**Examples:**

```bash
# Watch and rebuild Python bindings on changes
mirror_bridge watch src/ --module my_lib --lang python

# All languages with faster polling
mirror_bridge watch src/ --module my_lib --lang all --interval 1

# With precompiled header for fast iteration
mirror_bridge watch src/ --module my_lib --lang python --pch
```

### `init` - Initialize Project

Create a new Mirror Bridge project structure.

```bash
mirror_bridge init [dir] [options]
```

**Arguments:**

| Argument | Description |
|----------|-------------|
| `[dir]` | Project directory (default: current directory) |

**Options:**

| Option | Description |
|--------|-------------|
| `--name NAME` | Module name |
| `--lang LANG` | Target language(s) |

**Examples:**

```bash
# Initialize in current directory
mirror_bridge init --name my_module

# Initialize in new directory
mirror_bridge init my_project --name my_module --lang all
```

## Global Options

These options work with all commands:

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help message |
| `--version` | Show version information |

## Auto-Discovery

When using `generate`, Mirror Bridge scans all `.hpp` and `.h` files in the source directory and automatically binds all `struct` and `class` definitions found.

### Opt-Out Mechanism

Skip specific classes:
```cpp
// MIRROR_BRIDGE_SKIP
struct InternalClass { /* won't be bound */ };
```

Skip entire files:
```cpp
// MIRROR_BRIDGE_SKIP_FILE
// At the top of a header file
```

## Change Detection

By default, `generate` tracks source file changes and only recompiles when header files have been modified since the last build. Use `--force` to bypass this check.

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Error (invalid arguments, compilation failure, etc.) |

## Environment Variables

| Variable | Description |
|----------|-------------|
| `MIRROR_BRIDGE_PCH_DIR` | Default PCH directory |
| `MIRROR_BRIDGE_OUTPUT_DIR` | Default output directory |
| `CXX` | C++ compiler to use |
