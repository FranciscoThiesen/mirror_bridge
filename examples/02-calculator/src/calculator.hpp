#pragma once
#include <string>
#include <stdexcept>

// A calculator demonstrating methods with parameters
struct Calculator {
    double value = 0.0;

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

    double divide(double x) {
        if (x == 0) {
            throw std::runtime_error("Division by zero");
        }
        value /= x;
        return value;
    }

    // Method with multiple parameters
    double compute(double a, double b, double c) {
        return a + b * c;
    }

    // Const method (read-only)
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
