// One class per outcome a constructor call can have. Each comment says what
// the call used to do in Lua and JavaScript before constructor dispatch
// refused the arguments it could not use.
#pragma once

#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// A default constructor and a two-argument one. Arguments no constructor
// accepted used to be dropped in favour of Rect(): Rect(2) and Rect({}, {})
// were both a 0 x 0 rectangle, with no error.
struct Rect {
    double w = 0, h = 0;
    Rect() = default;
    Rect(double width, double height) : w(width), h(height) {}
    double area() const { return w * h; }
};

// No default constructor, so a call needs its arguments.
struct Cell {
    int row = 0, col = 0;
    Cell(int r, int c) : row(r), col(c) {}
};

// An aggregate declares no constructor, so nothing can take arguments.
// Point2(3, 4) used to be (0, 0).
struct Point2 {
    double x = 0, y = 0;
};

// A struct as the first parameter. In Lua the argument is a table, which
// must not be mistaken for the class table the colon form passes.
struct Segment {
    Point2 a, b;
    Segment(Point2 from, Point2 to) : a(from), b(to) {}
    double length() const { return std::hypot(b.x - a.x, b.y - a.y); }
};

// Two constructors of one arity: tried in declaration order, and the first
// whose arguments convert wins.
struct Tagged {
    double value = 0;
    std::string tag;
    Tagged(double v, double scale) : value(v * scale), tag("numeric") {}
    Tagged(std::string t, std::string suffix) : tag(t + suffix) {}
};

// A deleted overload is still a declared member. It used to be offered, and
// the new Celsius(int) it instantiated failed the whole module to compile.
struct Celsius {
    double degrees = 0;
    explicit Celsius(double d) : degrees(d) {}
    Celsius(int) = delete;
};

// An abstract class that declares a constructor, the shape of pmp-library's
// BasePropertyArray. new Gauge(int) used to be instantiated and failed the
// whole module ("allocating an object of abstract class type").
struct Gauge {
    int tag = 0;
    explicit Gauge(int t) : tag(t) {}
    virtual ~Gauge() = default;
    virtual int measure() const = 0;
    int doubled_tag() const { return tag * 2; }
};

struct FixedGauge : Gauge {
    explicit FixedGauge(int t) : Gauge(t) {}
    int measure() const override { return tag + 1; }
};

// A constructor that needs an lvalue cannot be called with a converted
// value. It used to fail the whole module; now it is not offered, and the
// class binds through its other constructors.
struct Config {
    int level = 3;
};

struct Widget {
    int level = 0;
    Widget() = default;
    explicit Widget(Config& c) : level(c.level) { c.level = 0; }
    explicit Widget(int l) : level(l) {}
};

// A constructor taking a class with no default constructor, even by const
// reference, cannot be offered: the converted arguments are held in a
// default-constructed tuple. It used to fail the whole module; now Line
// binds through its default constructor, and the two-argument call is
// refused by name.
struct Line {
    int length = 0;
    Line() = default;
    Line(const Cell& from, const Cell& to) : length(to.col - from.col) {}
};

// Nothing a host can call at all.
struct View {
    int level = 0;
    explicit View(Config& c) : level(c.level) {}
};

// More parameters than the sixteen argument slots JavaScript used to read:
// calling this read past the end of the buffer.
struct Big {
    int sum = 0;
    Big(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8,
        int a9, int a10, int a11, int a12, int a13, int a14, int a15, int a16)
        : sum(a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11 + a12 +
              a13 + a14 + a15 + a16) {}
};

// Two constructors of one arity, where trying the first converts a number to
// a string. A failed attempt must leave the arguments as they were, or the
// second, which takes the numbers, never sees them.
struct Mixed {
    std::string tag;
    int a = 0, b = 0;
    Mixed(std::string t, std::vector<int> v) : tag(t), a(v.empty() ? 0 : v[0]) {}
    Mixed(int x, int y) : tag("ints"), a(x), b(y) {}
};

// A raw pointer parameter has no converter. The constructor taking one used to
// fail the whole module; now it is not offered.
struct Linked {
    int value = 0;
    Linked() = default;
    Linked(int v, Linked* next) : value(next ? -1 : v) {}
};

// A borrowed-string overload tried first: in Lua it read the number argument
// through lua_tolstring, which rewrote the caller's 1/3 into the string
// "0.33333333333333", and the numeric overload then received that.
struct Meas {
    double value = 0;
    Meas(std::string_view label, std::vector<int> v) : value(label.size() + v.size()) {}
    Meas(double a, double b) : value(a * b) {}
};

// A constructor that throws: the exception becomes a host error.
struct Checked {
    int value = 0;
    explicit Checked(int v) : value(v) {
        if (v < 0) throw std::invalid_argument("Checked needs a non-negative value");
    }
};
