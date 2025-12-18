#pragma once
#include <cmath>
#include <string>

// Example: Static Methods and Constexpr Members
// Demonstrates class-level constants and factory methods

namespace math {

// Mathematical constants as a class
class Constants {
public:
    // Static constexpr members (compile-time constants)
    static constexpr double PI = 3.14159265358979323846;
    static constexpr double E = 2.71828182845904523536;
    static constexpr double PHI = 1.61803398874989484820;  // Golden ratio
    static constexpr double SQRT2 = 1.41421356237309504880;

    // Integer constants
    static constexpr int MAX_ITERATIONS = 1000;
    static constexpr int DEFAULT_PRECISION = 15;

    // Boolean constants
    static constexpr bool DEBUG_MODE = false;

    // Instance method using constants
    double circle_area(double radius) const {
        return PI * radius * radius;
    }

    double circle_circumference(double radius) const {
        return 2.0 * PI * radius;
    }
};

// Class with static factory methods
class Point2D {
public:
    double x = 0.0;
    double y = 0.0;

    // Static factory methods
    static Point2D origin() {
        return Point2D{0.0, 0.0};
    }

    static Point2D from_polar(double r, double theta) {
        return Point2D{
            r * std::cos(theta),
            r * std::sin(theta)
        };
    }

    static Point2D unit_x() {
        return Point2D{1.0, 0.0};
    }

    static Point2D unit_y() {
        return Point2D{0.0, 1.0};
    }

    // Static utility methods
    static double distance(const Point2D& a, const Point2D& b) {
        double dx = b.x - a.x;
        double dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    static Point2D midpoint(const Point2D& a, const Point2D& b) {
        return Point2D{(a.x + b.x) / 2.0, (a.y + b.y) / 2.0};
    }

    // Instance methods
    double magnitude() const {
        return std::sqrt(x * x + y * y);
    }

    double angle() const {
        return std::atan2(y, x);
    }

    Point2D normalized() const {
        double mag = magnitude();
        if (mag > 0) {
            return Point2D{x / mag, y / mag};
        }
        return Point2D{0.0, 0.0};
    }
};

// Configuration class with static members
class Config {
public:
    // Version info as constexpr
    static constexpr int VERSION_MAJOR = 2;
    static constexpr int VERSION_MINOR = 1;
    static constexpr int VERSION_PATCH = 0;

    // Static method to get version string
    static std::string version_string() {
        return std::to_string(VERSION_MAJOR) + "." +
               std::to_string(VERSION_MINOR) + "." +
               std::to_string(VERSION_PATCH);
    }

    // Computed version number
    static int version_number() {
        return VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_PATCH;
    }

    // Instance members for runtime config
    int timeout_ms = 5000;
    bool verbose = false;
    std::string name = "default";

    // Instance method using static
    std::string full_name() const {
        return name + " v" + version_string();
    }
};

// Counter with class-level state
class Counter {
public:
    static int total_count;  // Tracks across all instances

    int value = 0;

    void increment() {
        value++;
        total_count++;
    }

    void decrement() {
        value--;
        total_count--;
    }

    static void reset_total() {
        total_count = 0;
    }

    static int get_total() {
        return total_count;
    }
};

// Define static member
inline int Counter::total_count = 0;

} // namespace math
