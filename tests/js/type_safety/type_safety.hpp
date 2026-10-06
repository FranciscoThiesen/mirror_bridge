#pragma once
#include <string>

// Fixtures for the type-identity check at the JavaScript boundary.
//
// Every bound class shares one wrapper layout, so before the check
// napi_unwrap answered "yes, this object was wrapped" without saying wrapped
// as what, and a wrapper for any class could be read as a wrapper for any
// other. The shapes below cover the ways that went wrong: unrelated classes
// with incompatible storage, a base that is not the first one, and the
// derived-to-base conversions that must keep working.

namespace ts {

// Two unrelated classes. Reading a Label as a Curve produced a plausible
// double out of the string's bytes rather than an error.
struct Curve {
    double rate = 0.05;
    double at(double t) const { return rate * t; }
};

struct Label {
    std::string tag = "a string long enough to not fit in any small-string buffer";
    int size() const { return static_cast<int>(tag.size()); }
};

// Single inheritance: Base sits at offset 0, so passing a Derived where a
// Base is expected worked before the check and must keep working.
struct Base {
    int tag = 7;
    int get_tag() const { return tag; }
};

struct Derived : Base {
    int extra = 99;
};

// Multiple inheritance. Second is NOT at offset 0, so reusing the object's
// address for it reads First's bytes. The sentinels make that visible.
struct First {
    int first_marker = 11111;
};

struct Second {
    double second_marker = 22222.0;
};

struct Both : First, Second {
    int own = 33333;
};

// One class with a parameter of each shape, so the test can reach the
// conversion from a method call and from a field assignment.
struct Portfolio {
    double booked = 0.0;
    Curve held;

    double book(const Curve& c) { booked += c.rate; return booked; }
    int label_size(const Label& l) const { return l.size(); }
    int take_base(const Base& b) const { return b.get_tag(); }
    double take_second(const Second& s) const { return s.second_marker; }
};

}  // namespace ts
