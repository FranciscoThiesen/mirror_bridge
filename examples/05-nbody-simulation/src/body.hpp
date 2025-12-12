#pragma once
#include "vec3.hpp"
#include <string>
#include <cmath>
#include <numbers>

namespace nbody {

// Physical constants (SI units, can be scaled for simulations)
struct Constants {
    static constexpr double G = 6.67430e-11;         // Gravitational constant (m³/kg/s²)
    static constexpr double AU = 1.495978707e11;     // Astronomical unit (m)
    static constexpr double SOLAR_MASS = 1.989e30;   // Solar mass (kg)
    static constexpr double EARTH_MASS = 5.972e24;   // Earth mass (kg)
    static constexpr double DAY = 86400.0;           // Seconds per day
    static constexpr double YEAR = 365.25 * DAY;     // Seconds per year
};


// A celestial body with Newtonian physics
struct Body {
    Vec3 position;          // Position vector
    Vec3 velocity;          // Velocity vector
    Vec3 acceleration;      // Current acceleration (computed from forces)
    double mass = 1.0;      // Mass (determines gravitational attraction)
    double radius = 1.0;    // Physical radius (for collision detection)
    std::string name;       // Identifier
    bool fixed = false;     // If true, body doesn't move (e.g., central star)

    Body() = default;

    Body(std::string n, double m, Vec3 pos, Vec3 vel)
        : position(pos), velocity(vel), mass(m), name(std::move(n)) {}

    // Kinetic energy: KE = 0.5 * m * v²
    double kinetic_energy() const {
        return 0.5 * mass * velocity.length_squared();
    }

    // Momentum: p = m * v
    Vec3 momentum() const {
        return velocity.scale(mass);
    }

    // Angular momentum about origin: L = r × p
    Vec3 angular_momentum() const {
        return position.cross(momentum());
    }

    // Apply force: F = ma, so a = F/m
    void apply_force(Vec3 force) {
        if (!fixed && mass > 0) {
            Vec3 accel = force.scale(1.0 / mass);
            acceleration = acceleration.add(accel);
        }
    }

    // Reset acceleration (call before accumulating forces each timestep)
    void reset_acceleration() {
        acceleration = Vec3::zero();
    }

    // Speed (magnitude of velocity)
    double speed() const {
        return velocity.length();
    }

    // Distance from origin
    double distance_from_origin() const {
        return position.length();
    }

    // Distance to another body (center to center)
    double distance_to(const Body& other) const {
        return position.distance(other.position);
    }

    // Check collision with another body (overlapping radii)
    bool collides_with(const Body& other) const {
        double dist = distance_to(other);
        return dist < (radius + other.radius);
    }

    // Gravitational force exerted BY this body ON another
    // F = G * m1 * m2 / r² in direction of this body
    Vec3 gravitational_force_on(const Body& other, double G = Constants::G) const {
        Vec3 direction = position.sub(other.position);
        double dist_sq = direction.length_squared();

        // Softening to avoid singularity at small distances
        double softening = (radius + other.radius) * 0.1;
        dist_sq += softening * softening;

        if (dist_sq < 1e-20) return Vec3::zero();

        double force_mag = G * mass * other.mass / dist_sq;
        return direction.normalized().scale(force_mag);
    }

    // Escape velocity at distance r from this body
    double escape_velocity(double r, double G = Constants::G) const {
        if (r < 1e-10) return 0.0;
        return std::sqrt(2.0 * G * mass / r);
    }

    // Orbital velocity for circular orbit at distance r
    double orbital_velocity(double r, double G = Constants::G) const {
        if (r < 1e-10) return 0.0;
        return std::sqrt(G * mass / r);
    }

    // Sphere of influence (Hill sphere approximation)
    // How far from this body its gravity dominates over a more massive body
    double sphere_of_influence(double semi_major_axis, double parent_mass) const {
        if (parent_mass < 1e-10) return 0.0;
        return semi_major_axis * std::pow(mass / (3.0 * parent_mass), 1.0/3.0);
    }

    std::string to_string() const {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "Body('%s', mass=%.3e, pos=%s, vel=%s)",
            name.c_str(), mass,
            position.to_string().c_str(),
            velocity.to_string().c_str());
        return std::string(buf);
    }
};


