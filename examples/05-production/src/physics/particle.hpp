#pragma once
#include "../math/vector3.hpp"
#include <vector>

namespace physics {

struct Particle {
    mathlib::Vector3 position;
    mathlib::Vector3 velocity;
    double mass = 1.0;
    double lifetime = 0.0;
    bool active = true;

    void apply_force(double fx, double fy, double fz) {
        // F = ma, so a = F/m
        velocity.x += fx / mass;
        velocity.y += fy / mass;
        velocity.z += fz / mass;
    }

    void update(double dt) {
        if (!active) return;

        position.x += velocity.x * dt;
        position.y += velocity.y * dt;
        position.z += velocity.z * dt;

        lifetime += dt;
    }

    double kinetic_energy() const {
        double v2 = velocity.length_squared();
        return 0.5 * mass * v2;
    }

    void reset() {
        position = mathlib::Vector3();
        velocity = mathlib::Vector3();
        lifetime = 0.0;
        active = true;
    }
};

struct ParticleSystem {
    std::vector<double> masses;
    double gravity = -9.81;
    int particle_count = 0;

    void add_particle(double mass) {
        masses.push_back(mass);
        particle_count++;
    }

    double total_mass() const {
        double sum = 0.0;
        for (double m : masses) {
            sum += m;
        }
        return sum;
    }

    void clear() {
        masses.clear();
        particle_count = 0;
    }
};

} // namespace physics
