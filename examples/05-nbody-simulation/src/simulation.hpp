#pragma once
#include "body.hpp"
#include "integrator.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <functional>

namespace nbody {

// Main N-body simulation driver
class Simulation {
private:
    std::vector<Body> bodies_;
    double G_ = Constants::G;           // Gravitational constant
    double softening_ = 0.0;            // Softening length to prevent singularities
    double time_ = 0.0;                 // Current simulation time
    int step_count_ = 0;                // Number of steps taken

public:
    Simulation() = default;

    explicit Simulation(double G) : G_(G) {}

    // Configuration
    void set_gravitational_constant(double G) { G_ = G; }
    double gravitational_constant() const { return G_; }

    void set_softening(double eps) { softening_ = eps; }
    double softening() const { return softening_; }

    double time() const { return time_; }
    int step_count() const { return step_count_; }

    // Body management
    void add_body(const Body& body) {
        bodies_.push_back(body);
    }

    void add_body(std::string name, double mass, Vec3 position, Vec3 velocity) {
        bodies_.emplace_back(std::move(name), mass, position, velocity);
    }

    int body_count() const { return static_cast<int>(bodies_.size()); }

    Body get_body(int index) const {
        if (index < 0 || index >= static_cast<int>(bodies_.size())) {
            throw std::out_of_range("Body index out of range");
        }
        return bodies_[index];
    }

    void clear() {
        bodies_.clear();
        time_ = 0.0;
        step_count_ = 0;
    }

    // Get all body names
    std::vector<std::string> body_names() const {
        std::vector<std::string> names;
        names.reserve(bodies_.size());
        for (const auto& body : bodies_) {
            names.push_back(body.name);
        }
        return names;
    }

