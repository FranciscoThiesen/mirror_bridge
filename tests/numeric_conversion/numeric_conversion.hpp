#pragma once

// Integer and floating-point arguments at the Python boundary.
//
// Two things are pinned here. The first is range: a Python int is arbitrary
// precision and a C++ int is not, so every width needs a bound and a value
// just past it. The second is that a refused conversion leaves no Python
// error set — an overflow raised and never cleared surfaces at whatever call
// runs next and looks like it came from there, which is far harder to
// diagnose than the TypeError it should have been.

#include <cstdint>
#include <vector>

namespace numconv {

struct Numbers {
    int i = 0;
    unsigned u = 0;
    short sh = 0;
    unsigned char uc = 0;
    std::int64_t big = 0;
    std::uint64_t ubig = 0;
    double d = 0.0;
    float f = 0.0f;
    bool flag = false;

    int take_int(int v) const { return v; }
    unsigned take_unsigned(unsigned v) const { return v; }
    short take_short(short v) const { return v; }
    unsigned char take_uchar(unsigned char v) const { return v; }
    std::int64_t take_int64(std::int64_t v) const { return v; }
    std::uint64_t take_uint64(std::uint64_t v) const { return v; }
    double take_double(double v) const { return v; }
    float take_float(float v) const { return v; }
    bool take_bool(bool v) const { return v; }

    // Containers go down a separate bulk path, so they need the same
    // boundaries checked independently of the scalar arguments above.
    std::int64_t sum_ints(const std::vector<int>& xs) const {
        std::int64_t total = 0;
        for (int x : xs) total += x;
        return total;
    }
    std::int64_t sum_int64(const std::vector<std::int64_t>& xs) const {
        std::int64_t total = 0;
        for (std::int64_t x : xs) total += x;
        return total;
    }
    double sum_doubles(const std::vector<double>& xs) const {
        double total = 0.0;
        for (double x : xs) total += x;
        return total;
    }
    std::size_t count(const std::vector<unsigned>& xs) const { return xs.size(); }
};

}  // namespace numconv
