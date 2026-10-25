#pragma once

#include <array>
#include <map>
#include <optional>
#include <compare>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
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

    // std::optional: null in C# is nullopt in C++, both ways.
    std::optional<int> limit;
    std::optional<double> trim{2.5};
    std::optional<std::string> note{"hi"};
    std::optional<Mode> fallback;

    // An optional in the other two positions. Claiming the matrix row on the
    // strength of members alone was an overclaim, and the generator segfaulted
    // on a return because c_type has no spelling for an optional.
    std::optional<int> maybe(bool want) const {
        return want ? std::optional<int>{id * 2} : std::nullopt;
    }
    std::optional<std::string> maybe_name(bool want) const {
        return want ? std::optional<std::string>{label} : std::nullopt;
    }
    int absorb(std::optional<int> v) const { return v.has_value() ? *v : -1; }
    std::string absorb_name(std::optional<std::string> v) const {
        return v.has_value() ? *v : std::string("none");
    }

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
    std::array<std::string, 2> labels{"lo", "hi"};
    // Readable, never a writable window: see Frozen.
    const std::array<int, 2> pinned{1, 2};
    int tag = 4;

    // Recomputed in C++, so it proves a write through the fixed view landed in
    // the owner's own storage.
    double total() const { return fixed[0] + fixed[1] + fixed[2]; }
    std::string joined() const { return labels[0] + "/" + labels[1]; }

    // Returned by value: the array has no identity apart from its storage, so
    // a view of this would be a view of a temporary. The honest surface is a
    // copied double[], filled through a buffer the caller owns.
    std::array<double, 3> spread() const { return fixed; }

    // An array OF STRINGS returned by value is still refused: it would need an
    // array of char* out-slots and a free per element.
    std::array<std::string, 2> spread_labels() const { return labels; }

    // Inbound needs the vector parameter's prologue plus an arity check and
    // does not have it yet, so this is reported too.
    double sum_of_fixed(std::array<double, 3> a) const {
        return a[0] + a[1] + a[2];
    }
};

// Every one of these is a defect found by an adversarial read of the backend
// and reproduced before it was fixed. They are ordinary C++ -- that is the
// point: each one used to take the whole module down, or worse, bind silently
// to the wrong thing, on a header nobody would call unusual.

// `in` is a C# keyword and a perfectly ordinary C++ parameter name. One
// escaped spelling used to be written into both surfaces, so the shim read
// `const char* @in` and the module did not parse.
struct Echoer {
    std::string prefix = "p:";
    std::string echo(std::string in) const { return prefix + in; }
};

// `r` and `p` are what the generated C# calls its own locals, and C# puts
// parameters and locals in one scope, so these were CS0128 twice over.
struct Locals {
    int v = 0;
    explicit Locals(int p) : v(p) {}
    int add(int r) const { return v + r; }
};

// Two C++ overloads that collapse to one C# signature, so one is refused. The
// shim used to forward the raw wire variable and let C++ re-resolve, which on
// LP64 picked the exact match -- whichever overload was REFUSED. Both
// declaration orders are here because a single order passes by luck: with the
// narrow overload first the accidental resolution happens to agree with the
// choice, so only the other order shows the bug.
struct WideFirst {
    int pick(long long) const { return 2; }
    int pick(long) const { return 1; }
    static int spick(long long) { return 2; }
    static int spick(long) { return 1; }
};

struct NarrowFirst {
    int pick(long) const { return 1; }
    int pick(long long) const { return 2; }
    static int spick(long) { return 1; }
    static int spick(long long) { return 2; }
};

// A constructor taking a container, beside one taking bool. The constructor
// emitter had no vector arm, so the vector overload was declared as a bare
// void* and `new Pair(xs)` resolved through the pointer-to-bool conversion:
// it compiled, ran the bool constructor, and reported nothing. Both now bind,
// because all three emitters build their parameter lists from one arg_slot.
struct Pair {
    int which = 0;
    int n = 0;
    Pair(std::vector<double> xs) : which(1), n(static_cast<int>(xs.size())) {}
    Pair(bool b) : which(2), n(b ? 1 : 0) {}
};

