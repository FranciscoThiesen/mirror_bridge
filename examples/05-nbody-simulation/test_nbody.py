"""
N-Body Gravitational Simulation - Integration Tests

Demonstrates the nbody C++ library from Python, showcasing:
- Vector math and physics calculations
- Celestial body dynamics
- Numerical integration methods (Verlet, RK4)
- Energy conservation analysis
- Orbital mechanics
"""

import nbody
import math


def test_vec3_basics():
    """Test Vec3 fundamental operations"""
    print("=== Vec3 Basic Operations ===")

    # Create vectors
    v1 = nbody.Vec3(1.0, 2.0, 3.0)
    v2 = nbody.Vec3(4.0, 5.0, 6.0)

    print(f"v1 = {v1.to_string()}")
    print(f"v2 = {v2.to_string()}")

    # Arithmetic
    v_add = v1.add(v2)
    v_sub = v1.sub(v2)
    v_scale = v1.scale(2.0)
    print(f"v1 + v2 = {v_add.to_string()}")
    print(f"v1 - v2 = {v_sub.to_string()}")
    print(f"v1 * 2 = {v_scale.to_string()}")

    # Products
    dot = v1.dot(v2)
    cross = v1.cross(v2)
    print(f"v1 · v2 = {dot}")
    print(f"v1 × v2 = {cross.to_string()}")


def test_vec3_geometry():
    """Test Vec3 geometric operations"""
    print("\n=== Vec3 Geometry ===")

    v = nbody.Vec3(3.0, 4.0, 0.0)
    print(f"v = {v.to_string()}")
    print(f"Length: {v.length()} (expected: 5)")
    print(f"Length²: {v.length_squared()} (expected: 25)")

    norm = v.normalized()
    print(f"Normalized: {norm.to_string()}")
    print(f"Normalized length: {norm.length():.6f} (expected: 1)")

    # Distance
    p1 = nbody.Vec3(0, 0, 0)
    p2 = nbody.Vec3(1, 1, 1)
    print(f"Distance (0,0,0) to (1,1,1): {p1.distance(p2):.4f} (expected: {math.sqrt(3):.4f})")

    # Angle between vectors
    vx = nbody.Vec3.unit_x()
    vy = nbody.Vec3.unit_y()
    angle = vx.angle(vy)
    print(f"Angle between X and Y axes: {math.degrees(angle):.1f}° (expected: 90°)")

    # Projection
    a = nbody.Vec3(3, 4, 0)
    b = nbody.Vec3(1, 0, 0)
    proj = a.project(b)
    print(f"(3,4,0) projected onto X axis: {proj.to_string()}")


def test_body_physics():
    """Test Body physics calculations"""
    print("\n=== Body Physics ===")

    # Create a body
    earth = nbody.Body()
    earth.name = "Earth"
    earth.mass = nbody.Constants.EARTH_MASS
    earth.position = nbody.Vec3(nbody.Constants.AU, 0, 0)
    earth.velocity = nbody.Vec3(0, 29780, 0)  # ~30 km/s orbital velocity

    print(f"Body: {earth.to_string()}")
    print(f"Kinetic energy: {earth.kinetic_energy():.3e} J")
    print(f"Speed: {earth.speed():.1f} m/s")
    print(f"Distance from origin: {earth.distance_from_origin():.3e} m")

    # Momentum
    p = earth.momentum()
    print(f"Momentum magnitude: {p.length():.3e} kg·m/s")

    # Angular momentum
    L = earth.angular_momentum()
    print(f"Angular momentum magnitude: {L.length():.3e} kg·m²/s")


