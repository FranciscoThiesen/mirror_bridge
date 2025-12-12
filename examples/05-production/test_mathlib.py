#!/usr/bin/env python3
"""Test the production mathlib module."""

import sys
sys.path.insert(0, 'build')

import mathlib

def test_vector3():
    v = mathlib.Vector3()
    v.x = 3.0
    v.y = 4.0
    v.z = 0.0

    length = v.length()
    assert abs(length - 5.0) < 0.001, f"Expected 5.0, got {length}"

    v.normalize()
    assert abs(v.length() - 1.0) < 0.001, "Expected normalized length 1.0"

    print("✓ Vector3 test passed")

def test_particle():
    p = mathlib.Particle()
    p.mass = 2.0

    # Set initial velocity
    p.velocity.x = 10.0
    p.velocity.y = 0.0
    p.velocity.z = 0.0

    # Apply force
    p.apply_force(4.0, 0.0, 0.0)  # a = F/m = 4/2 = 2
    assert p.velocity.x == 12.0, f"Expected 12.0, got {p.velocity.x}"

    # Update position
    p.update(0.5)  # dt = 0.5s
    assert p.position.x == 6.0, f"Expected 6.0, got {p.position.x}"

    # Check kinetic energy
    ke = p.kinetic_energy()
    assert ke > 0, f"Expected positive kinetic energy, got {ke}"

    print("✓ Particle test passed")

def test_particle_system():
    ps = mathlib.ParticleSystem()
    ps.gravity = -10.0

    ps.add_particle(1.0)
    ps.add_particle(2.0)
    ps.add_particle(3.0)

    assert ps.particle_count == 3
    assert ps.total_mass() == 6.0

    ps.clear()
    assert ps.particle_count == 0

    print("✓ ParticleSystem test passed")

def test_config():
    cfg = mathlib.Config()

    assert cfg.app_name == "PhysicsSimulator"
    assert cfg.version == "1.0.0"
    assert cfg.debug_mode == False

    cfg.enable_debug()
    assert cfg.debug_mode == True

    info = cfg.info()
    assert "PhysicsSimulator" in info
    assert "1.0.0" in info

    print("✓ Config test passed")

def test_nested_access():
    p = mathlib.Particle()

    # Set nested vector properties
    p.position.x = 10.0
    p.position.y = 20.0
    p.velocity.x = 5.0

    assert p.position.x == 10.0
    assert p.velocity.x == 5.0

    # Set from dict
    p.position = {"x": 1.0, "y": 2.0, "z": 3.0}
    assert p.position.x == 1.0

    print("✓ Nested access test passed")

if __name__ == "__main__":
    test_vector3()
    test_particle()
    test_particle_system()
    test_config()
    test_nested_access()
    print("\n✓ All tests passed!")
