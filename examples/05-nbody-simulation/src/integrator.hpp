#pragma once
#include "body.hpp"
#include <vector>
#include <functional>
#include <string>

namespace nbody {

// Function signature for computing acceleration given position
// Used by higher-order integrators that need to evaluate forces at intermediate positions
using AccelerationFunction = std::function<Vec3(const Body&, Vec3)>;


// Numerical integration methods for N-body simulation
// Each method has different accuracy/stability/performance tradeoffs


// Forward Euler integration (1st order)
// Simple but inaccurate, energy drifts over time
// O(h) local error, O(1) global error
struct EulerIntegrator {
    std::string name = "Euler";
    int order = 1;

    void step(Body& body, double dt) const {
        if (body.fixed) return;

        // x(t+dt) = x(t) + v(t) * dt
        body.position = body.position.add(body.velocity.scale(dt));

        // v(t+dt) = v(t) + a(t) * dt
        body.velocity = body.velocity.add(body.acceleration.scale(dt));
    }

    void step_all(std::vector<Body>& bodies, double dt) const {
        for (auto& body : bodies) {
            step(body, dt);
        }
    }
};


// Semi-implicit Euler (Symplectic Euler)
// Updates velocity first, then position with new velocity
// Better energy conservation than forward Euler
struct SymplecticEulerIntegrator {
    std::string name = "SymplecticEuler";
    int order = 1;

    void step(Body& body, double dt) const {
        if (body.fixed) return;

        // v(t+dt) = v(t) + a(t) * dt (update velocity first)
        body.velocity = body.velocity.add(body.acceleration.scale(dt));

        // x(t+dt) = x(t) + v(t+dt) * dt (use new velocity)
        body.position = body.position.add(body.velocity.scale(dt));
    }

    void step_all(std::vector<Body>& bodies, double dt) const {
        for (auto& body : bodies) {
            step(body, dt);
        }
    }
};


// Velocity Verlet integration (2nd order, symplectic)
// Excellent energy conservation for conservative systems
// O(h²) local error, O(h²) global error
// Standard choice for molecular dynamics and orbital mechanics
struct VerletIntegrator {
    std::string name = "Verlet";
    int order = 2;

    // Requires storing previous acceleration
    // First half: update position and half-update velocity
    void step_position(Body& body, double dt) const {
        if (body.fixed) return;

        // x(t+dt) = x(t) + v(t)*dt + 0.5*a(t)*dt²
        body.position = body.position
            .add(body.velocity.scale(dt))
            .add(body.acceleration.scale(0.5 * dt * dt));

        // v(t+dt/2) = v(t) + 0.5*a(t)*dt
        body.velocity = body.velocity.add(body.acceleration.scale(0.5 * dt));
    }

    // Second half: complete velocity update with new acceleration
    void step_velocity(Body& body, double dt) const {
        if (body.fixed) return;

        // v(t+dt) = v(t+dt/2) + 0.5*a(t+dt)*dt
        body.velocity = body.velocity.add(body.acceleration.scale(0.5 * dt));
    }

    // Convenience method for single body (forces must be recomputed between calls)
    void step_positions(std::vector<Body>& bodies, double dt) const {
        for (auto& body : bodies) {
            step_position(body, dt);
        }
    }

    void step_velocities(std::vector<Body>& bodies, double dt) const {
        for (auto& body : bodies) {
            step_velocity(body, dt);
        }
    }
};


// 4th order Runge-Kutta integration
// High accuracy but not symplectic (energy may drift)
// O(h⁴) local error, O(h⁴) global error
// Good for short simulations requiring precision
struct RK4Integrator {
    std::string name = "RK4";
    int order = 4;

