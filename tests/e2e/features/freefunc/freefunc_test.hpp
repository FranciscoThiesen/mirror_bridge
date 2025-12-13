#pragma once
#include <string>
#include <cmath>
#include <vector>
#include <numeric>

namespace freefunc_test {

// Simple free functions with different signatures
inline int add(int a, int b) {
    return a + b;
}

inline double multiply(double a, double b) {
    return a * b;
}

inline std::string greet(const std::string& name) {
    return "Hello, " + name + "!";
}

// No-argument function
inline double get_pi() {
    return 3.14159265358979;
}

// Void function
inline int global_counter = 0;
inline void increment_counter() {
    global_counter++;
}

inline int get_counter() {
    return global_counter;
}

inline void reset_counter() {
    global_counter = 0;
}

// Function taking vector
inline double sum_vector(const std::vector<double>& values) {
    return std::accumulate(values.begin(), values.end(), 0.0);
}

// Math functions
inline double distance(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return std::sqrt(dx*dx + dy*dy);
}

// Function returning vector
inline std::vector<int> range(int n) {
    std::vector<int> result;
    result.reserve(n);
    for (int i = 0; i < n; ++i) {
        result.push_back(i);
    }
    return result;
}

// A class that the functions can work with
struct Point {
    double x = 0.0;
    double y = 0.0;

    double length() const {
        return std::sqrt(x*x + y*y);
    }
};

// Free function taking a bound class
inline double point_distance(const Point& p1, const Point& p2) {
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    return std::sqrt(dx*dx + dy*dy);
}

// Free function returning a bound class
inline Point make_point(double x, double y) {
    return Point{x, y};
}

} // namespace freefunc_test
