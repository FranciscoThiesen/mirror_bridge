#pragma once

// Test class for new Mirror Bridge features
// Used to test: stubgen, options (exclude/rename/readonly), buffer views

#include <string>
#include <vector>
#include <cstdint>
#include <cmath>

struct TestClass {
    // Public data members
    int id = 0;
    std::string name = "default";
    double value = 0.0;

    // Member to be renamed (camelCase -> snake_case)
    int memberCount = 0;

    // Member to be marked readonly
    int computedValue = 42;

    // Constructors
    TestClass() = default;

    TestClass(int i, std::string n)
        : id(i), name(std::move(n)) {}

    TestClass(int i, std::string n, double v)
        : id(i), name(std::move(n)), value(v) {}

    // Methods
    void set_id(int new_id) { id = new_id; }
    int get_id() const { return id; }

    void set_name(const std::string& new_name) { name = new_name; }
    std::string get_name() const { return name; }

    double calculate(double x, double y) const {
        return x * y + value;
    }

    // Method returning a byte buffer (for buffer protocol testing)
    std::vector<uint8_t> get_bytes() const {
        std::vector<uint8_t> result(100);
        for (size_t i = 0; i < result.size(); ++i) {
            result[i] = static_cast<uint8_t>(i);
        }
        return result;
    }

    // Method returning float buffer
    std::vector<float> get_floats() const {
        return {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    }

    // Static method
    static int static_add(int a, int b) {
        return a + b;
    }

    // Static constant
    static constexpr int VERSION = 1;
};

// Another class for testing multiple classes in stub generation
struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    Vec2() = default;
    Vec2(double x_, double y_) : x(x_), y(y_) {}

    double dot(const Vec2& other) const {
        return x * other.x + y * other.y;
    }

    double length() const {
        return std::sqrt(x * x + y * y);
    }

    Vec2 add(const Vec2& other) const {
        return Vec2(x + other.x, y + other.y);
    }
};
