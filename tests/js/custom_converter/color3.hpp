// Custom type for testing type converter extension
// This type is NOT reflected - it will be converted via custom converters

#pragma once

#include <cstdint>

// A simple RGB color type that we'll convert to/from language-native types
// This simulates third-party types like Eigen::Vector3d
struct Color3 {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    Color3() = default;
    Color3(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}

    bool operator==(const Color3& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
};

// A struct that USES Color3 - this gets reflected and bound normally
struct Pixel {
    int x = 0;
    int y = 0;
    Color3 color;

    Pixel() = default;
    Pixel(int x_, int y_, Color3 c) : x(x_), y(y_), color(c) {}

    // Method that returns a Color3
    Color3 get_color() const { return color; }

    // Method that takes a Color3 parameter
    void set_color(Color3 c) { color = c; }

    // Method that modifies and returns Color3
    Color3 inverted_color() const {
        return Color3(255 - color.r, 255 - color.g, 255 - color.b);
    }
};
