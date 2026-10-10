#pragma once
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

}  // namespace sb
