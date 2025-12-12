#pragma once
#include "simulation.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>

namespace nbody {

// Time series data point for tracking simulation quantities
struct DataPoint {
    double time = 0.0;
    double value = 0.0;
};

// Statistics tracker for monitoring simulation health and conservation laws
class SimulationStats {
private:
    std::vector<DataPoint> energy_history_;
    std::vector<DataPoint> momentum_history_;
    std::vector<DataPoint> angular_momentum_history_;

    double initial_energy_ = 0.0;
    Vec3 initial_momentum_ = Vec3::zero();
    Vec3 initial_angular_momentum_ = Vec3::zero();
    bool initialized_ = false;

public:
    void initialize(const Simulation& sim) {
        initial_energy_ = sim.total_energy();
        initial_momentum_ = sim.total_momentum();
        initial_angular_momentum_ = sim.total_angular_momentum();

        energy_history_.clear();
        momentum_history_.clear();
        angular_momentum_history_.clear();

        record(sim);
        initialized_ = true;
    }

    void record(const Simulation& sim) {
        double t = sim.time();

        energy_history_.push_back({t, sim.total_energy()});
        momentum_history_.push_back({t, sim.total_momentum().length()});
        angular_momentum_history_.push_back({t, sim.total_angular_momentum().length()});
    }

    bool is_initialized() const { return initialized_; }

    // Energy conservation metrics
    double initial_energy() const { return initial_energy_; }

    double current_energy(const Simulation& sim) const {
        return sim.total_energy();
    }

    double energy_error(const Simulation& sim) const {
        if (std::abs(initial_energy_) < 1e-30) return 0.0;
        return std::abs((sim.total_energy() - initial_energy_) / initial_energy_);
    }

    double max_energy_error() const {
        if (energy_history_.empty() || std::abs(initial_energy_) < 1e-30) return 0.0;

        double max_err = 0.0;
        for (const auto& dp : energy_history_) {
            double err = std::abs((dp.value - initial_energy_) / initial_energy_);
            max_err = std::max(max_err, err);
        }
        return max_err;
    }

    // Momentum conservation
    Vec3 initial_momentum() const { return initial_momentum_; }

    double momentum_error(const Simulation& sim) const {
        double initial_mag = initial_momentum_.length();
        if (initial_mag < 1e-30) {
            return sim.total_momentum().length();
        }
        Vec3 diff = sim.total_momentum().sub(initial_momentum_);
        return diff.length() / initial_mag;
    }

    // Angular momentum conservation
    Vec3 initial_angular_momentum() const { return initial_angular_momentum_; }

    double angular_momentum_error(const Simulation& sim) const {
        double initial_mag = initial_angular_momentum_.length();
        if (initial_mag < 1e-30) {
            return sim.total_angular_momentum().length();
        }
        Vec3 diff = sim.total_angular_momentum().sub(initial_angular_momentum_);
        return diff.length() / initial_mag;
    }

    // History access
    std::vector<double> energy_history_times() const {
        std::vector<double> times;
        times.reserve(energy_history_.size());
        for (const auto& dp : energy_history_) {
            times.push_back(dp.time);
        }
        return times;
    }

    std::vector<double> energy_history_values() const {
        std::vector<double> values;
        values.reserve(energy_history_.size());
        for (const auto& dp : energy_history_) {
            values.push_back(dp.value);
        }
        return values;
    }

    int history_length() const {
        return static_cast<int>(energy_history_.size());
    }

    // Summary statistics
    std::string conservation_report(const Simulation& sim) const {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "Conservation Report (t=%.3e, %d steps):\n"
            "  Energy: initial=%.6e, current=%.6e, error=%.2e%%\n"
            "  Momentum: |p|=%.6e, error=%.2e%%\n"
            "  Angular Momentum: |L|=%.6e, error=%.2e%%\n"
            "  Max energy drift: %.2e%%",
            sim.time(), sim.step_count(),
            initial_energy_, sim.total_energy(), energy_error(sim) * 100,
            sim.total_momentum().length(), momentum_error(sim) * 100,
            sim.total_angular_momentum().length(), angular_momentum_error(sim) * 100,
            max_energy_error() * 100
        );
        return std::string(buf);
    }
};


// Virial theorem analysis for gravitational systems
// In equilibrium: 2<KE> + <PE> = 0
struct VirialAnalysis {
    double kinetic_energy = 0.0;
    double potential_energy = 0.0;
    double virial_ratio = 0.0;  // 2*KE / |PE|, should be ~1 at equilibrium

    static VirialAnalysis compute(const Simulation& sim) {
        VirialAnalysis result;
        result.kinetic_energy = sim.total_kinetic_energy();
        result.potential_energy = sim.total_potential_energy();

        if (std::abs(result.potential_energy) > 1e-30) {
            result.virial_ratio = 2.0 * result.kinetic_energy / std::abs(result.potential_energy);
        }

        return result;
    }

    bool is_bound() const {
        // System is bound if total energy < 0
        return (kinetic_energy + potential_energy) < 0;
    }

    bool is_virialized() const {
        // Approximately virialized if ratio is close to 1
        return std::abs(virial_ratio - 1.0) < 0.2;
    }

