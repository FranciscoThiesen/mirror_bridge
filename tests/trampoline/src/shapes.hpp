// Regression test for Python-subclass-overrides-C++-virtual (trampoline).
//
// The critical path: a C++ function receives a Python-subclassed object,
// calls a virtual method on it via normal C++ dispatch, and the call should
// resolve to the Python override defined in the subclass.
#pragma once

#include <string>
#include <vector>

namespace shapes {

class Animal {
public:
    Animal() = default;
    virtual ~Animal() = default;
    virtual std::string Sound() const = 0;       // pure
    virtual std::string Name() const { return "Animal"; }  // concrete default
    int LegCount() const { return legs_; }       // non-virtual
protected:
    int legs_ = 4;
};

// Exercises Animal through its virtual interface — this is the C++ code
// that triggers trampoline dispatch when animal was constructed from a
// Python subclass.
class Zoo {
public:
    std::string describe(const Animal& a) const {
        return "Zoo heard: " + a.Sound() + " from " + a.Name();
    }

};

}  // namespace shapes
