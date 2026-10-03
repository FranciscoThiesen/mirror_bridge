#pragma once
#include <string>
#include <vector>

// Fixtures for the type-identity check at the Python boundary.
//
// Every bound class shares one wrapper layout, so before the check a wrapper
// for any class (or any unrelated PyObject) could be read as a wrapper for
// any other. The shapes below cover the ways that used to go wrong:
// unrelated classes with incompatible storage, a base that is not the first
// one, and the derived-to-base conversions that must keep working.

namespace ts {

// Two unrelated classes. Reading a Label as a Curve used to produce a
// plausible double out of the string's bytes rather than an error.
struct Curve {
    double rate = 0.05;
    double at(double t) const { return rate * t; }
};

struct Label {
    std::string tag = "a string long enough to not fit in any small-string buffer";
    std::size_t size() const { return tag.size(); }
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
    int marker() const { return first_marker; }
};

struct Second {
    double second_marker = 22222.0;
    double marker() const { return second_marker; }
};

struct Both : First, Second {
    int own = 33333;
};

// Virtual inheritance. The base subobject's offset is not a compile-time
// constant here, which is the case that most justifies going through a
// recorded thunk rather than reusing the address.
struct VBase { int shared = 777; int value() const { return shared; } };
struct VLeft : virtual VBase {};
struct VRight : virtual VBase {};
struct VDiamond : VLeft, VRight { int own = 5; };

// Abstract base with a concrete implementation: exercises pointer storage,
// where the wrapper's object is aliased instead of copied.
struct Shape {
    virtual ~Shape() = default;
    virtual double area() const = 0;
};

struct Square : Shape {
    double side = 3.0;
    double area() const override { return side * side; }
};

// Non-copyable: also forced onto the pointer-storage path.
struct Handle {
    int id = 1234;
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    int value() const { return id; }
};

// A free function per parameter shape, so the test can check each one.
// Abstract and non-copyable parameters are not among them on purpose: free
// functions convert arguments into value storage, so those two shapes are
// reachable only as method parameters (see Inspector).
inline double take_curve_cref(const Curve& c) { return c.at(2.0); }
inline double take_curve_value(Curve c) { return c.at(4.0); }
inline std::size_t take_label(const Label& l) { return l.size(); }
inline int take_base(const Base& b) { return b.get_tag(); }
inline int take_first(const First& f) { return f.marker(); }
inline double take_second(const Second& s) { return s.marker(); }
inline int take_vbase(const VBase& v) { return v.value(); }
// The downcast direction: a Base is NOT a Derived and must be refused.
inline int take_derived(const Derived& d) { return d.extra; }

// Methods give the pointer-storage path, where the wrapper's object is
// aliased rather than copied: a second, separate unchecked cast.
struct Inspector {
    int calls = 0;
    double area_of(const Shape& s) { ++calls; return s.area(); }
    int id_of(const Handle& h) { ++calls; return h.value(); }
    // Pointer storage reaching a base that is not at offset 0.
    double second_of(const Second& s) { ++calls; return s.marker(); }
};

// Container elements go through the same conversion.
inline double sum_curves(const std::vector<Curve>& cs) {
    double total = 0.0;
    for (const auto& c : cs) total += c.rate;
    return total;
}

// A value type with a conversion operator. This is the shape that used to
// make the whole module fail to compile: a conversion function is not an
// operator function and has no identifier, so it passed the method filter
// and then reached identifier_of. Binding this class at all is the test.
struct Ticks {
    long count = 250;
    explicit operator double() const { return count * 0.01; }
    long raw() const { return count; }
};

// A class-typed parameter on a method and on a constructor.
struct Portfolio {
    double booked = 0.0;
    Portfolio() = default;
    explicit Portfolio(const Curve& c) : booked(c.rate) {}
    double book(const Curve& c) { booked += c.rate; return booked; }
    // Overload set: picking the wrong one is how a Label became a Curve.
    double value(const Curve& c) const { return c.rate; }
    double value(const Label& l) const { return static_cast<double>(l.size()); }
    Curve held;
};

}  // namespace ts
