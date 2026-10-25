#pragma once

// A library that opens its namespaces from a macro, the way nlohmann-json
// does with NLOHMANN_JSON_NAMESPACE_BEGIN. The text scan cannot expand a
// macro, and it blanks preprocessor lines, so neither the opening brace nor
// its closer is visible to it: namespace names stack instead of popping and
// the enclosing namespace is missing altogether.
#define MACRONS_BEGIN namespace macrons { inline namespace v1 {
#define MACRONS_END   } }

MACRONS_BEGIN

namespace detail {
struct Helper {
    int count = 3;
    int doubled() const { return count * 2; }
};
}  // namespace detail

struct Widget {
    double width = 1.5;
    double height = 2.5;
    double area() const { return width * height; }
};

MACRONS_END
