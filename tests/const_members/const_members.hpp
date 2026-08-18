#pragma once

#include <cstdint>
#include <string>

// Classes with const data members appear throughout real-world libraries
// (Clipper2's random engine is where we first hit one). They must bind as
// readable, non-assignable attributes — not fail to compile.
struct Sensor {
    const uint64_t id = 42;
    const std::string model = "MB-1000";
    double reading = 0.0;

    double scaled(double factor) const { return reading * factor; }
};
