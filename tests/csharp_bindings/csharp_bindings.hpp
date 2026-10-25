#pragma once

#include <array>
#include <compare>
#include <stdexcept>
#include <string>
#include <vector>

namespace csb {

// Crosses as its underlying integer and is declared as a real C# enum, so the
// generated API stays typed.
//
// `None` is here because it is a keyword in Python and not in C#. The Python
// backend has to rename it, since `Permission.None` is a syntax error there
// with no spelling a caller could find; C# needs no escaping at all and keeps
// the name the C++ gave it. The two backends diverge on exactly this, which is
// why the enumerator mangling is not shared code.
enum class Mode : int {
    None = 0,
    Fast = 1,
    Careful = 7,
};


// Each member and method here pins one branch of the C ABI policy: what
// crosses as a scalar, what crosses as UTF-8, what is read-only because C++
// said so, and what is reported as left out rather than silently dropped.
struct Device {
    int id = 7;
    double calibration = 1.25;
    bool online = true;
    std::string label = "probe";

    const int serial = 4242;

    // A live view of the C++ vector: Add and the indexer reach this object.
    std::vector<double> samples;

    // Same, over strings, so the per-element UTF-8 path is exercised too.
    std::vector<std::string> names;

    // vector<bool> packs bits and hands back a proxy, so there is no element to
    // address. It must be reported as not crossing rather than bound wrongly.
    std::vector<bool> flags{true, false};

    int doubled() const { return id * 2; }
    double scale(double factor) const { return calibration * factor; }
    bool toggle(bool on) { online = on; return online; }
    std::string describe() const { return label + "/" + std::to_string(id); }
    void rename(std::string next) { label = next; }
    int sum(int a, int b) const { return a + b; }

    // A container crossing INWARD, which is the shape a real caller writes
    // and the one this backend refused longest.
    double sum_of(const std::vector<double>& values) const {
        double t = 0.0;
        for (double v : values) t += v;
        return t;
    }

    void replace_samples(const std::vector<double>& values) { samples = values; }

    int count_names(const std::vector<std::string>& who) const {
        return static_cast<int>(who.size());
    }

    Mode mode = Mode::Fast;

    Mode promote(Mode m) const { return m == Mode::None ? Mode::Fast : Mode::Careful; }

    // C# overloads natively, so these need only distinct exported symbols.
    int scaled(int factor) const { return id * factor; }
    double scaled(double factor) const { return calibration * factor; }

    // Distinct in C++ and identical once both are `long` in C#, so the second
    // must be refused by name rather than emitted twice.
    long widen(long v) const { return v + 1; }
    long long widen(long long v) const { return v + 2; }

    static int version() { return 3; }
    static std::string describe_mode(Mode m) {
        return m == Mode::Careful ? "careful" : "other";
    }

    void fail() const { throw std::runtime_error("device is on fire"); }

    // Each of these reaches C# as a different exception type, so a caller can
    // catch the one it can act on instead of one InvalidOperationException for
    // everything the C++ might throw.
    void reject() const { throw std::invalid_argument("slot is not a number"); }
    void overrun() const { throw std::out_of_range("slot 99 of 7"); }
    void too_big() const { throw std::overflow_error("reading overflowed"); }

    // Returns a type with no wire representation.
    std::vector<double> history() const { return samples; }
};

struct Counter {
    int value = 0;
    int bump() { return ++value; }

    Counter() = default;
    explicit Counter(int start) : value(start) {}
    Counter(int start, int bumps) : value(start) {
        for (int i = 0; i < bumps; ++i) ++value;
    }
};

// Inheritance. The method cache in core already walks bases; data members did
// not until this backend needed them, so both halves are pinned here.
struct Base {
    int base_id = 11;
    std::string tag = "base";
    int base_method() const { return base_id * 2; }
};

struct Derived : Base {
    double extra = 0.5;
    double combined() const { return base_id + extra; }
};

// PascalCase C++ that collides with the generated surface. This is not an
// exotic shape: `int Handle` and a method called `Dispose()` are house style at
// several large shops, and before the collision was handled this produced C#
// that did not compile.
struct Clashing {
    int Handle = 1;
    int Lib = 2;
    double Check = 3.5;
    int Dispose() const { return Handle + Lib; }
    int New() const { return 99; }
};

// Nested objects. A member is borrowed from its owner, so writing through the
// view must reach the real subobject; a value returned by a method is a copy
// the caller owns, because a pointer to the temporary would dangle.
struct Inner {
    double x = 1.0;
    int n = 2;
    double scaled(double k) const { return x * k; }
};

struct Outer {
    Inner inner;
    int tag = 5;

    Inner make() const {
        Inner i;
        i.x = 9.5;
        i.n = 42;
        return i;
    }

    double consume(Inner other) const { return other.x + inner.x; }
};

// A member `x` and a method `get_x` both want the symbol mb_Collide_get_x.
// The two name spaces were never disjoint, and a second definition is a link
// error rather than a warning.
struct Collide {
    int x = 1;
    int get_x() const { return x * 10; }
};

// std::array has value_type and .data() but no allocator_type. Naming
// allocator_type outside the detection guard made this a hard error in the
// generator rather than a member reported as not crossing.
struct WithArray {
    std::array<double, 3> fixed{1.0, 2.0, 3.0};
    int tag = 4;
    double total() const { return fixed[0] + fixed[1] + fixed[2]; }
};

// A C++20 feature in the bound header. The shim re-parses this header, so
// pinning that compile at C++17 rejected it.
struct Modern {
    int version = 20;
    auto operator<=>(const Modern&) const = default;
    int doubled() const { return version * 2; }
};

// Two bases contributing one member name. Unqualified access gave "member 'v'
// found in multiple base classes of different types" and took the whole module
// down; the access is qualified with the declaring class now.
struct LeftBase  { int v = 1; int from_left() const { return v * 10; } };
struct RightBase { int v = 2; };
struct Mixin : LeftBase, RightBase { double w = 0.5; };

// An inherited member named after the derived class. The injected-class-name
// wins in C++ ("cannot refer to type member 'Shadow' ... with '->'"), and C#
// separately forbids a member named after its enclosing type.
struct HasShadow { int Shadow = 7; };
struct Shadow : HasShadow { double z = 1.0; };

// Names the generator uses for itself at namespace scope. Before these were
// prefixed, either one was CS0101 "already contains a definition".
struct Native { int id = 3; };
struct DoubleVector { int n = 4; };

// Subtype polymorphism. A generated class is sealed and has no C# base type,
// so without an upcast a Derived cannot be passed where a Base is expected --
// which rules out binding a class hierarchy, and most real C++ libraries are
// one. Two bases, so the pointer adjustment is not trivially zero for both.
struct ShapeBase {
    int sides = 0;
    int describe() const { return sides * 100; }
};

struct Tagged {
    double weight = 2.5;
};

struct Square : Tagged, ShapeBase {
    double edge = 3.0;
    double area() const { return edge * edge; }
};

struct Judge {
    int count(const ShapeBase& s) const { return s.sides; }
    double weigh(const Tagged& t) const { return t.weight; }
};

}  // namespace csb
