# Mirror Bridge Examples

Progressive examples from beginner to production-ready.

## Quick Start

```bash
cd examples/01-hello-world
../../tools/mirror_bridge generate src/ --module greeter --lang python
python3 test_greeter.py
```

## Example Progression

| Example | Concepts Covered |
|---------|------------------|
| [01-hello-world](01-hello-world/) | Basic class, properties, simple methods |
| [02-calculator](02-calculator/) | Multiple methods, parameters, exceptions |
| [03-containers](03-containers/) | Vectors, arrays, container conversion |
| [04-game-engine](04-game-engine/) | Nested objects, complex relationships |
| [05-production](05-production/) | Full project structure, namespaces, config files |

## Example Details

### 01: Hello World
**Time**: 5 minutes | **Difficulty**: Beginner

A simple `Greeter` class demonstrating:
- Properties (read/write)
- Simple methods
- Default values

### 02: Calculator
**Time**: 10 minutes | **Difficulty**: Beginner

A calculator with various method signatures:
- Methods with parameters
- Multiple parameters
- Const methods
- Exception handling
- String return values

### 03: Containers
**Time**: 10 minutes | **Difficulty**: Intermediate

Working with C++ containers:
- `std::vector<T>` to Python list
- `std::array<T, N>` to Python list
- Container methods

### 04: Game Engine
**Time**: 15 minutes | **Difficulty**: Intermediate

Mini game engine with nested objects:
- Nested structs (Entity has Transform)
- Dict assignment for nested objects
- Complex class relationships

### 05: Production
**Time**: 20 minutes | **Difficulty**: Advanced

Full production project structure:
- Multiple directories
- Namespaced classes
- Config file binding
- Cross-file dependencies

## Legacy Examples

The `option2/` and `option3/` directories contain the original examples and are kept for backward compatibility.

## Running All Examples

```bash
# From the examples directory
for dir in 01-* 02-* 03-* 04-* 05-*; do
    echo "=== $dir ==="
    cd "$dir"
    ../../tools/mirror_bridge generate src/ --module example --lang python
    python3 test_*.py
    cd ..
done
```

## Multi-Language Examples

Each example can be built for any supported language:

```bash
# Python
../../tools/mirror_bridge generate src/ --module example --lang python

# Lua
../../tools/mirror_bridge generate src/ --module example --lang lua

# JavaScript
../../tools/mirror_bridge generate src/ --module example --lang js

# All at once
../../tools/mirror_bridge generate src/ --module example --lang all
```

## Documentation

- [Getting Started](../docs/getting-started/quickstart.md)
- [CLI Reference](../docs/reference/cli.md)
- [Type Conversion](../docs/reference/type-conversion.md)
