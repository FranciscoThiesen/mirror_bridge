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

// A member whose type has no default constructor is the same situation
// arrived at differently: readable, not assignable. The setter converts into
// a local and moves it into place, and that local needs a default
// constructor, so without this the member's type takes the whole module down
// rather than the one attribute.
//
// nlohmann::detail::dtoa_impl::diyfp is the real one -- it declares only
// diyfp(uint64_t, int), and a class holding one made the whole of
// nlohmann-json a compile failure.
struct Fixed {
    int value;
    explicit Fixed(int v) : value(v) {}
};

struct Holder {
    Fixed anchor{7};
    double adjustable = 1.5;

    int anchor_value() const { return anchor.value; }
    void reseat(int v) { anchor = Fixed(v); }
};