// Orbital elements for Keplerian orbit description
struct OrbitalElements {
    double semi_major_axis = 0.0;     // a: size of orbit
    double eccentricity = 0.0;        // e: shape (0=circle, <1=ellipse, 1=parabola, >1=hyperbola)
    double inclination = 0.0;         // i: tilt relative to reference plane (radians)
    double longitude_ascending = 0.0; // Ω: where orbit crosses reference plane ascending (radians)
    double argument_periapsis = 0.0;  // ω: orientation of periapsis (radians)
    double true_anomaly = 0.0;        // ν: current position in orbit (radians)

    // Derived quantities
    double periapsis() const { return semi_major_axis * (1.0 - eccentricity); }
    double apoapsis() const { return semi_major_axis * (1.0 + eccentricity); }

    double orbital_period(double central_mass, double G = Constants::G) const {
        if (semi_major_axis <= 0 || central_mass <= 0) return 0.0;
        return 2.0 * std::numbers::pi * std::sqrt(
            std::pow(semi_major_axis, 3) / (G * central_mass)
        );
    }

    // Mean motion (radians per second)
    double mean_motion(double central_mass, double G = Constants::G) const {
        double period = orbital_period(central_mass, G);
        if (period < 1e-10) return 0.0;
        return 2.0 * std::numbers::pi / period;
    }

    // Vis-viva equation: velocity at given distance
    double velocity_at_distance(double r, double central_mass, double G = Constants::G) const {
        if (r < 1e-10 || semi_major_axis < 1e-10) return 0.0;
        return std::sqrt(G * central_mass * (2.0 / r - 1.0 / semi_major_axis));
    }

    std::string to_string() const {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "Orbit(a=%.3e, e=%.4f, i=%.2f°, Ω=%.2f°, ω=%.2f°, ν=%.2f°)",
            semi_major_axis, eccentricity,
            inclination * 180.0 / std::numbers::pi,
            longitude_ascending * 180.0 / std::numbers::pi,
            argument_periapsis * 180.0 / std::numbers::pi,
            true_anomaly * 180.0 / std::numbers::pi);
        return std::string(buf);
    }

    // Compute orbital elements from state vectors
    static OrbitalElements from_state_vectors(
        Vec3 position, Vec3 velocity, double central_mass, double G = Constants::G
    ) {
        OrbitalElements elements;

        double mu = G * central_mass;
        double r = position.length();
        double v = velocity.length();

        if (r < 1e-10 || mu < 1e-10) return elements;

        // Specific orbital energy
        double energy = v * v / 2.0 - mu / r;

        // Semi-major axis from vis-viva
        if (std::abs(energy) > 1e-20) {
            elements.semi_major_axis = -mu / (2.0 * energy);
        }

        // Specific angular momentum
        Vec3 h = position.cross(velocity);
        double h_mag = h.length();

        // Eccentricity vector
        Vec3 e_vec = velocity.cross(h).scale(1.0 / mu).sub(position.normalized());
        elements.eccentricity = e_vec.length();

        // Inclination
        if (h_mag > 1e-10) {
            elements.inclination = std::acos(std::clamp(h.z / h_mag, -1.0, 1.0));
        }

        // Node vector (points to ascending node)
        Vec3 n = Vec3::unit_z().cross(h);
        double n_mag = n.length();

        // Longitude of ascending node
        if (n_mag > 1e-10) {
            elements.longitude_ascending = std::acos(std::clamp(n.x / n_mag, -1.0, 1.0));
            if (n.y < 0) {
                elements.longitude_ascending = 2.0 * std::numbers::pi - elements.longitude_ascending;
            }
        }

        // Argument of periapsis
        if (n_mag > 1e-10 && elements.eccentricity > 1e-10) {
            double cos_omega = n.dot(e_vec) / (n_mag * elements.eccentricity);
            elements.argument_periapsis = std::acos(std::clamp(cos_omega, -1.0, 1.0));
            if (e_vec.z < 0) {
                elements.argument_periapsis = 2.0 * std::numbers::pi - elements.argument_periapsis;
            }
        }

        // True anomaly
        if (elements.eccentricity > 1e-10) {
            double cos_nu = e_vec.dot(position) / (elements.eccentricity * r);
            elements.true_anomaly = std::acos(std::clamp(cos_nu, -1.0, 1.0));
            if (position.dot(velocity) < 0) {
                elements.true_anomaly = 2.0 * std::numbers::pi - elements.true_anomaly;
            }
        }

        return elements;
    }
};

} // namespace nbody
