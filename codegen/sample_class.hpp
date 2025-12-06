// Sample C++ class for codegen integration test
#pragma once

#include <string>
#include <cmath>

class Calculator {
public:
    double value = 0.0;

    Calculator() = default;
    Calculator(double initial) : value(initial) {}

    double add(double x) {
        value += x;
        return value;
    }

    double subtract(double x) {
        value -= x;
        return value;
    }

    double multiply(double x) {
        value *= x;
        return value;
    }

    double get_value() const {
        return value;
    }

    void reset() {
        value = 0.0;
    }

    std::string to_string() const {
        return "Calculator(value=" + std::to_string(value) + ")";
    }
};

class Point2D {
public:
    double x = 0.0;
    double y = 0.0;

    Point2D() = default;
    Point2D(double x_, double y_) : x(x_), y(y_) {}

    double distance_from_origin() const {
        return std::sqrt(x * x + y * y);
    }

    // Simplified methods that don't take Point2D parameters (codegen limitation for now)
    double length_squared() const {
        return x * x + y * y;
    }

    void scale(double factor) {
        x *= factor;
        y *= factor;
    }

    double dot_xy(double ox, double oy) const {
        return x * ox + y * oy;
    }
};