    std::string to_string() const {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "Virial(KE=%.3e, PE=%.3e, 2KE/|PE|=%.3f, bound=%s, virialized=%s)",
            kinetic_energy, potential_energy, virial_ratio,
            is_bound() ? "yes" : "no",
            is_virialized() ? "yes" : "no"
        );
        return std::string(buf);
    }
};


// Lagrange point calculator for restricted three-body problem
// (two massive bodies + test particle)
struct LagrangePoints {
    Vec3 L1;  // Between the two bodies
    Vec3 L2;  // Beyond the smaller body
    Vec3 L3;  // Beyond the larger body
    Vec3 L4;  // Leading equilateral point
    Vec3 L5;  // Trailing equilateral point

    // Compute for two bodies at positions p1, p2 with masses m1 >= m2
    static LagrangePoints compute(Vec3 p1, Vec3 p2, double m1, double m2) {
        LagrangePoints lp;

        // Ensure m1 is the larger mass
        if (m1 < m2) {
            std::swap(m1, m2);
            std::swap(p1, p2);
        }

        Vec3 r12 = p2.sub(p1);
        double d = r12.length();

        if (d < 1e-10) return lp;

        Vec3 dir = r12.normalized();

        // Mass ratio
        double mu = m2 / (m1 + m2);

        // L1: between bodies
        // Approximate: r1 ≈ d * (1 - (mu/3)^(1/3))
        double r1_ratio = 1.0 - std::cbrt(mu / 3.0);
        lp.L1 = p1.add(dir.scale(d * r1_ratio));

        // L2: beyond smaller body
        double r2_ratio = 1.0 + std::cbrt(mu / 3.0);
        lp.L2 = p1.add(dir.scale(d * r2_ratio));

        // L3: beyond larger body
        double r3_ratio = -1.0 - 5.0 * mu / 12.0;
        lp.L3 = p1.add(dir.scale(d * r3_ratio));

        // L4 and L5: equilateral points
        // Form equilateral triangle with the two bodies
        Vec3 perp = Vec3::unit_z().cross(dir);
        if (perp.length() < 0.1) {
            perp = Vec3::unit_y().cross(dir);
        }
        perp = perp.normalized();

        Vec3 midpoint = p1.add(p2).scale(0.5);
        double height = d * std::sqrt(3.0) / 2.0;

        lp.L4 = midpoint.add(perp.scale(height));   // Leading
        lp.L5 = midpoint.sub(perp.scale(height));   // Trailing

        return lp;
    }
};


// Statistical analysis of body distribution
struct ClusterStatistics {
    int body_count = 0;
    double total_mass = 0.0;

    Vec3 center_of_mass;
    Vec3 center_of_mass_velocity;

    double mean_radius = 0.0;      // Mean distance from CoM
    double rms_radius = 0.0;       // Root mean square radius
    double half_mass_radius = 0.0; // Radius containing 50% of mass

    double velocity_dispersion = 0.0;  // RMS velocity relative to CoM
    double crossing_time = 0.0;        // Characteristic time to cross cluster

    static ClusterStatistics compute(const Simulation& sim) {
        ClusterStatistics stats;
        const auto& bodies = sim.bodies();

        stats.body_count = static_cast<int>(bodies.size());
        if (stats.body_count == 0) return stats;

        // Total mass and center of mass
        for (const auto& body : bodies) {
            stats.total_mass += body.mass;
        }

        stats.center_of_mass = sim.center_of_mass();
        stats.center_of_mass_velocity = sim.center_of_mass_velocity();

        // Radial distribution
        std::vector<std::pair<double, double>> radii_mass;  // (radius, mass)
        double sum_r = 0.0;
        double sum_r2 = 0.0;

        for (const auto& body : bodies) {
            double r = body.position.distance(stats.center_of_mass);
            radii_mass.push_back({r, body.mass});
            sum_r += r * body.mass;
            sum_r2 += r * r * body.mass;
        }

        stats.mean_radius = sum_r / stats.total_mass;
        stats.rms_radius = std::sqrt(sum_r2 / stats.total_mass);

        // Half-mass radius
        std::sort(radii_mass.begin(), radii_mass.end());
        double cumulative_mass = 0.0;
        double half_mass = stats.total_mass / 2.0;

        for (const auto& [r, m] : radii_mass) {
            cumulative_mass += m;
            if (cumulative_mass >= half_mass) {
                stats.half_mass_radius = r;
                break;
            }
        }

        // Velocity dispersion
        double sum_v2 = 0.0;
        for (const auto& body : bodies) {
            Vec3 rel_vel = body.velocity.sub(stats.center_of_mass_velocity);
            sum_v2 += rel_vel.length_squared() * body.mass;
        }
        stats.velocity_dispersion = std::sqrt(sum_v2 / stats.total_mass);

        // Crossing time estimate
        if (stats.velocity_dispersion > 1e-20) {
            stats.crossing_time = 2.0 * stats.half_mass_radius / stats.velocity_dispersion;
        }

        return stats;
    }

    std::string to_string() const {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "ClusterStats(n=%d, M=%.3e):\n"
            "  CoM: %s\n"
            "  <r>=%.3e, r_rms=%.3e, r_half=%.3e\n"
            "  sigma_v=%.3e, t_cross=%.3e",
            body_count, total_mass,
            center_of_mass.to_string().c_str(),
            mean_radius, rms_radius, half_mass_radius,
            velocity_dispersion, crossing_time
        );
        return std::string(buf);
    }
};

} // namespace nbody
