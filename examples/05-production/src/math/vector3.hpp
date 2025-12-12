#pragma once
#include <cmath>
#include <string>

namespace mathlib {

struct Vector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    double length() const {
        return std::sqrt(x*x + y*y + z*z);
    }

    double length_squared() const {
        return x*x + y*y + z*z;
    }

    void normalize() {
        double len = length();
        if (len > 0) {
            x /= len;
            y /= len;
            z /= len;
        }
    }

    double dot(double ox, double oy, double oz) const {
        return x*ox + y*oy + z*oz;
    }

    void scale(double factor) {
        x *= factor;
        y *= factor;
        z *= factor;
    }

    void add(double dx, double dy, double dz) {
        x += dx;
        y += dy;
        z += dz;
    }

    std::string to_string() const {
        return "Vector3(" + std::to_string(x) + ", " +
               std::to_string(y) + ", " + std::to_string(z) + ")";
    }
};

} // namespace mathlib
