#pragma once
// A header with no classes at all: the class scan has no reason to include
// it, the planner does (its function templates instantiate over geom's types).
#include "geom.hpp"

namespace geom {

template <typename T>
T largest(const Vector3<T>& v) { return v.x > v.y ? (v.x > v.z ? v.x : v.z) : (v.y > v.z ? v.y : v.z); }

inline int answer() { return 42; }

// A namespace-scope operator has no identifier. Asking the planner for one
// used to end its consteval evaluation, so the discovery TU failed to
// compile and every free function and template instantiation in the module
// was silently dropped. Real headers are full of these.
struct Tally { int n = 0; };
inline Tally operator+(const Tally& a, const Tally& b) { return Tally{a.n + b.n}; }

// noexcept is part of the function type, so this needs its own traits
// specialization. Without one the module did not fail to bind it, it failed
// to compile at all.
inline double half(double x) noexcept { return 0.5 * x; }

}  // namespace geom
