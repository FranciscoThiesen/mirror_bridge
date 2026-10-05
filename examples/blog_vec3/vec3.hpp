#pragma once
#include <cmath>

struct Vec3 {
    double x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(double x, double y, double z) : x(x), y(y), z(z) {}

    double dot(const Vec3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    double length() const {
        return std::sqrt(x*x + y*y + z*z);
    }

    Vec3 normalize() const {
        double len = length();
        if (len == 0) return Vec3();
        return Vec3(x/len, y/len, z/len);
    }

    // THE KEY: Move the entire hot loop to C++
    // Instead of calling dot() 1M times from Python, do it all in one call
    static double hot_loop(int n) {
        Vec3 direction(1, 1, 1);
        double dir_len = direction.length();
        double total = 0.0;

        for (int i = 0; i < n; ++i) {
            Vec3 v(i * 0.1, i * 0.2, i * 0.3);
            total += v.dot(direction) / dir_len;
        }
        return total;
    }
};
