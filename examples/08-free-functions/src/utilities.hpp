#pragma once
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

// Example: Free Functions (Standalone Functions)
// Demonstrates binding functions outside of classes

namespace utils {

// Simple arithmetic
int add(int a, int b) {
    return a + b;
}

double multiply(double a, double b) {
    return a * b;
}

double divide(double a, double b) {
    if (b == 0.0) {
        throw std::runtime_error("Division by zero");
    }
    return a / b;
}

// Math utilities
double clamp(double value, double min_val, double max_val) {
    return std::max(min_val, std::min(value, max_val));
}

double lerp(double a, double b, double t) {
    return a + t * (b - a);
}

double degrees_to_radians(double degrees) {
    return degrees * 3.14159265358979 / 180.0;
}

double radians_to_degrees(double radians) {
    return radians * 180.0 / 3.14159265358979;
}

// String utilities
std::string to_upper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string repeat(const std::string& s, int n) {
    std::string result;
    result.reserve(s.size() * n);
    for (int i = 0; i < n; ++i) {
        result += s;
    }
    return result;
}

bool starts_with(const std::string& s, const std::string& prefix) {
    if (prefix.size() > s.size()) return false;
    return s.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with(const std::string& s, const std::string& suffix) {
    if (suffix.size() > s.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Vector utilities
double sum(const std::vector<double>& values) {
    return std::accumulate(values.begin(), values.end(), 0.0);
}

double mean(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    return sum(values) / static_cast<double>(values.size());
}

double min_value(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    return *std::min_element(values.begin(), values.end());
}

double max_value(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    return *std::max_element(values.begin(), values.end());
}

std::vector<int> range(int start, int end) {
    std::vector<int> result;
    for (int i = start; i < end; ++i) {
        result.push_back(i);
    }
    return result;
}

std::vector<double> linspace(double start, double end, int num) {
    std::vector<double> result;
    if (num <= 0) return result;
    if (num == 1) {
        result.push_back(start);
        return result;
    }
    double step = (end - start) / (num - 1);
    for (int i = 0; i < num; ++i) {
        result.push_back(start + i * step);
    }
    return result;
}

// Void functions (side effects only, in real code these might log, etc.)
void validate_positive(double value) {
    if (value <= 0) {
        throw std::runtime_error("Value must be positive");
    }
}

// Function returning nothing meaningful but still useful
int count_words(const std::string& text) {
    if (text.empty()) return 0;
    int count = 0;
    bool in_word = false;
    for (char c : text) {
        if (std::isspace(c)) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            count++;
        }
    }
    return count;
}

} // namespace utils
