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
    // The second parameter's real name is a legal Python identifier, so it
    // must survive: the substitute for the first must not claim it.
    double collide(double lambda, double lambda_) const { return lambda + lambda_; }

    // Unnamed in the declaration, so there is no keyword for either.
    double unnamed(double, double) const { return 0.0; }
};

// A static method and a free function take no keyword arguments at all, so
// every parameter of theirs is reachable by position only.
double ratio(double numerator, double denominator) {
    return denominator == 0.0 ? 0.0 : numerator / denominator;
}

}  // namespace kwtest
