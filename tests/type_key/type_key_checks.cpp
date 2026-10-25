// core::type_key is the cross-module identity of a C++ class: the Lua backend
// uses it as the metatable's registry key and the JavaScript backend compares
// it by value. Two distinct types sharing a key therefore share a metatable
// and compare equal, which is the type confusion the boundary checks exist to
// prevent. Everything here is a static_assert, so the test is "does this
// translation unit compile".

#include "core/mirror_bridge_core.hpp"

#include <string_view>

namespace {

using mirror_bridge::core::type_key;
using mirror_bridge::core::type_key_is_distinctive;

template<typename A, typename B>
consteval bool same_key() {
    return std::string_view(type_key<A>) == std::string_view(type_key<B>);
}

template<typename T, int N = 0>
struct Box { T value{}; };

struct Plain { int x = 0; };
namespace deep { struct Inner { int x = 0; }; }

enum class Colour { Red, Green };

template<Colour C>
struct EnumParam { int x = 0; };

// ---------------------------------------------------------------- the bug
// spell() tested is_pointer_type before is_const, and remove_pointer drops
// the qualifiers on the pointer itself, so a const pointer spelled the same
// as a mutable one.
static_assert(!same_key<Box<int*>, Box<int* const>>(),
              "Box<int*> and Box<int* const> share a key");
static_assert(!same_key<Box<int*>, Box<int* volatile>>(),
              "Box<int*> and Box<int* volatile> share a key");
static_assert(!same_key<Box<int* const>, Box<int* volatile>>(),
              "Box<int* const> and Box<int* volatile> share a key");

// The qualifiers land on the pointer, not on the pointee. Only nested ones
// survive: type_key strips the top level, so type_key<int* const> is "int*"
// on purpose.
static_assert(std::string_view(type_key<Box<int* const>>) == "Box<int* const, 0>");
static_assert(std::string_view(type_key<Box<const int*>>) == "Box<int const*, 0>");
static_assert(std::string_view(type_key<int*>) == "int*");

// ------------------------------------------------- what must not have moved
// type_key strips top-level cv-ref from the type it is asked about, so these
// are deliberately the same.
static_assert(same_key<Plain, const Plain&>(), "type_key must ignore top-level cv-ref");
static_assert(same_key<Plain, Plain&&>(), "type_key must ignore top-level cv-ref");

static_assert(std::string_view(type_key<Plain>) == "Plain");
static_assert(std::string_view(type_key<deep::Inner>) == "deep::Inner");
static_assert(std::string_view(type_key<const int>) == "int");
static_assert(!same_key<Box<int>, Box<unsigned>>(), "distinct arguments must differ");
static_assert(!same_key<Box<int, 1>, Box<int, 2>>(), "distinct arguments must differ");
static_assert(!same_key<Box<int>, Box<Box<int>>>(), "nesting must differ");
static_assert(!same_key<Plain, deep::Inner>(), "distinct classes must differ");

// ---------------------------------------- keys that no compiler can be sure of
// An enum-valued template argument is spelled through display_string_of,
// which is implementation-defined: clang-p2996 collapses every such argument
// to one placeholder while GCC writes the enumerator out. Which of the two
// happens is not the invariant; the invariant is that a key is never both
// ambiguous and accepted, because bind_class is what consumes it.
static_assert(!same_key<EnumParam<Colour::Red>, EnumParam<Colour::Green>>() ||
                  !type_key_is_distinctive<EnumParam<Colour::Red>>(),
              "two specialisations share a key and the guard still accepts them");

static_assert(type_key_is_distinctive<Plain>(), "a plain class must be bindable");
static_assert(type_key_is_distinctive<deep::Inner>(), "a nested class must be bindable");
static_assert(type_key_is_distinctive<Box<int>>(), "a plain specialisation must be bindable");

}  // namespace

int main() { return 0; }
