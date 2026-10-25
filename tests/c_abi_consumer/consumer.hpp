#pragma once

#include <stdexcept>
#include <string>
#include <vector>

// Deliberately modern C++: the generator reads this with reflection, and the
// consumer below never sees it. Only the generated C header.
namespace cab {

struct Reading {
    int id = 7;
    double value = 1.5;
    std::string tag = "probe";
    std::vector<double> history;

    double scaled(double factor) const { return value * factor; }
    double sum_of(const std::vector<double>& xs) const {
        double t = 0.0;
        for (double x : xs) t += x;
        return t;
    }
    void boom() const { throw std::out_of_range("reading out of range"); }
};

}  // namespace cab