def test_orbital_elements():
    """Test orbital element calculations"""
    print("\n=== Orbital Elements ===")

    # Create a circular orbit
    sun_mass = nbody.Constants.SOLAR_MASS
    r = nbody.Constants.AU  # 1 AU

    # Circular orbital velocity
    G = nbody.Constants.G
    v_circ = math.sqrt(G * sun_mass / r)

    pos = nbody.Vec3(r, 0, 0)
    vel = nbody.Vec3(0, v_circ, 0)

    elements = nbody.OrbitalElements.from_state_vectors(pos, vel, sun_mass, G)

    print(f"Circular orbit at 1 AU:")
    print(f"  Semi-major axis: {elements.semi_major_axis / nbody.Constants.AU:.4f} AU")
    print(f"  Eccentricity: {elements.eccentricity:.6f} (expected: ~0)")
    print(f"  Period: {elements.orbital_period(sun_mass, G) / nbody.Constants.DAY:.1f} days")

    # Derived quantities
    print(f"  Periapsis: {elements.periapsis() / nbody.Constants.AU:.4f} AU")
    print(f"  Apoapsis: {elements.apoapsis() / nbody.Constants.AU:.4f} AU")

    # Test elliptical orbit (higher velocity = elliptical)
    v_ellip = v_circ * 1.2
    vel_ellip = nbody.Vec3(0, v_ellip, 0)
    elements_ellip = nbody.OrbitalElements.from_state_vectors(pos, vel_ellip, sun_mass, G)

    print(f"\nElliptical orbit (20% faster):")
    print(f"  Semi-major axis: {elements_ellip.semi_major_axis / nbody.Constants.AU:.4f} AU")
    print(f"  Eccentricity: {elements_ellip.eccentricity:.4f}")
    print(f"  Periapsis: {elements_ellip.periapsis() / nbody.Constants.AU:.4f} AU")
    print(f"  Apoapsis: {elements_ellip.apoapsis() / nbody.Constants.AU:.4f} AU")


def test_two_body_simulation():
    """Test two-body gravitational simulation"""
    print("\n=== Two-Body Simulation ===")

    # Create binary system
    sim = nbody.create_binary_system(
        m1=1e10,  # kg
        m2=1e10,  # kg
        separation=1000.0,  # m
        G=nbody.Constants.G
    )

    print(f"Created binary system with {sim.body_count()} bodies")

    # Initial state
    E0 = sim.total_energy()
    L0 = sim.total_angular_momentum()
    print(f"Initial energy: {E0:.6e} J")
    print(f"Initial angular momentum: {L0.length():.6e} kg·m²/s")

    # Run simulation
    dt = 0.1  # 0.1 second timestep
    sim.run(100.0, dt)  # Run for 100 seconds

    # Final state
    E1 = sim.total_energy()
    L1 = sim.total_angular_momentum()
    print(f"\nAfter {sim.step_count()} steps (t={sim.time():.1f}s):")
    print(f"Final energy: {E1:.6e} J")
    print(f"Energy error: {abs((E1-E0)/E0)*100:.6f}%")
    print(f"Angular momentum error: {abs(L1.length()-L0.length())/L0.length()*100:.6f}%")


def test_solar_system():
    """Test inner solar system simulation"""
    print("\n=== Inner Solar System ===")

    sim = nbody.create_solar_system_inner()
    sim.set_softening(1e8)  # 100 km softening

    print(f"Bodies: {sim.body_names()}")
    print(f"Body count: {sim.body_count()}")

    # Get Earth's initial orbital elements
    earth_idx = sim.find_body("Earth")
    earth = sim.get_body(earth_idx)
    print(f"\nEarth initial state:")
    print(f"  Position: {earth.position.distance_from_origin()/nbody.Constants.AU:.4f} AU")
    print(f"  Velocity: {earth.velocity.length()/1000:.2f} km/s")

    elements = sim.orbital_elements_of(earth_idx)
    print(f"  Orbital elements: {elements.to_string()}")

    # Run for 1 year
    day = nbody.Constants.DAY
    year = nbody.Constants.YEAR
    dt = day / 10  # 2.4 hour timestep

    print(f"\nRunning simulation for 1 year...")
    sim.run(year, dt)

    print(f"Steps: {sim.step_count()}, Time: {sim.time()/year:.3f} years")

    # Check Earth returned to approximately the same position
    earth_final = sim.get_body(earth_idx)
    print(f"\nEarth after 1 year:")
    print(f"  Position: {earth_final.position.distance_from_origin()/nbody.Constants.AU:.4f} AU")

    # Angular position
    angle = math.atan2(earth_final.position.y, earth_final.position.x)
    print(f"  Angular position: {math.degrees(angle):.1f}° (expected: ~360° = ~0°)")


def test_energy_conservation():
    """Test energy conservation with different integrators"""
    print("\n=== Energy Conservation Comparison ===")

    def run_test(name, step_fn, sim, dt, duration):
        E0 = sim.total_energy()
        steps = int(duration / dt)

        for _ in range(steps):
            step_fn(dt)

        E1 = sim.total_energy()
        error = abs((E1 - E0) / E0) * 100
        return error

    # Create identical binary systems for each test
    def make_sim():
        return nbody.create_binary_system(1e10, 1e10, 1000.0)

    dt = 0.01
    duration = 10.0

    # Test Verlet
    sim_verlet = make_sim()
    E0 = sim_verlet.total_energy()
    sim_verlet.run(duration, dt)
    verlet_error = abs((sim_verlet.total_energy() - E0) / E0) * 100
    print(f"Verlet integrator: {verlet_error:.6f}% energy error")

    # Test Euler (using step_euler)
    sim_euler = make_sim()
    E0 = sim_euler.total_energy()
    steps = int(duration / dt)
    for _ in range(steps):
        sim_euler.step_euler(dt)
    euler_error = abs((sim_euler.total_energy() - E0) / E0) * 100
    print(f"Symplectic Euler: {euler_error:.6f}% energy error")

    print(f"\nVerlet is typically better for long-term energy conservation")


