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

// box2d: a variadic function has no FunctionTraits specialization, so
// reaching one is a hard error rather than a declined binding.
inline int sum_of(int count, ...) { return count; }

// box2d's b2BlockAllocator::Free(void*, int). void* was allowed through the
// parameter gate as an "opaque handle" and no from_python was ever written
// for it, so the gate said yes and the conversion had no overload.
struct Allocator {
    int freed = 0;
    void release(void* block, int size) { (void)block; freed += size; }
    int usable(int n) const { return n + freed; }
};

// tinyxml2: a nested class the enclosing class keeps private still reaches
// the template planner through signature closure, and the generated binding
// cannot name it.
template <int N>
class Pool {
public:
    int capacity = N;
private:
    struct Block { int tag = 0; };
    Block* head_ = nullptr;
public:
    Block* raw() { return head_; }
};

struct UsesPool {
    Pool<16> pool;
    int cap() const { return pool.capacity; }
};

}  // namespace decl
