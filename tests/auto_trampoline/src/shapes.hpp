// Auto-trampoline test: Python subclasses override C++ virtuals with ZERO
// hand-written trampoline glue. Reflection enumerates T's virtuals and a
// custom vtable is built and swapped into instances at allocation.
#pragma once
#include <string>

namespace shapes {

// Concrete base — defaults provided for every virtual. Auto-trampoline
// assumes a concrete base so it can allocate T directly and fall through
// to the original implementation when Python doesn't override.
class Animal {
public:
    Animal() = default;
    virtual ~Animal() = default;

    virtual std::string Sound() const { return "generic"; }
    virtual std::string Name() const { return "Animal"; }
    virtual int Weight() const { return 10; }

    int LegCount() const { return 4; }  // non-virtual, exposed via reflection
};

// Exercises Animal through virtual dispatch. This is the path the auto
// trampoline intercepts — without vtable swap, C++ code here would call
// the base defaults unconditionally.
class Zoo {
public:
    std::string describe(const Animal& a) const {
        return a.Sound() + " from " + a.Name();
    }
    int total_weight(const Animal& a, const Animal& b) const {
        return a.Weight() + b.Weight();
    }
};

}  // namespace shapes