// Declared, never defined. std::is_class_v is TRUE for a forward-declared
// class and core::Bindable only adds a reflection query an incomplete type can
// still satisfy, so this used to reach typeid -- which does not compile for an
// incomplete type, taking the whole module down with an error pointing into
// mirror_bridge's own source rather than naming a method.
//
// A MEMBER of an incomplete type is ill-formed C++, so it cannot be written
// here; a function that returns or takes one is legal to DECLARE, which is
// exactly how yaml-cpp's YAML::detail::iterator_value gets in. Nothing calls
// these, so they need no definition.
struct Opaque;

struct Forward {
    int id = 5;
    int doubled() const { return id * 2; }

    Opaque make() const;
    int take(Opaque o) const;
};

// A std::vector and a std::optional parameter on a STATIC method. Neither was
// expressible before the three emitters shared one slot description: the static
// emitter had no second wire slot for a length or a has-flag, so a vector
// parameter arrived as a bare void* and an optional reached the
// MB_WIRE_UNHANDLED poison. Nothing was written for the static case to make
// these work -- they fall out of the same arg_slot a method uses.
struct Shared {
    int seen = 0;

    static int total(std::vector<int> xs) {
        int t = 0;
        for (auto x : xs) t += x;
        return t;
    }

    static int fallback(std::optional<int> v) { return v.value_or(-1); }

    static std::string shout(std::string s) { return s + "!"; }

    // Returns, which needed RetSlot: the has-flag and the heap copy are
    // trailing out-slots and a shim body, and both lived inside emit_method.
    static std::optional<int> maybe(bool want) {
        return want ? std::optional<int>(9) : std::nullopt;
    }

    static std::vector<int> spread(int n) {
        std::vector<int> out;
        for (int i = 0; i < n; ++i) out.push_back(i * 2);
        return out;
    }
};

// A string-keyed map is the most common non-vector container in a real public
// API. Its snapshot is two owned vectors rather than a third indexable type,
// so the key and value halves reuse the view classes their element types
// already have -- a map<string,int> member in a module that also has a
// vector<int> member shares that vector's helper set.
struct Registry {
    std::map<std::string, int> counts{{"a", 1}, {"b", 2}, {"c", 3}};
    std::map<std::string, std::string> labels{{"mode", "fast"}, {"tier", "gold"}};
    std::unordered_map<int, double> weights{{1, 0.5}, {2, 1.5}};

    std::map<std::string, int> doubled() const {
        std::map<std::string, int> out;
        for (const auto& kv : counts) out[kv.first] = kv.second * 2;
        return out;
    }

    // Recomputed in C++ every call, so it is the proof that a write made
    // through the C# view reached the real map rather than a copy of it.
    int total() const {
        int t = 0;
        for (const auto& kv : counts) t += kv.second;
        return t;
    }

    // Inbound is a smaller set than outbound: a map parameter needs the
    // materialise-or-pass-through prologue a vector parameter has, and does
    // not have it yet, so this must be reported rather than emitted wrong.
    int sum_of(const std::map<std::string, int>& m) const {
        int t = 0;
        for (const auto& kv : m) t += kv.second;
        return t;
    }
};

// key_type and value_type but no mapped_type, which is exactly how a set is
// told apart from a map. It has to be reported as not crossing, not quietly
// bound as one.
struct TagSet {
    std::set<std::string> names{"x", "y"};
    int count = 2;
};

// A const member of container or class type cannot cross the way a non-const
// one does -- as a pointer into the owner that writes reach -- because taking
// its address gives a const T* and static_cast cannot drop that. Before this
// was guarded the run reported "0 member(s) left out" and then failed inside
// the C++ compiler, which the consumer cannot act on without editing the const
// off their own header. Clipper2 and nlohmann-json both ship members of this
// shape, which is why tests/const_members exists for the other backends.
struct Frozen {
    const std::vector<double> baseline{1.0, 2.0, 3.0};
    const Counter origin{};
    double drift = 0.25;

    double baseline_sum() const {
        return baseline[0] + baseline[1] + baseline[2];
    }
};

// long and long long are distinct C++ types that the C# side spells
// identically, and the plan is keyed on the C++ type, so these two used to
// register two plans that minted one set of mb_vec_long_* helpers and one
// MbLongVector class: a duplicate native symbol and CS0101 in the same build.
struct Widths {
    std::vector<long> narrow{1, 2};
    std::vector<long long> wide{3, 4, 5};

    long narrow_sum() const {
        long t = 0;
        for (auto v : narrow) t += v;
        return t;
    }
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
