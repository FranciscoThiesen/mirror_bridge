#pragma once
#include <string>
#include <vector>
#include <cmath>

namespace overload_test {

// Test class with method overloading
struct Calculator {
    double value = 0.0;

    // Overloaded add methods
    void add(int x) {
        value += x;
    }

    void add(double x) {
        value += x;
    }

    void add(int x, int y) {
        value += x + y;
    }

    // Overloaded set methods with different types
    void set(int x) {
        value = static_cast<double>(x);
    }

    void set(double x) {
        value = x;
    }

    void set(const std::string& s) {
        value = std::stod(s);
    }

    // Non-overloaded method
    double get() const {
        return value;
    }

    void reset() {
        value = 0.0;
    }
};

// Test class with overloaded constructors (for later)
struct Point {
    double x = 0.0;
    double y = 0.0;

    // Different ways to create a point
    static Point from_coords(double x, double y) {
        Point p;
        p.x = x;
        p.y = y;
        return p;
    }

    static Point from_single(double v) {
        Point p;
        p.x = v;
        p.y = v;
        return p;
    }

    double distance() const {
        return std::sqrt(x * x + y * y);
    }
};

} // namespace overload_test