    // Full RK4 step (requires acceleration function that can evaluate at any position)
    void step(Body& body, double dt, const AccelerationFunction& accel_fn) const {
        if (body.fixed) return;

        Vec3 x0 = body.position;
        Vec3 v0 = body.velocity;

        // k1: evaluate at current state
        Vec3 a1 = accel_fn(body, x0);
        Vec3 k1_v = a1;
        Vec3 k1_x = v0;

        // k2: evaluate at midpoint using k1
        Vec3 x2 = x0.add(k1_x.scale(dt / 2));
        Vec3 v2 = v0.add(k1_v.scale(dt / 2));
        Vec3 a2 = accel_fn(body, x2);
        Vec3 k2_v = a2;
        Vec3 k2_x = v2;

        // k3: evaluate at midpoint using k2
        Vec3 x3 = x0.add(k2_x.scale(dt / 2));
        Vec3 v3 = v0.add(k2_v.scale(dt / 2));
        Vec3 a3 = accel_fn(body, x3);
        Vec3 k3_v = a3;
        Vec3 k3_x = v3;

        // k4: evaluate at endpoint using k3
        Vec3 x4 = x0.add(k3_x.scale(dt));
        Vec3 v4 = v0.add(k3_v.scale(dt));
        Vec3 a4 = accel_fn(body, x4);
        Vec3 k4_v = a4;
        Vec3 k4_x = v4;

        // Weighted average: y(t+dt) = y(t) + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
        body.position = x0.add(
            k1_x.add(k2_x.scale(2)).add(k3_x.scale(2)).add(k4_x).scale(dt / 6)
        );
        body.velocity = v0.add(
            k1_v.add(k2_v.scale(2)).add(k3_v.scale(2)).add(k4_v).scale(dt / 6)
        );
    }
};


// Leapfrog integration (2nd order, symplectic)
// Alternative formulation of Verlet, positions and velocities offset by dt/2
// Used in particle-in-cell codes and cosmological simulations
struct LeapfrogIntegrator {
    std::string name = "Leapfrog";
    int order = 2;
    bool initialized = false;

    // Initialize: offset velocity by -dt/2 to start leapfrog
    void initialize(std::vector<Body>& bodies, double dt) {
        for (auto& body : bodies) {
            if (!body.fixed) {
                // v(-dt/2) = v(0) - a(0) * dt/2
                body.velocity = body.velocity.sub(body.acceleration.scale(dt / 2));
            }
        }
        initialized = true;
    }

    // Kick: update velocity by full timestep
    void kick(Body& body, double dt) const {
        if (body.fixed) return;
        body.velocity = body.velocity.add(body.acceleration.scale(dt));
    }

    // Drift: update position by full timestep
    void drift(Body& body, double dt) const {
        if (body.fixed) return;
        body.position = body.position.add(body.velocity.scale(dt));
    }

    // Combined kick-drift (standard leapfrog step)
    void step(Body& body, double dt) const {
        kick(body, dt);
        drift(body, dt);
    }

    void step_all(std::vector<Body>& bodies, double dt) const {
        for (auto& body : bodies) {
            step(body, dt);
        }
    }
};


// Adaptive timestep controller using error estimation
struct AdaptiveTimestep {
    double dt_min = 1e-10;      // Minimum allowed timestep
    double dt_max = 1e6;        // Maximum allowed timestep
    double tolerance = 1e-6;    // Desired accuracy
    double safety = 0.9;        // Safety factor for timestep adjustment

    // Estimate error by comparing two half-steps to one full step
    // Returns recommended timestep
    double compute_new_dt(double error, double current_dt) const {
        if (error < 1e-15) {
            return std::min(current_dt * 2.0, dt_max);
        }

        // Optimal step size for 2nd order method: dt_new = dt * (tol/err)^(1/2)
        double factor = safety * std::sqrt(tolerance / error);
        factor = std::clamp(factor, 0.1, 4.0);  // Don't change too drastically

        double new_dt = current_dt * factor;
        return std::clamp(new_dt, dt_min, dt_max);
    }

    // Estimate velocity error between two integration results
    double estimate_error(const std::vector<Body>& fine, const std::vector<Body>& coarse) const {
        double max_error = 0.0;

        for (size_t i = 0; i < fine.size(); ++i) {
            // Relative velocity error
            Vec3 v_diff = fine[i].velocity.sub(coarse[i].velocity);
            double v_mag = fine[i].velocity.length();

            double rel_error = (v_mag > 1e-10) ? v_diff.length() / v_mag : v_diff.length();
            max_error = std::max(max_error, rel_error);
        }

        return max_error;
    }
};

} // namespace nbody
