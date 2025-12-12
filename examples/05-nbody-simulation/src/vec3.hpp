#pragma once
#include <cmath>
#include <string>
#include <stdexcept>

namespace nbody {

// 3D vector with full mathematical operations
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    // Vector addition
    Vec3 add(Vec3 other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    // Vector subtraction
    Vec3 sub(Vec3 other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    // Scalar multiplication
    Vec3 scale(double s) const {
        return Vec3(x * s, y * s, z * s);
    }

    // Component-wise multiplication
    Vec3 mul(Vec3 other) const {
        return Vec3(x * other.x, y * other.y, z * other.z);
    }

    // Dot product
    double dot(Vec3 other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    // Cross product
    Vec3 cross(Vec3 other) const {
        return Vec3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    // Magnitude (length)
    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    // Squared magnitude (avoids sqrt for comparisons)
    double length_squared() const {
        return x * x + y * y + z * z;
    }

    // Distance to another vector
    double distance(Vec3 other) const {
        return sub(other).length();
    }

    // Squared distance (avoids sqrt)
    double distance_squared(Vec3 other) const {
        return sub(other).length_squared();
    }

    // Unit vector (normalized)
    Vec3 normalized() const {
        double len = length();
        if (len < 1e-12) {
            return Vec3(0, 0, 0);
        }
        return scale(1.0 / len);
    }

    // In-place normalization, returns length before normalizing
    double normalize() {
        double len = length();
        if (len > 1e-12) {
            x /= len;
            y /= len;
            z /= len;
        }
        return len;
    }

    // Linear interpolation
    Vec3 lerp(Vec3 other, double t) const {
        return Vec3(
            x + (other.x - x) * t,
            y + (other.y - y) * t,
            z + (other.z - z) * t
        );
    }

    // Reflection around a normal
    Vec3 reflect(Vec3 normal) const {
        Vec3 n = normal.normalized();
        return sub(n.scale(2.0 * dot(n)));
    }

    // Angle between vectors in radians
    double angle(Vec3 other) const {
        double len_product = length() * other.length();
        if (len_product < 1e-12) return 0.0;
        double cos_angle = dot(other) / len_product;
        // Clamp for numerical stability
        cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
        return std::acos(cos_angle);
    }

    // Project this vector onto another
    Vec3 project(Vec3 onto) const {
        double len_sq = onto.length_squared();
        if (len_sq < 1e-12) return Vec3(0, 0, 0);
        return onto.scale(dot(onto) / len_sq);
    }

    // Reject this vector from another (perpendicular component)
    Vec3 reject(Vec3 from) const {
        return sub(project(from));
    }

    // Check if approximately zero
    bool is_zero(double epsilon = 1e-10) const {
        return length_squared() < epsilon * epsilon;
    }

    // Check approximate equality
    bool approx_equal(Vec3 other, double epsilon = 1e-10) const {
        return sub(other).is_zero(epsilon);
    }

    // Clamp each component
    Vec3 clamp(double min_val, double max_val) const {
        return Vec3(
            std::max(min_val, std::min(max_val, x)),
            std::max(min_val, std::min(max_val, y)),
            std::max(min_val, std::min(max_val, z))
        );
    }

    // Clamp length to maximum
    Vec3 clamp_length(double max_len) const {
        double len = length();
        if (len > max_len && len > 1e-12) {
            return scale(max_len / len);
        }
        return *this;
    }

    std::string to_string() const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Vec3(%.6g, %.6g, %.6g)", x, y, z);
        return std::string(buf);
    }

    // Static factory methods
    static Vec3 zero() { return Vec3(0, 0, 0); }
    static Vec3 one() { return Vec3(1, 1, 1); }
    static Vec3 unit_x() { return Vec3(1, 0, 0); }
    static Vec3 unit_y() { return Vec3(0, 1, 0); }
    static Vec3 unit_z() { return Vec3(0, 0, 1); }

    // Create from spherical coordinates (r, theta, phi)
    // theta: polar angle from +z axis (0 to pi)
    // phi: azimuthal angle from +x axis (0 to 2*pi)
    static Vec3 from_spherical(double r, double theta, double phi) {
        double sin_theta = std::sin(theta);
        return Vec3(
            r * sin_theta * std::cos(phi),
            r * sin_theta * std::sin(phi),
            r * std::cos(theta)
        );
    }
};

} // namespace nbody
