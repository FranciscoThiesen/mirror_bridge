#pragma once

#include <string>
#include <cmath>

// Basic class: numeric members + a zero-parameter method
struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    double length() const {
        return std::sqrt(x * x + y * y);
    }
};

// Mixed types: int, double, bool, string
struct Config {
    int width = 800;
    int height = 600;
    double scale = 1.0;
    bool fullscreen = false;
    std::string title = "default";
};

// Edge case: empty class
struct Empty {};

// Edge case: methods only, no data members
struct MethodsOnly {
    double compute() const { return 42.0; }
    void reset() {}
};

// Edge case: "type" is a valid C++ identifier but a Rust keyword.
// The generated Rust code must use raw identifier syntax (r#type).
struct HasKeywordMember {
    int type = 0;
    double value = 1.0;
};
