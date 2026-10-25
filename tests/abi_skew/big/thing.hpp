#pragma once
namespace xm {

// The other team's revision of the same class, four members wider. Same
// namespace, same name, so typeid(xm::Thing).name() is identical in both
// modules and the shared type registry cannot tell them apart by name.
struct Thing {
    double a = 1.0;
    double pad[16]{};
};

// Returns a Thing by value, which is what sends the result through
// to_python and therefore through the registry lookup. This module never
// binds Thing itself, so the lookup is the only thing deciding which
// PyTypeObject the result gets.
struct Factory {
    Thing make() const {
        Thing t;
        t.a = 7.0;
        return t;
    }
};

}  // namespace xm
