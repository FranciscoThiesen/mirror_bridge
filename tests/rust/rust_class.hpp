#pragma once

#include <string>

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

struct Config {
    int width = 800;
    int height = 600;
    double scale = 1.0;
    std::string title = "default";
};
