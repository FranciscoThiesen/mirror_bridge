#pragma once

#include <string>
#include <expected>
#include <cmath>
#include <vector>

// Test class demonstrating std::expected integration with mirror_bridge.
// Shows how C++ functions can use std::expected for error handling and have
// those errors automatically propagated as exceptions in target languages.

struct MathService {
    double precision = 1e-10;

    // Division with error handling — returns error string on divide-by-zero
    std::expected<double, std::string> safe_divide(double a, double b) const {
        if (std::abs(b) < precision) {
            return std::unexpected("division by zero");
        }
        return a / b;
    }

    // Square root with validation — negative inputs produce an error
    std::expected<double, std::string> safe_sqrt(double x) const {
        if (x < 0.0) {
            return std::unexpected("cannot take square root of negative number");
        }
        return std::sqrt(x);
    }

    // Demonstrates numeric error codes instead of string messages
    std::expected<int, int> parse_positive(int value) const {
        if (value < 0) {
            return std::unexpected(-1);
        }
        if (value == 0) {
            return std::unexpected(0);
        }
        return value;
    }

    // A method that always succeeds — tests the happy path
    std::expected<std::string, std::string> greet(const std::string& name) const {
        if (name.empty()) {
            return std::unexpected("name cannot be empty");
        }
        return "Hello, " + name + "!";
    }

    // Chained operations: both can fail
    std::expected<double, std::string> safe_divide_then_sqrt(double a, double b) const {
        auto div_result = safe_divide(a, b);
        if (!div_result) {
            return std::unexpected(div_result.error());
        }
        return safe_sqrt(*div_result);
    }
};
