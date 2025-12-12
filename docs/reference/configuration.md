# Configuration Reference

Mirror Bridge supports configuration files for production use cases where you need explicit control over what gets bound.

## Config File Format

Configuration files use the `.mirror` extension by convention.

### Basic Structure

```
# Module name (required)
module: my_module

# Include directories (optional)
include_dirs: src/, include/

# Class bindings
ClassName: header_file.hpp
AnotherClass: path/to/header.hpp
namespace::Class: class.hpp as PythonName
```

### Comments

Lines starting with `#` are comments:

```
# This is a comment
module: my_module  # Inline comments are NOT supported
```

### Module Name

The `module:` directive is required and specifies the output module name:

```
module: my_module
```

This produces `my_module.so` (Python/Lua) or `my_module.node` (JavaScript).

### Include Directories

The `include_dirs:` directive specifies where to find headers:

```
include_dirs: src/, include/, ../common/
```

- Comma-separated list
- Paths are relative to the config file location
- Added as `-I` flags during compilation

### Class Bindings

Each class binding has the format:

```
ClassName: header_file.hpp [as ExposedName]
```

**Examples:**

```
# Basic binding
Calculator: calculator.hpp

# Class in subdirectory
math::Vector3: math/vector3.hpp

# Rename for the target language
internal::ComplexCalculator: internal/calc.hpp as Calculator

# Namespace handling
mylib::MyClass: mylib/my_class.hpp as MyClass
```

## Complete Example

```
# Math library bindings
# Generated: 2024-01-15

module: mathlib

include_dirs: src/, include/

# Core types
Vector2: core/vector2.hpp
Vector3: core/vector3.hpp
Matrix4: core/matrix4.hpp

# Math operations
math::Calculator: math/calculator.hpp as Calculator
math::Statistics: math/statistics.hpp as Statistics

# Geometry
geometry::Point: geometry/point.hpp as Point
geometry::Rectangle: geometry/rectangle.hpp as Rectangle
geometry::Circle: geometry/circle.hpp as Circle
```

## Usage

### Generate from Config

```bash
mirror_bridge config my_module.mirror
```

### Options

```bash
mirror_bridge config my_module.mirror [options]

Options:
  -o, --output DIR      Output directory (default: build/)
  -k, --keep-generated  Keep generated binding .cpp file
  -v, --verbose         Show compilation output
  --pch [PATH]          Use precompiled header
  --lang LANG           Target language (default: python)
```

### Multiple Languages

Create separate config files for language-specific bindings:

```bash
# Python config (all classes)
mirror_bridge config mathlib_python.mirror --lang python

# Lua config (subset of classes)
mirror_bridge config mathlib_lua.mirror --lang lua
```

Or use the same config for multiple languages:

```bash
mirror_bridge config mathlib.mirror --lang python
mirror_bridge config mathlib.mirror --lang lua
mirror_bridge config mathlib.mirror --lang js
```

## When to Use Config Files

### Use Config Files When:

- Production deployments requiring explicit control
- Large codebases with selective exposure
- Multiple modules from the same source
- Class renaming is needed
- Version control of binding definitions
- CI/CD pipeline reproducibility

### Use Auto-Discovery When:

- Rapid prototyping
- Small to medium projects
- Binding entire directories
- Development iterations

## Comparison

| Feature | Auto-Discovery | Config File |
|---------|---------------|-------------|
| Setup effort | Minimal | Write config |
| Control | All or opt-out | Explicit list |
| New classes | Automatic | Manual add |
| Renaming | Not supported | Supported |
| VCS friendly | N/A | Yes |
| Best for | Development | Production |

## Migration Path

Start with auto-discovery during development, then create a config file for production:

```bash
# Development: quick iteration
mirror_bridge generate src/ --module my_module --lang python

# Production: create config with discovered classes
# (manually create config file listing the classes you want)
mirror_bridge config my_module.mirror --lang python
```

## Validation

The config file is validated during generation:
- Module name must be present
- Header files must exist
- Class names must be valid identifiers
- Duplicate class names are rejected

Errors are reported with line numbers for easy debugging.
