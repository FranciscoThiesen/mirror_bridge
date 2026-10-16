#pragma once
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace sb {

enum class tag { a, b };

// magic_enum::customize_t is exactly this: a class whose public API is its
// own, reached through a standard-library base. Binding the base's methods
// picks up std::pair::swap, whose const overload is ill-formed for these
// member types, and the whole module stops compiling.
class Customize : public std::pair<tag, std::string_view> {
public:
    Customize() : std::pair<tag, std::string_view>{tag::a, {}} {}
    int own_method() const { return 7; }
};

struct Base {
    int inherited() const { return 1; }
};

// A user-defined base must still contribute its methods.
struct Derived : Base {
    int own() const { return 2; }
};

// A non-template standard-library base contributes real API. Dropping every
// std base, rather than every std *template* base, took what() off this --
// which is the method the class exists to carry.
struct AppError : std::runtime_error {
    AppError() : std::runtime_error("boom") {}
    int code() const { return 42; }
};

}  // namespace sb

// Only ::std is reserved. A user namespace of the same name is legal, and an
// unanchored match took its classes' inherited methods away with no diagnostic.
namespace lookalike {
namespace std {
struct Helper {
    int helper_method() const { return 11; }
};
}  // namespace std

struct UsesLookalikeBase : lookalike::std::Helper {
    int own() const { return 12; }
};
}  // namespace lookalike
