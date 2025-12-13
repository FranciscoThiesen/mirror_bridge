#pragma once
#include <string>
#include <cstdint>

namespace constexpr_test {

// Test class with various static constexpr members
struct PhysicsConstants {
    // Numeric constexpr constants
    static constexpr double PI = 3.14159265358979323846;
    static constexpr double E = 2.71828182845904523536;
    static constexpr double SPEED_OF_LIGHT = 299792458.0;  // m/s
    static constexpr double PLANCK_CONSTANT = 6.62607015e-34;  // J⋅s

    // Integer constexpr constants
    static constexpr int MAX_ITERATIONS = 1000;
    static constexpr std::size_t BUFFER_SIZE = 4096;
    static constexpr int64_t LARGE_NUMBER = 1234567890123LL;

    // Boolean constexpr
    static constexpr bool DEBUG_MODE = false;

    // Non-static member (should still work)
    double custom_value = 0.0;

    // Methods that use the constants
    double circle_area(double radius) const {
        return PI * radius * radius;
    }

    double circle_circumference(double radius) const {
        return 2.0 * PI * radius;
    }
};

// Another class with mixed members
struct Config {
    static constexpr int VERSION_MAJOR = 2;
    static constexpr int VERSION_MINOR = 5;
    static constexpr int VERSION_PATCH = 0;

    // Non-static data
    int setting_a = 10;
    double setting_b = 20.0;

    int get_version_number() const {
        return VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_PATCH;
    }
};

} // namespace constexpr_test
