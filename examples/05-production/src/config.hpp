#pragma once
#include <string>

// Application configuration
struct Config {
    std::string app_name = "PhysicsSimulator";
    std::string version = "1.0.0";
    int max_particles = 1000;
    double time_step = 0.016;  // ~60 FPS
    bool debug_mode = false;

    void enable_debug() {
        debug_mode = true;
    }

    void disable_debug() {
        debug_mode = false;
    }

    std::string info() const {
        return app_name + " v" + version;
    }
};
