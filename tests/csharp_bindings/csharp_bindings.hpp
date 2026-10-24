#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace csb {

// Each member and method here pins one branch of the C ABI policy: what
// crosses as a scalar, what crosses as UTF-8, what is read-only because C++
// said so, and what is reported as left out rather than silently dropped.
struct Device {
    int id = 7;
    double calibration = 1.25;
    bool online = true;
    std::string label = "probe";

    const int serial = 4242;

    // No scalar or string representation, so this must appear in the skipped
    // report rather than vanish from the generated C#.
    std::vector<double> samples;

    int doubled() const { return id * 2; }
    double scale(double factor) const { return calibration * factor; }
    bool toggle(bool on) { online = on; return online; }
    std::string describe() const { return label + "/" + std::to_string(id); }
    void rename(std::string next) { label = next; }
    int sum(int a, int b) const { return a + b; }

    void fail() const { throw std::runtime_error("device is on fire"); }

    // Returns a type with no wire representation.
    std::vector<double> history() const { return samples; }
};

struct Counter {
    int value = 0;
    int bump() { return ++value; }
};

}  // namespace csb