    // Find body by name, returns -1 if not found
    int find_body(const std::string& name) const {
        for (size_t i = 0; i < bodies_.size(); ++i) {
            if (bodies_[i].name == name) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    // Compute gravitational force on body i from all other bodies
    Vec3 compute_force_on(int i) const {
        Vec3 total_force = Vec3::zero();
        const Body& target = bodies_[i];

        for (size_t j = 0; j < bodies_.size(); ++j) {
            if (static_cast<int>(j) == i) continue;

            const Body& source = bodies_[j];
            Vec3 direction = source.position.sub(target.position);
            double dist_sq = direction.length_squared();

            // Apply softening
            double soft_sq = softening_ * softening_;
            double effective_dist_sq = dist_sq + soft_sq;

            if (effective_dist_sq < 1e-30) continue;

            double force_mag = G_ * target.mass * source.mass / effective_dist_sq;
            double dist = std::sqrt(effective_dist_sq);

            total_force = total_force.add(direction.scale(force_mag / dist));
        }

        return total_force;
    }

    // Compute all forces and update accelerations
    void compute_forces() {
        // Reset accelerations
        for (auto& body : bodies_) {
            body.reset_acceleration();
        }

        // O(n²) force calculation - compute each pair once
        for (size_t i = 0; i < bodies_.size(); ++i) {
            for (size_t j = i + 1; j < bodies_.size(); ++j) {
                Body& bi = bodies_[i];
                Body& bj = bodies_[j];

                Vec3 direction = bj.position.sub(bi.position);
                double dist_sq = direction.length_squared();

                // Apply softening
                double soft_sq = softening_ * softening_;
                double effective_dist_sq = dist_sq + soft_sq;

                if (effective_dist_sq < 1e-30) continue;

                double dist = std::sqrt(effective_dist_sq);
                double force_mag = G_ * bi.mass * bj.mass / effective_dist_sq;

                Vec3 force = direction.scale(force_mag / dist);

                bi.apply_force(force);
                bj.apply_force(force.scale(-1));  // Newton's third law
            }
        }
    }

    // Step simulation using Velocity Verlet (most common for N-body)
    void step_verlet(double dt) {
        VerletIntegrator integrator;

        // Step 1: Update positions and half-update velocities using old acceleration
        integrator.step_positions(bodies_, dt);

        // Step 2: Compute new forces/accelerations at new positions
        compute_forces();

        // Step 3: Complete velocity update using new acceleration
        integrator.step_velocities(bodies_, dt);

        time_ += dt;
        step_count_++;
    }

    // Step simulation using Symplectic Euler
    void step_euler(double dt) {
        SymplecticEulerIntegrator integrator;

        compute_forces();
        integrator.step_all(bodies_, dt);

        time_ += dt;
        step_count_++;
    }

    // Run simulation for specified time duration
    void run(double duration, double dt) {
        int steps = static_cast<int>(duration / dt);
        for (int i = 0; i < steps; ++i) {
            step_verlet(dt);
        }
    }

    // Run with callback for monitoring (called every n steps)
    using StepCallback = std::function<void(const Simulation&)>;

    void run_with_callback(double duration, double dt, int callback_interval, StepCallback callback) {
        int steps = static_cast<int>(duration / dt);
        for (int i = 0; i < steps; ++i) {
            step_verlet(dt);
            if (callback && (i + 1) % callback_interval == 0) {
                callback(*this);
            }
        }
    }

    // Energy calculations
    double total_kinetic_energy() const {
        double ke = 0.0;
        for (const auto& body : bodies_) {
            ke += body.kinetic_energy();
        }
        return ke;
    }

    double total_potential_energy() const {
        double pe = 0.0;
        for (size_t i = 0; i < bodies_.size(); ++i) {
            for (size_t j = i + 1; j < bodies_.size(); ++j) {
                double r = bodies_[i].position.distance(bodies_[j].position);
                double soft_r = std::sqrt(r * r + softening_ * softening_);
                if (soft_r > 1e-20) {
                    pe -= G_ * bodies_[i].mass * bodies_[j].mass / soft_r;
                }
            }
        }
        return pe;
    }

    double total_energy() const {
        return total_kinetic_energy() + total_potential_energy();
    }

    // Momentum (should be conserved)
    Vec3 total_momentum() const {
        Vec3 p = Vec3::zero();
        for (const auto& body : bodies_) {
            p = p.add(body.momentum());
        }
        return p;
    }

    // Angular momentum about origin (should be conserved)
    Vec3 total_angular_momentum() const {
        Vec3 L = Vec3::zero();
        for (const auto& body : bodies_) {
            L = L.add(body.angular_momentum());
        }
        return L;
    }

    // Center of mass
    Vec3 center_of_mass() const {
        double total_mass = 0.0;
        Vec3 weighted_sum = Vec3::zero();

        for (const auto& body : bodies_) {
            total_mass += body.mass;
            weighted_sum = weighted_sum.add(body.position.scale(body.mass));
        }

        if (total_mass < 1e-30) return Vec3::zero();
        return weighted_sum.scale(1.0 / total_mass);
    }

    // Center of mass velocity
    Vec3 center_of_mass_velocity() const {
        Vec3 p = total_momentum();
        double total_mass = 0.0;
        for (const auto& body : bodies_) {
            total_mass += body.mass;
        }
        if (total_mass < 1e-30) return Vec3::zero();
        return p.scale(1.0 / total_mass);
    }

    // Find closest approach between any two bodies
    double minimum_separation() const {
        double min_dist = std::numeric_limits<double>::max();
        for (size_t i = 0; i < bodies_.size(); ++i) {
            for (size_t j = i + 1; j < bodies_.size(); ++j) {
                double dist = bodies_[i].position.distance(bodies_[j].position);
                min_dist = std::min(min_dist, dist);
            }
        }
        return min_dist;
    }

    // Get orbital elements of body around most massive other body
    OrbitalElements orbital_elements_of(int body_index) const {
        if (body_index < 0 || body_index >= static_cast<int>(bodies_.size())) {
            return OrbitalElements{};
        }

        // Find most massive body (assumed to be central body)
        int central_index = -1;
        double max_mass = 0.0;
        for (size_t i = 0; i < bodies_.size(); ++i) {
            if (static_cast<int>(i) != body_index && bodies_[i].mass > max_mass) {
                max_mass = bodies_[i].mass;
                central_index = static_cast<int>(i);
            }
        }

        if (central_index < 0) return OrbitalElements{};

        // Compute relative position and velocity
        Vec3 rel_pos = bodies_[body_index].position.sub(bodies_[central_index].position);
        Vec3 rel_vel = bodies_[body_index].velocity.sub(bodies_[central_index].velocity);

        return OrbitalElements::from_state_vectors(rel_pos, rel_vel, max_mass, G_);
    }

    // Access to raw bodies for advanced operations
    const std::vector<Body>& bodies() const { return bodies_; }
    std::vector<Body>& bodies() { return bodies_; }
};


// Factory functions for common solar system setups

// Create a two-body system with circular orbit
inline Simulation create_binary_system(
    double m1, double m2,
    double separation,
    double G = Constants::G
) {
    Simulation sim(G);

    // Place at center of mass
    double total_mass = m1 + m2;
    double r1 = separation * m2 / total_mass;
    double r2 = separation * m1 / total_mass;

    // Orbital velocity for circular orbit
    double v1 = std::sqrt(G * m2 * m2 / (total_mass * separation));
    double v2 = std::sqrt(G * m1 * m1 / (total_mass * separation));

    sim.add_body("Body1", m1, Vec3(-r1, 0, 0), Vec3(0, -v1, 0));
    sim.add_body("Body2", m2, Vec3(r2, 0, 0), Vec3(0, v2, 0));

    return sim;
}

// Create simplified inner solar system
inline Simulation create_solar_system_inner() {
    Simulation sim(Constants::G);

    // Sun at origin (fixed)
    Body sun("Sun", Constants::SOLAR_MASS, Vec3::zero(), Vec3::zero());
    sun.fixed = true;
    sim.add_body(sun);

    // Mercury
    double mercury_dist = 0.387 * Constants::AU;
    double mercury_vel = std::sqrt(Constants::G * Constants::SOLAR_MASS / mercury_dist);
    sim.add_body("Mercury", 3.285e23, Vec3(mercury_dist, 0, 0), Vec3(0, mercury_vel, 0));

    // Venus
    double venus_dist = 0.723 * Constants::AU;
    double venus_vel = std::sqrt(Constants::G * Constants::SOLAR_MASS / venus_dist);
    sim.add_body("Venus", 4.867e24, Vec3(venus_dist, 0, 0), Vec3(0, venus_vel, 0));

    // Earth
    double earth_dist = Constants::AU;
    double earth_vel = std::sqrt(Constants::G * Constants::SOLAR_MASS / earth_dist);
    sim.add_body("Earth", Constants::EARTH_MASS, Vec3(earth_dist, 0, 0), Vec3(0, earth_vel, 0));

    // Mars
    double mars_dist = 1.524 * Constants::AU;
    double mars_vel = std::sqrt(Constants::G * Constants::SOLAR_MASS / mars_dist);
    sim.add_body("Mars", 6.39e23, Vec3(mars_dist, 0, 0), Vec3(0, mars_vel, 0));

    return sim;
}

// Create a cluster of bodies with random positions/velocities
inline Simulation create_random_cluster(
    int n_bodies,
    double total_mass,
    double radius,
    double velocity_dispersion,
    double G = Constants::G
) {
    Simulation sim(G);

    double mass_per_body = total_mass / n_bodies;

    // Simple pseudo-random distribution (not using <random> for simplicity)
    auto pseudo_random = [](int seed) -> double {
        // Linear congruential generator
        static unsigned int state = 12345;
        state = state * 1103515245 + seed;
        return (state % 10000) / 10000.0;
    };

    for (int i = 0; i < n_bodies; ++i) {
        // Random position in sphere
        double r = radius * std::cbrt(pseudo_random(i * 3));
        double theta = std::acos(2.0 * pseudo_random(i * 3 + 1) - 1.0);
        double phi = 2.0 * 3.14159265359 * pseudo_random(i * 3 + 2);

        Vec3 pos = Vec3::from_spherical(r, theta, phi);

        // Random velocity
        double vr = velocity_dispersion * (pseudo_random(i * 3 + 100) - 0.5) * 2;
        double vtheta = velocity_dispersion * (pseudo_random(i * 3 + 101) - 0.5) * 2;
        double vphi = velocity_dispersion * (pseudo_random(i * 3 + 102) - 0.5) * 2;

        Vec3 vel(vr, vtheta, vphi);

        sim.add_body("Body" + std::to_string(i), mass_per_body, pos, vel);
    }

    // Set softening to prevent close encounters
    sim.set_softening(radius / (n_bodies * 2));

    return sim;
}

} // namespace nbody
