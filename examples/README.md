# Mirror Bridge Examples

Progressive examples from beginner to production-ready scientific computing.

## Quick Start

```bash
cd examples/01-hello-world
../../tools/mirror_bridge generate src/ --module greeter --lang python
python3 test_greeter.py
```

## Example Progression

| Example | Focus | Complexity |
|---------|-------|------------|
| [01-hello-world](01-hello-world/) | Properties, simple methods | Beginner |
| [02-calculator](02-calculator/) | Parameters, exceptions, const methods | Beginner |
| [03-containers](03-containers/) | Vector/array conversion, STL containers | Intermediate |
| [04-image-processing](04-image-processing/) | Real-world image processing library | Advanced |
| [05-nbody-simulation](05-nbody-simulation/) | Scientific computing, physics simulation | Advanced |

## Example Details

### 01: Hello World
**Difficulty**: Beginner

A simple `Greeter` class demonstrating:
- Properties (read/write)
- Simple methods
- Default values

### 02: Calculator
**Difficulty**: Beginner

A calculator with various method signatures:
- Methods with parameters
- Multiple parameters
- Const methods
- Exception handling

### 03: Containers
**Difficulty**: Intermediate

Working with C++ containers:
- `std::vector<T>` to Python list
- `std::array<T, N>` to Python list
- Container methods

### 04: Image Processing Library
**Difficulty**: Advanced | **Lines**: ~1200

A realistic image processing library demonstrating why C++ bindings matter:

```
src/
├── color.hpp      # RGBA/HSV, blending, color math
├── image.hpp      # Image buffer, transforms, compositing
├── kernel.hpp     # Convolution (blur, sharpen, edge detect)
└── histogram.hpp  # Analysis, equalization, thresholding
```

**Features**: Gaussian blur, Sobel edge detection, histogram equalization,
bilinear resize, HSV color space, Otsu's method

### 05: N-Body Gravitational Simulation
**Difficulty**: Advanced | **Lines**: ~1400

Production-quality gravitational simulation for astrophysics:

```
src/
├── vec3.hpp         # Full 3D vector math
├── body.hpp         # Celestial bodies, orbital elements
├── integrator.hpp   # Euler, Verlet, RK4, Leapfrog
├── simulation.hpp   # Force calculation, simulation driver
└── statistics.hpp   # Conservation analysis, virial theorem
```

**Features**: Symplectic integration, energy conservation, Keplerian orbital
elements, Lagrange points, cluster statistics, virial analysis

## Running Examples

Each example follows the same pattern:

```bash
cd examples/04-image-processing
../../tools/mirror_bridge generate src/ --module imgproc --lang python
python3 test_imgproc.py
```

### Run All Examples

```bash
# From the examples directory
for dir in 01-* 02-* 03-* 04-* 05-*; do
    name=$(echo $dir | cut -d'-' -f2-)
    echo "=== $dir ==="
    cd "$dir"
    ../../tools/mirror_bridge generate src/ --module $name --lang python
    python3 test_*.py
    cd ..
done
```

## Multi-Language Support

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

## Why These Examples?

**Examples 01-03** teach mirror_bridge concepts incrementally.

**Examples 04-05** show realistic use cases where C++ bindings provide real value:
- Image processing: Pixel-level operations at native speed
- N-body simulation: Millions of floating-point operations per frame

These mirror production libraries like OpenCV (image processing) and REBOUND
(orbital mechanics) that use the same Python-calls-C++ pattern.

## Documentation

- [Getting Started](../docs/getting-started/quickstart.md)
- [CLI Reference](../docs/reference/cli.md)
- [Type Conversion](../docs/reference/type-conversion.md)
