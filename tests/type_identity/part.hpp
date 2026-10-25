#pragma once

namespace om {

struct Part {
    double x = 1.5;
    double scaled(double by) const { return x * by; }
};

// Returning a Part by value is what goes through to_python, which is the
// path that used to disagree with tp_new about which type object a Part has.
struct Maker {
    double seed = 2.5;
    Part make() const { return Part{seed}; }
};

}  // namespace om
