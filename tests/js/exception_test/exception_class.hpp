#pragma once

#include <string>
#include <stdexcept>
#include <cmath>

struct ThrowingService {
    double value = 0.0;

    double safe_divide(double a, double b) {
        if (b == 0.0) {
            throw std::runtime_error("division by zero");
        }
        return a / b;
    }

    double safe_sqrt(double x) {
        if (x < 0.0) {
            throw std::invalid_argument("cannot take square root of negative number");
        }
        return std::sqrt(x);
    }

    void set_positive(double x) {
        if (x <= 0.0) {
            throw std::domain_error("value must be positive");
        }
        value = x;
    }
};
