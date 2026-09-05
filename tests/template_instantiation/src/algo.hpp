#pragma once
// A header with no classes at all: the class scan has no reason to include
// it, the planner does (its function templates instantiate over geom's types).
#include "geom.hpp"

namespace geom {

template <typename T>
T largest(const Vector3<T>& v) { return v.x > v.y ? (v.x > v.z ? v.x : v.z) : (v.y > v.z ? v.y : v.z); }

inline int answer() { return 42; }

}  // namespace geom
