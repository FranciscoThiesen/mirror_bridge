#pragma once

#include <string>
#include <cmath>

struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    double length() const {
        return std::sqrt(x * x + y * y);
    }
};

struct Config {
    int width = 800;
    int height = 600;
    double scale = 1.0;
    bool fullscreen = false;
    std::string title = "default";
};
