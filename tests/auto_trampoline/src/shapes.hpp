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

// Returns by value, so the binding owns the result. An auto-trampoline class
// whose payload fits inside the Python object is constructed in place, and
// freeing that as a heap pointer aborts the process. This is the only shape
// in the suite that reaches that dealloc with an owned object.
inline Animal CloneAnimal(const Animal& a) { return Animal(a); }

// Every other class here is built in tp_new, because none of them declares
// a constructor that takes anything. A class that does is built in tp_init
// instead, and that is a different slot with a different capture rule: the
// __init__ descriptor PyType_Ready writes into tp_dict holds the function
// pointer it saw, so a slot replaced afterwards is not the one
// super().__init__() calls. Binding one here is what keeps the two paths
// honest; without it, bind_class_auto raised TypeError on every single
// construction of such a class and the suite stayed green.
class Robot {
public:
    Robot() = default;
    explicit Robot(std::string id) : id_(std::move(id)) {}
    virtual ~Robot() = default;

    virtual std::string Sound() const { return "beep"; }
    virtual std::string Name() const { return id_.empty() ? "unnamed" : id_; }

    const std::string& id() const { return id_; }

private:
    std::string id_;
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
    std::string describe_robot(const Robot& r) const {
        return r.Sound() + " from " + r.Name();
    }
};

}  // namespace shapes
