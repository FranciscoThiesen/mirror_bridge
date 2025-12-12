# Example 05: N-Body Gravitational Simulation

A production-quality gravitational N-body simulation demonstrating scientific
computing patterns commonly used in astrophysics and physics research.

## Why This Example?

N-body simulation is a canonical scientific computing problem where C++ bindings
provide real value:

- **Performance**: O(n²) force calculations over millions of timesteps
- **Numerical precision**: Double-precision physics with conservation guarantees
- **Algorithm complexity**: Multiple integrators, orbital mechanics, statistics
- **Research workflows**: Python for analysis/visualization, C++ for computation

Libraries like REBOUND, GADGET, and many molecular dynamics codes use this pattern.

## Library Structure

```
src/
├── vec3.hpp         # 3D vector math with full geometric operations
├── body.hpp         # Celestial body physics, orbital elements
├── integrator.hpp   # Numerical integrators (Euler, Verlet, RK4, Leapfrog)
├── simulation.hpp   # Main simulation driver, force calculation
└── statistics.hpp   # Conservation analysis, virial theorem, cluster stats
```

## Features Demonstrated

### Vector Mathematics (`vec3.hpp`)
- Full 3D vector operations (add, sub, scale, dot, cross)
- Geometric operations (normalize, project, reflect, angle)
- Spherical coordinate conversion
- Length clamping and approximate equality

### Celestial Bodies (`body.hpp`)
- Newtonian physics (kinetic energy, momentum, angular momentum)
- Gravitational force calculation with softening
- Escape/orbital velocity computation
- Sphere of influence (Hill sphere)

### Orbital Mechanics (`body.hpp`)
- Keplerian orbital elements (a, e, i, Ω, ω, ν)
- State vector to orbital elements conversion
- Derived quantities (periapsis, apoapsis, period, vis-viva)
- Mean motion and orbital period calculation

### Numerical Integration (`integrator.hpp`)
- **Euler**: Simple O(h) method (baseline)
- **Symplectic Euler**: Better energy conservation
- **Velocity Verlet**: O(h²) symplectic, standard for N-body
- **RK4**: O(h⁴) accuracy, requires force re-evaluation
- **Leapfrog**: Alternative symplectic formulation
- **Adaptive timestep**: Error-based dt adjustment

### Simulation Engine (`simulation.hpp`)
- Pairwise force calculation (Newton's third law optimization)
- Softening to prevent singularities
- Energy, momentum, angular momentum tracking
- Center of mass computation
- Factory functions for common setups

### Statistical Analysis (`statistics.hpp`)
- Conservation law monitoring (energy, momentum, angular momentum)
- Virial theorem analysis (bound/virialized state)
- Lagrange point calculation
- Cluster statistics (half-mass radius, velocity dispersion)
- Crossing time estimation

## Building

```bash
mirror_bridge generate examples/05-nbody-simulation/src/ \
    --module nbody \
    --lang python \
    --output examples/05-nbody-simulation/build/
```

## Usage from Python

```python
import nbody

# Create a binary star system
sim = nbody.create_binary_system(
    m1=1.0e30,     # Solar mass
    m2=1.0e30,
    separation=1e11,  # 1 AU-ish
    G=nbody.Constants.G
)

# Run simulation using Velocity Verlet
dt = 3600.0  # 1 hour timestep
sim.run(365.25 * 24 * 3600, dt)  # Run for 1 year

# Check energy conservation
print(f"Energy error: {sim.total_energy()}")

# Get orbital elements
elements = sim.orbital_elements_of(0)
print(f"Semi-major axis: {elements.semi_major_axis}")
print(f"Eccentricity: {elements.eccentricity}")

# Statistical analysis
stats = nbody.SimulationStats()
stats.initialize(sim)
print(stats.conservation_report(sim))
```

## Physics Background

### Gravitational Force
```
F = G * m1 * m2 / r²
```

With softening ε to prevent singularity:
```
F = G * m1 * m2 / (r² + ε²)
```

### Integrator Comparison

| Method | Order | Symplectic | Energy Drift | Use Case |
|--------|-------|------------|--------------|----------|
| Euler | O(h) | No | Large | Never (baseline only) |
| Symplectic Euler | O(h) | Yes | Bounded | Quick tests |
| Verlet | O(h²) | Yes | Bounded | Long-term orbital |
| RK4 | O(h⁴) | No | Cumulative | Short-term precision |
| Leapfrog | O(h²) | Yes | Bounded | Cosmological |

### Virial Theorem
For a gravitationally bound system in equilibrium:
```
2⟨KE⟩ + ⟨PE⟩ = 0
```

The virial ratio `2*KE/|PE|` indicates:
- < 1: Collapsing
- ≈ 1: Virialized (equilibrium)
- \> 1: Expanding

## Real-World Applications

This library structure mirrors production scientific codes:

1. **Planetary dynamics**: REBOUND, Mercury, SWIFT
2. **Star clusters**: NBODY6, PhiGRAPE
3. **Cosmology**: GADGET, AREPO (with SPH)
4. **Molecular dynamics**: LAMMPS, GROMACS (different force law)

## What This Shows About mirror_bridge

- **Static methods**: `Vec3::zero()`, `create_binary_system()`
- **Factory functions**: Simulation creation helpers
- **Complex algorithms**: Force calculation, orbital elements
- **Nested return types**: `OrbitalElements` returned from methods
- **Containers**: `std::vector<Body>`, `std::vector<double>` for histories
- **Method chaining**: Vec3 operations return new Vec3
- **Callbacks**: `run_with_callback` for monitoring (std::function)
