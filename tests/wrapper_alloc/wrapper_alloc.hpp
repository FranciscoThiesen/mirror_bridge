#pragma once
#include <array>
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

// Over-aligned: the object allocator does not promise this alignment, so it
// has to keep the heap path.
struct alignas(64) Wide {
    double v[8]{};
    double first() const { return v[0]; }
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
