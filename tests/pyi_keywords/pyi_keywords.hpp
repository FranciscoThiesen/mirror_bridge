#pragma once

// Names that are ordinary C++ identifiers but reserved in Python. `lambda` is
// routine in options maths, `from` in date and transfer code, and a parameter
// called `self` collides with the receiver the stub has to declare.
namespace kwtest {

struct Greeks {
    double delta = 0.0;
    double lambda = 0.0;

    double scale(double lambda, double vega) const { return lambda * vega; }
    double pass(double from, double import_) const { return from + import_; }
    double shadow(double self, double other) const { return self + other; }
    double plain(double first, double second) const { return first - second; }
    static double fold(double lambda) { return lambda; }
};

}  // namespace kwtest
