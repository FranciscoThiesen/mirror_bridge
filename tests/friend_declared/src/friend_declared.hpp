#pragma once

// pmp-library's shape. `friend class SurfaceMesh;` inside a template class
// introduces the name into namespace pmp before the class itself is declared,
// and clang-p2996 then does not report it as a member of that namespace even
// after the real definition appears -- so the namespace walk never sees the
// class the whole library exists for. Measured on pmp 3.0.0:
// members_of(^^pmp) lists 86 named members and SurfaceMesh is not one of them,
// while ^^pmp::SurfaceMesh resolves and is_complete_type says yes.
//
// What recovers it is the library's own signatures: a class its functions hand
// back or take in is part of its API.
namespace fd {

template <class T>
class Property {
    friend class Mesh;   // the name enters the namespace here, undeclared
    T value_{};

public:
    T get() const { return value_; }
};

class Mesh {
    int count_ = 3;

public:
    int count() const { return count_; }
    void grow() { ++count_; }
};

// The only route reflection has to Mesh.
inline int count_of(const Mesh& m) { return m.count(); }

// A class reachable only as a return type, to cover the other half.
class Report {
    int value_ = 7;

public:
    int value() const { return value_; }
};

inline Report report_for(const Mesh&) { return Report{}; }

}  // namespace fd