def test_statistics():
    """Test simulation statistics tracking"""
    print("\n=== Simulation Statistics ===")

    sim = nbody.create_binary_system(1e10, 1e10, 1000.0)

    # Initialize stats tracker
    stats = nbody.SimulationStats()
    stats.initialize(sim)

    # Run simulation and record periodically
    dt = 0.1
    for _ in range(100):
        sim.step_verlet(dt)
        stats.record(sim)

    print(stats.conservation_report(sim))
    print(f"\nHistory length: {stats.history_length()} data points")

    # Virial analysis
    virial = nbody.VirialAnalysis.compute(sim)
    print(f"\n{virial.to_string()}")


def test_cluster_statistics():
    """Test cluster analysis on random N-body system"""
    print("\n=== Cluster Statistics ===")

    # Create a small cluster
    sim = nbody.create_random_cluster(
        n_bodies=20,
        total_mass=1e12,
        radius=1000.0,
        velocity_dispersion=10.0
    )

    print(f"Created cluster with {sim.body_count()} bodies")

    # Compute statistics
    cluster_stats = nbody.ClusterStatistics.compute(sim)
    print(cluster_stats.to_string())

    # Run and see how cluster evolves
    print("\nEvolving cluster...")
    sim.run(100.0, 0.1)

    cluster_stats_final = nbody.ClusterStatistics.compute(sim)
    print(f"\nAfter evolution:")
    print(cluster_stats_final.to_string())


def test_lagrange_points():
    """Test Lagrange point calculation"""
    print("\n=== Lagrange Points ===")

    # Sun-Earth system
    sun_pos = nbody.Vec3.zero()
    earth_pos = nbody.Vec3(nbody.Constants.AU, 0, 0)
    sun_mass = nbody.Constants.SOLAR_MASS
    earth_mass = nbody.Constants.EARTH_MASS

    lp = nbody.LagrangePoints.compute(sun_pos, earth_pos, sun_mass, earth_mass)

    AU = nbody.Constants.AU
    print("Sun-Earth Lagrange points (relative to Sun):")
    print(f"  L1 (between): {lp.L1.x/AU:.4f} AU")
    print(f"  L2 (beyond Earth): {lp.L2.x/AU:.4f} AU")
    print(f"  L3 (opposite): {lp.L3.x/AU:.4f} AU")
    print(f"  L4 (leading): ({lp.L4.x/AU:.4f}, {lp.L4.y/AU:.4f}) AU")
    print(f"  L5 (trailing): ({lp.L5.x/AU:.4f}, {lp.L5.y/AU:.4f}) AU")

    # L1 should be about 1.5 million km from Earth (0.01 AU)
    l1_from_earth = lp.L1.distance(earth_pos)
    print(f"\nL1 distance from Earth: {l1_from_earth/1e6:.2f} million km")
    print(f"  (James Webb Space Telescope orbits near L2)")


def test_physical_constants():
    """Verify physical constants"""
    print("\n=== Physical Constants ===")

    print(f"Gravitational constant G: {nbody.Constants.G:.5e} m³/kg/s²")
    print(f"Astronomical Unit (AU): {nbody.Constants.AU:.6e} m")
    print(f"Solar mass: {nbody.Constants.SOLAR_MASS:.3e} kg")
    print(f"Earth mass: {nbody.Constants.EARTH_MASS:.3e} kg")
    print(f"Day: {nbody.Constants.DAY} seconds")
    print(f"Year: {nbody.Constants.YEAR:.2f} seconds")


if __name__ == "__main__":
    test_vec3_basics()
    test_vec3_geometry()
    test_body_physics()
    test_orbital_elements()
    test_two_body_simulation()
    test_solar_system()
    test_energy_conservation()
    test_statistics()
    test_cluster_statistics()
    test_lagrange_points()
    test_physical_constants()

    print("\n=== All tests completed! ===")
