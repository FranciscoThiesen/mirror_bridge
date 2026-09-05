#pragma once
// Fixture for the template instantiation planner: nothing here says which
// instantiations to bind; `mirror_bridge generate` has to work it out.
#include <array>
#include <cmath>
#include <concepts>
#include <string>
#include <vector>

namespace geom {

template <typename T>
struct Vector3 {
    T x{}, y{}, z{};
    Vector3() = default;
    Vector3(T x_, T y_, T z_) : x(x_), y(y_), z(z_) {}
    T dot(const Vector3& o) const { return x * o.x + y * o.y + z * o.z; }
    double norm() const { return std::sqrt(double(dot(*this))); }
    Vector3 scaled(T k) const { return {x * k, y * k, z * k}; }
    template <typename U> Vector3<U> cast() const { return {U(x), U(y), U(z)}; }
};

// Aliases are declarations: these are the canonical instantiations and
// their Python names.
using Vec3f = Vector3<float>;
using Vec3d = Vector3<double>;
typedef Vector3<int> Vec3i;

template <typename T, int N>
struct Matrix {
    std::array<T, N * N> m{};
    int size() const { return N; }
    T trace() const { T t{}; for (int i = 0; i < N; ++i) t += m[i * N + i]; return t; }
};
using Mat3f = Matrix<float, 3>;

// A plain class whose members drag in a specialization nobody aliased
// (Vector3<long> -> Python name Vector3_long).
struct Robot {
    std::string name;
    Vector3<double> position;
    Vector3<long> ticks;
    std::vector<Vec3f> path;
    double distance_to(const Vector3<double>& p) const { return (position.scaled(-1.0).dot(p)); }
    template <typename U> Vector3<U> ticks_as() const { return ticks.cast<U>(); }
};

// 2D C arrays convert both ways (regression: nested rows were copied by value).
struct Grid {
    float cells[2][3]{};
    float sum() const { float s = 0; for (auto& r : cells) for (float v : r) s += v; return s; }
};

// Free functions using specializations.
inline float dot3(const Vec3f& a, const Vec3f& b) { return a.dot(b); }
// Legal C++ (only the constructor is implicitly instantiated), but a binding
// needs every member and Vector3<std::string>::dot has no operator*: the
// function is reported and left unbound rather than breaking the module.
inline Vector3<std::string> labels() { return {"x", "y", "z"}; }

// Constrained function template: the constraint is the instantiation contract.
template <std::floating_point T>
T clamp(T v, T lo, T hi) { return v < lo ? lo : v > hi ? hi : v; }

// Unconstrained function template: only the compiler can tell which types work.
template <typename T>
T twice(T v) { return v + v; }

// Two type parameters.
template <typename A, typename B>
double mix(A a, B b) { return double(a) * 0.5 + double(b) * 0.5; }

// Instantiates fine but takes a pointer Python cannot pass: skipped at bind time.
template <typename T>
T deref(T* p) { return *p; }

// A class template nobody mentions gets the Python scalar baseline.
template <typename T>
struct Stack {
    std::vector<T> items;
    void push(T v) { items.push_back(v); }
    T pop() { T v = items.back(); items.pop_back(); return v; }
    std::size_t size() const { return items.size(); }
};

// Explicit specialization, reached through count_true's signature.
template <> struct Vector3<bool> { bool x, y, z; int count() const { return x + y + z; } };
inline int count_true(const Vector3<bool>& v) { return v.count(); }

// Overloaded plain functions have no unique address: reported, not bound.
inline int overloaded(int v) { return v; }
inline double overloaded(double v) { return v; }

}  // namespace geom
