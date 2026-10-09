#pragma once
#include <cstddef>
#include <string>
#include <vector>

// Shapes that must be declined rather than failing the whole module. Every
// one of these came from a real library in the compatibility corpus, where a
// single awkward class or parameter cost all the others.
namespace decl {

struct Plain {
    double x = 1.0;
    double twice() const { return x * 2.0; }
};

// tinyxml2::XMLElement: the library deliberately makes it undestroyable from
// outside, so no wrapper can free it.
class Undestroyable {
public:
    int value = 7;
    static Undestroyable* make() { return new Undestroyable(); }
private:
    ~Undestroyable() = default;
};

// Json::Value: satisfies value_type + begin() + size(), so it looks like a
// container, and offers none of indexing by a signed index, push_back or
// insert. Nothing can be put into it.
struct Unfillable {
    using value_type = int;
    std::vector<int> data;
    const int* begin() const { return data.data(); }
    const int* end() const { return data.data() + data.size(); }
    std::size_t size() const { return data.size(); }
};

// cxxopts::KeyValue: a container element with no default constructor, which
// the element-wise fill loop cannot build.
struct NoDefault {
    std::string key;
    explicit NoDefault(std::string k) : key(std::move(k)) {}
};

struct NoDefaultBag {
    using value_type = NoDefault;
    std::vector<NoDefault> items;
    auto begin() const { return items.begin(); }
    auto end() const { return items.end(); }
    std::size_t size() const { return items.size(); }
    void push_back(const NoDefault& v) { items.push_back(v); }
};

// The point: a method taking one of the awkward types must not cost the class
// its other methods, nor the module its other classes.
struct Consumer {
    double scale = 2.0;
    double usable(double v) const { return v * scale; }
    std::size_t takes_unfillable(const Unfillable& u) const { return u.size(); }
    std::size_t takes_nodefault(const NoDefaultBag& b) const { return b.size(); }
};

}  // namespace decl
