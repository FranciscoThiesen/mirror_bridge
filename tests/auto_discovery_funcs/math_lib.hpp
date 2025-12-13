#pragma once
#include <cmath>
#include <vector>
#include <string>

namespace mathlib {

// A simple point class
struct Point {
    double x = 0.0;
    double y = 0.0;

    double length() const {
        return std::sqrt(x*x + y*y);
    }
};

// Free function: compute distance between two points
inline double distance(const Point& a, const Point& b) {
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    return std::sqrt(dx*dx + dy*dy);
}

// Free function: create a point
inline Point make_point(double x, double y) {
    return Point{x, y};
}

// Free function: add two points
inline Point add_points(const Point& a, const Point& b) {
    return Point{a.x + b.x, a.y + b.y};
}

// Free function: scale a point
inline Point scale_point(const Point& p, double factor) {
    return Point{p.x * factor, p.y * factor};
}

// Simple math functions
inline double add(double a, double b) {
    return a + b;
}

inline double multiply(double a, double b) {
    return a * b;
}

inline double clamp(double value, double min_val, double max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

// This namespace should be skipped (detail pattern)
namespace detail {
    inline double internal_helper(double x) {
        return x * 2;
    }
}

// Another class
struct Rectangle {
    double width = 0.0;
    double height = 0.0;

    double area() const {
        return width * height;
    }
};

// Free function for Rectangle
inline double perimeter(const Rectangle& r) {
    return 2.0 * (r.width + r.height);
}

} // namespace mathlib
