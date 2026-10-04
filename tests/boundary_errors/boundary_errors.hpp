#pragma once

// C++ exceptions crossing back into Python, and constructor argument binding.
//
// Both halves used to fail silently or fatally. An exception thrown from a
// free function or a constructor unwound past the interpreter and aborted the
// process; a keyword constructor call ignored the keywords and handed back a
// default-constructed object.

#include <stdexcept>
#include <string>

namespace boundary {

struct Thrower {
    Thrower() = default;

    // A constructor that validates its arguments is ordinary C++. It used to
    // abort the interpreter.
    explicit Thrower(int x) {
        if (x < 0) throw std::invalid_argument("ctor rejects negative");
        value = x;
    }

    int value = 0;

    int method(int x) const {
        if (x < 0) throw std::runtime_error("method rejects negative");
        return x;
    }

    static int static_method(int x) {
        if (x < 0) throw std::out_of_range("static rejects negative");
        return x;
    }

    // Something that is not a std::exception at all.
    int exotic(int x) const {
        if (x < 0) throw std::string("not a std::exception");
        return x;
    }
};

// Distinct parameter names so a keyword call can be told from a positional one
// by its result alone.
struct Point {
    double x = -1.0, y = -1.0, z = -1.0;
    Point() = default;
    Point(double x_in, double y_in) : x(x_in), y(y_in), z(0.0) {}
    Point(double x_in, double y_in, double z_in) : x(x_in), y(y_in), z(z_in) {}
};

struct DefaultOnly {
    int tag = 42;
};

// A call that does not match should say why. With one candidate the
// parameter names and the arity are compile-time facts, so the message can
// name the offending keyword instead of reporting only that nothing matched.
struct Signatures {
    double scale(double factor, int times) const { return factor * times; }
    double with_default(double cash, int periods, double spread = 0.0) const {
        return cash / (1 + periods + spread);
    }
    int nullary() const { return 7; }

    // Overloaded: here the generic message is the honest one, because there
    // really are several candidates.
    int pick(int a) const { return a; }
    int pick(double a) const { return static_cast<int>(a); }
};

inline int free_thrower(int x) {
    if (x < 0) throw std::runtime_error("free function rejects negative");
    return x * 2;
}

inline int free_ok(int x) { return x + 1; }

}  // namespace boundary
