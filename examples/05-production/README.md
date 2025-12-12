# Example 05: Production Project

A full production-style project structure with multiple directories and namespaces.

## What You'll Learn

- Multi-directory project organization
- Namespaced classes
- Config file-based binding
- Cross-file dependencies
- Production workflow

## Project Structure

```
05-production/
├── src/
│   ├── math/
│   │   └── vector3.hpp      # mathlib::Vector3
│   ├── physics/
│   │   └── particle.hpp     # physics::Particle, ParticleSystem
│   └── config.hpp           # Config
├── mathlib.mirror           # Binding configuration
├── test_mathlib.py          # Python tests
└── README.md
```

## The C++ Code

### math/vector3.hpp
```cpp
namespace mathlib {
    struct Vector3 {
        double x, y, z;
        double length() const;
        void normalize();
        double dot(double ox, double oy, double oz) const;
    };
}
```

### physics/particle.hpp
```cpp
namespace physics {
    struct Particle {
        mathlib::Vector3 position;  // Cross-namespace reference
        mathlib::Vector3 velocity;
        double mass;

        void apply_force(double fx, double fy, double fz);
        void update(double dt);
        double kinetic_energy() const;
    };
}
```

## Build Methods

### Method 1: Auto-Discovery (Development)

```bash
# Quick iteration during development
../../tools/mirror_bridge generate src/ --module mathlib --lang python
```

### Method 2: Config File (Production)

```bash
# Explicit control for production
../../tools/mirror_bridge config mathlib.mirror
```

The config file (`mathlib.mirror`):
```
module: mathlib
include_dirs: src/

mathlib::Vector3: math/vector3.hpp as Vector3
physics::Particle: physics/particle.hpp as Particle
physics::ParticleSystem: physics/particle.hpp as ParticleSystem
Config: config.hpp
```

## Run Tests

```bash
python3 test_mathlib.py
```

## Usage

```python
import mathlib

# Vector math
v = mathlib.Vector3()
v.x, v.y, v.z = 3.0, 4.0, 0.0
print(v.length())  # 5.0

v.normalize()
print(v.length())  # 1.0

# Physics simulation
particle = mathlib.Particle()
particle.mass = 2.0
particle.velocity.x = 10.0

particle.apply_force(4.0, 0.0, 0.0)  # Accelerate
particle.update(0.016)               # Simulate one frame

print(particle.position.x)           # Position after update
print(particle.kinetic_energy())     # KE = 0.5 * m * v^2

# Configuration
config = mathlib.Config()
config.enable_debug()
print(config.info())  # "PhysicsSimulator v1.0.0"
```

## Production Recommendations

1. **Use config files** for explicit control over what's exposed
2. **Organize by feature** (math/, physics/, etc.)
3. **Use namespaces** to avoid naming conflicts
4. **Create comprehensive tests** for all bound classes
5. **Use PCH** for faster iteration:
   ```bash
   ../../tools/mirror_bridge pch --output build/ --type release
   ../../tools/mirror_bridge generate src/ --module mathlib --lang python --pch
   ```

## Multi-Language Support

Generate bindings for all supported languages:

```bash
# All at once
../../tools/mirror_bridge generate src/ --module mathlib --lang all

# Or individually
../../tools/mirror_bridge generate src/ --module mathlib --lang python
../../tools/mirror_bridge generate src/ --module mathlib --lang lua
../../tools/mirror_bridge generate src/ --module mathlib --lang js
```
