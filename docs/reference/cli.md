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
| `--json` | Emit one JSON result object on stdout (see [Machine-readable output](#machine-readable-output)) |
| `--instantiate SPEC` | Python: also bind this template specialization, e.g. `"geom::Vector3<short>"` or `"geom::clamp<float>"` (repeatable) |
| `--template-cap N` | Python: max argument combinations tried per function template before falling back to Python scalars (default 64) |
| `--no-templates` | Python: skip template and free-function planning, bind classes only |

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
| `--check` | Never modify the snapshot; exit `1` on drift, `2` if no snapshot exists |
| `--json` | Emit one JSON result object on stdout |

The first run creates an initial snapshot. Subsequent runs compare against it and show added/removed binding surface elements.

**Examples:**

```bash
# Check for changes
mirror_bridge diff src/

# Non-interactive update (for CI pipelines)
mirror_bridge diff src/ --update

# CI gate: fail the build if the binding surface drifted from the
# committed snapshot
mirror_bridge diff src/ --check

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

### `doctor` - Diagnose Setup Issues

Run environment diagnostics: compiler and reflection support, Python/Lua/Node
development files, library paths, the Mirror Bridge installation itself, and a
quick end-to-end binding compile + import test.

```bash
mirror_bridge doctor [options]
```

**Options:**

| Option | Description |
|--------|-------------|
| `-v, --verbose` | Show detailed output for each check |
| `--fix` | Attempt to fix common issues (experimental) |
| `--json` | Emit one JSON object on stdout instead of pretty output |

Exits `0` when no check failed (warnings allowed), `1` otherwise.

**Examples:**

```bash
mirror_bridge doctor             # Pretty diagnostic report
mirror_bridge doctor --json      # Machine-readable report
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

### Templates and Free Functions (Python)

A template is not a type until it is instantiated, and headers rarely say
which instantiations a binding should contain. For Python modules, `generate`
works it out from the headers themselves and lets the compiler settle what it
cannot:

- **Aliases are declarations.** `using Vec3f = Vector3<float>;` binds
  `Vector3<float>` as `Vec3f`. Specializations that only appear in signatures
  or members (`Vector3<long> ticks;`) are bound under a synthesized name
  (`Vector3_long`, `Matrix_float_3`).
- **Function and member templates** are instantiated over the *universe* —
  Python's scalars (`bool`, `long`, `double`, `std::string`) plus every type
  the headers *accept*: template arguments, parameter and member types
  (a `size_t` that only appears as a return type does not count) — and each
  candidate is compiled in a probe; the ones that fail (unsatisfied
  constraints, no `operator+` for that type, ...) are dropped with the
  compiler's reason. A class template nobody mentions gets the scalar
  baseline.
- **Free functions** are bound when every parameter and the return type
  convert. Overload sets are skipped for now.

In Python every template is a *family* that behaves like a generic type:

```python
geom.Vec3f(1.0, 2.0, 3.0)          # the alias
geom.Vector3[float]                # == geom.Vec3d (Python float is a double)
geom.Vector3["int"]                # a C++ spelling names the exact instantiation
geom.Vector3[np.float32]           # numpy dtypes work as keys
geom.Vector3(1.0, 2.0, 3.0)        # CTAD-like: the best constructor across instantiations
geom.Vector3.instantiations        # ['float', 'double', 'int', 'long', 'bool']
geom.clamp(1.5, 0.0, 1.0)          # dispatches on the arguments
geom.twice[np.int16](7)            # or pick the instantiation explicitly
v.cast[int]()                      # member templates too
```

The full plan — what was bound, what the compiler rejected and why, what
compiles but cannot be bound (a `T*` parameter), and every note — is written
to `<output>/<module>_plan.txt`. To bind an instantiation the headers never
mention, pass `--instantiate "geom::Vector3<short>"`; to keep the old
classes-only behaviour, pass `--no-templates`.

## Change Detection

By default, `generate` tracks source file changes and only recompiles when header files have been modified since the last build. Use `--force` to bypass this check.

## Machine-readable Output

`generate`, `diff`, and `doctor` accept `--json`. The contract: **stdout
carries exactly one JSON object** and all human-readable progress goes to
stderr, so scripts, CI pipelines, and AI agents can parse results without
scraping log text.

`generate --json` on success:

```json
{
  "status": "ok",
  "module": "my_module",
  "languages": ["python"],
  "classes": [{"name": "Greeter", "header": "greeter.hpp"}],
  "outputs": ["/path/to/build/my_module.so"],
  "templates": {
    "functions": ["dot3", "answer"],
    "instantiations": [{"kind": "class", "template": "geom::Vector3", "args": "float",
                        "cpp": "geom::Vector3<float>", "python": "Vec3f", "origin": "alias Vec3f"}],
    "rejected": [{"cpp": "geom::twice<geom::Robot>", "reason": "invalid operands to binary expression ..."}],
    "unbindable": [{"cpp": "geom::deref<long>", "reason": "parameter 'long*' is a raw pointer (only const char* converts)"}],
    "skipped_functions": [],
    "notes": [],
    "rounds": 4,
    "report": "/path/to/build/my_module_plan.txt"
  },
  "errors": []
}
```

`templates` is `null` for non-Python modules and with `--no-templates`.

On failure, each entry in `errors` carries the first compiler error plus an
actionable suggestion:

```json
{
  "status": "error",
  "errors": [{
    "lang": "python",
    "reason": "python/mirror_bridge_python.hpp:1127:14: error: no matching function for call to 'from_python'",
    "suggestion": "A member type has no converter. Mark it [[=exclude{}]] or add a custom converter. See docs/reference/type-conversion.md"
  }]
}
```

`diff --json` reports `{"status": "ok", "changed": true, "added": 1, "removed": 0}`.
`doctor --json` reports pass/warn/fail per check with a `hint` for each
non-pass. See the [Error Catalog](errors.md) for the full symptom-to-fix
reference the suggestions link to.

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Error (invalid arguments, compilation failure, binding-surface drift with `diff --check`, failed `doctor` checks) |
| 2 | `diff --check` ran without a stored snapshot to compare against |

## Environment Variables

| Variable | Description |
|----------|-------------|
| `MIRROR_BRIDGE_PCH_DIR` | Default PCH directory |
| `MIRROR_BRIDGE_OUTPUT_DIR` | Default output directory |
| `CXX` | C++ compiler to use |
