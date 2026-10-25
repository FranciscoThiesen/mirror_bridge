#pragma once
#include <cstdint>
#include <string>

namespace en {

enum class Color { Red, Green, Blue };

// Non-contiguous and negative values: an IntEnum keeps whatever the C++
// enumerator says, it does not renumber.
enum class Signal : std::int16_t { Quit = -1, Idle = 0, Run = 7, Halt = 100 };

// Unscoped, which reflection reports the same way.
enum Plain { Alpha, Beta, Gamma };

// `None` is a Python keyword, so `Permission.None` is a syntax error rather
// than an AttributeError and there is no spelling a caller could find.
enum class Permission { None = 0, Read = 1, Write = 2, All = 3 };

// Already has the name the renaming would produce, so the renaming has to
// keep going rather than collide.
enum class Awkward { None = 0, None_ = 1 };

struct Outer {
    enum class Kind { First, Second };
    Kind kind = Kind::First;
};

struct Light {
    Color colour = Color::Red;
    Signal state = Signal::Idle;

    Color get_colour() const { return colour; }
    void set_colour(Color c) { colour = c; }
    bool is_red() const { return colour == Color::Red; }
};

inline std::string name_of(Color c) {
    switch (c) {
        case Color::Red: return "red";
        case Color::Green: return "green";
        default: return "blue";
    }
}

inline Color brighter(Color c) {
    return c == Color::Blue ? Color::Blue : static_cast<Color>(static_cast<int>(c) + 1);
}

inline int permission_bits(Permission p) { return static_cast<int>(p); }

}  // namespace en
