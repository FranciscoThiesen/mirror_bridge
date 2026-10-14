#pragma once

#include <string>

// A Python module exposes every class under its unqualified name, so two
// classes with the same simple name cannot both answer to it. Which one does
// used to be whichever was bound last, in silence -- and the last one bound is
// whichever the header happened to declare second, which here is the
// implementation detail.
//
// Six of the seven libraries in the compatibility corpus whose discovery was
// poisoned by export macros turned out to have this underneath: doctest's two
// `String`, fmt's two `Factor`, spdlog's two `tcp_client`, abseil's two
// `static_var`, googletest's two `GTestExpectFatalFailureHelper`.
namespace lib {

struct Config {
    int timeout = 30;
    int doubled() const { return timeout * 2; }
};

// Same simple name, deeper namespace. Nothing a caller of the library is
// meant to reach.
namespace detail {
struct Config {
    std::string scratch = "internal";
    int marker() const { return 999; }
};
}  // namespace detail

// Not a collision: one class, one name, and it must still bind.
struct Session {
    int id = 5;
    int next() const { return id + 1; }
};

}  // namespace lib
