#pragma once
#include <array>
#include <stdexcept>
#include <string>

namespace alloc {

inline int& live_count()  { static int n = 0; return n; }
inline int& dtor_count()  { static int n = 0; return n; }

// Placement new has to be paired with an explicit destructor call and never
// with delete, so the counts are what actually pins the inline path.
struct Counted {
    double x = 0.0;
    Counted() { ++live_count(); }
    Counted(const Counted& o) : x(o.x) { ++live_count(); }
    Counted(double v) : x(v) { ++live_count(); }
    ~Counted() { --live_count(); ++dtor_count(); }
};

// Throws after a sub-object is live, which is what makes re-initialising in
// place dangerous: unwinding destroys the built members, so leaving the
// pointer in place would let tp_dealloc destroy them a second time.
struct Fragile {
    std::string buf;
    int tag = 0;
    Fragile() : buf(256, 'a') {}
    Fragile(int t) : buf(256, 'b'), tag(t) {
        if (t < 0) throw std::runtime_error("negative tag");
    }
    Fragile(const Fragile& o) : buf(o.buf), tag(o.tag) {}
    ~Fragile() { ++dtor_count(); }
    int get() const { return tag; }
};

// Exactly the alignment the object allocator does promise, so this one stays
// inline and would fault on a misaligned payload offset.
struct alignas(16) Snug {
    double a = 1.0;
    double b = 2.0;
    double sum() const { return a + b; }
};

// Over-aligned: the object allocator does not promise this alignment, so it
// has to keep the heap path.
struct alignas(64) Wide {
    double v[8]{};
    double first() const { return v[0]; }
};

// Over-aligned, so its C++ object is on the heap rather than in the payload,
// and it has a parameterised constructor, so __init__ really is py_init.
// Those two together are what made a member view dangle: freeing the owner's
// object to re-initialise it left the view reading freed memory.
struct alignas(64) WideNest {
    Counted held;
    double pad[8]{};
    WideNest() = default;
    WideNest(double v) { held.x = v; }
};

struct Holder {
    Counted c;
    Wide wide;
    std::string label = "holder";
    Counted make(double v) const { return Counted(v); }
    static int live() { return live_count(); }
    static int destroyed() { return dtor_count(); }
    static void reset() { dtor_count() = 0; }
};

}  // namespace alloc
