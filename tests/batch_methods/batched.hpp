#pragma once
#include <cmath>
#include <string>
#include <vector>

// Every shape that decides whether a batch sibling is generated, so the
// eligibility rule is pinned by behaviour rather than by reading it.
struct Batched {
    double gain = 2.0;
    long calls = 0;

    // Should get a sibling.
    double scale(double x) const { return x * gain; }
    int quantize(double x) const { return static_cast<int>(x * 10); }
    double accumulate(double x) { calls += 1; return x + static_cast<double>(calls); }

    // Should not: no parameter, two parameters, non-arithmetic either side,
    // bool (vector<bool> is the proxy specialization), void return, and an
    // overloaded name where a batch would have to resolve per element.
    double reset() { calls = 0; return 0.0; }
    double mix(double a, double b) const { return a * b; }
    std::string label(double x) const { return std::to_string(x); }
    double parse(const std::string& s) const { return std::stod(s); }
    bool positive(double x) const { return x > 0; }
    bool flip(bool b) const { return !b; }
    void note(double x) { calls += static_cast<long>(x); }
    double over(double x) const { return x; }
    double over(int x) const { return x * 2.0; }

    // The author wrote this one, so nothing should be generated over it.
    double manual(double x) const { return x - 1.0; }
    std::vector<double> manual_batch(const std::vector<double>& xs) const {
        std::vector<double> out;
        out.reserve(xs.size());
        for (double x : xs) out.push_back(manual(x) * 1000.0);  // deliberately different
        return out;
    }
};
