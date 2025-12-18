#pragma once
#include <cmath>

// A 2D vector class - the star of our multi-language demo
struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    // Constructors
    Vec2() = default;
    Vec2(double x_, double y_) : x(x_), y(y_) {}

    // Length/magnitude
    double length() const {
        return std::sqrt(x * x + y * y);
    }

    // Normalize to unit vector
    Vec2 normalized() const {
        double len = length();
        if (len > 0) {
            return Vec2{x / len, y / len};
        }
        return Vec2{0.0, 0.0};
    }

    // Dot product
    double dot(const Vec2& other) const {
        return x * other.x + y * other.y;
    }

    // Scale by scalar
    Vec2 scale(double factor) const {
        return Vec2{x * factor, y * factor};
    }

    // Add two vectors
    Vec2 add(const Vec2& other) const {
        return Vec2{x + other.x, y + other.y};
    }

    // Distance to another vector
    double distance_to(const Vec2& other) const {
        double dx = other.x - x;
        double dy = other.y - y;
        return std::sqrt(dx * dx + dy * dy);
    }
};
