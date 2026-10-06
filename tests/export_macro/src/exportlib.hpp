#pragma once

#include "exportlib_config.hpp"

namespace exportlib {

class EXPORTLIB_API Widget {
public:
    int width = 3;
    int height = 4;

    int area() const { return width * height; }

    class EXPORTLIB_API Handle {
    public:
        int id = 7;
        int doubled() const { return id * 2; }
    };

private:
    // Binding an inaccessible class is ill-formed, so discovery must not
    // report this one however deep the walk goes.
    class Secret {
    public:
        int token = 42;
    };
};

class EXPORTLIB_API WidgetCursor;

class EXPORTLIB_API WidgetList {
public:
    // An alias is a type, and a class type. Following one re-enters a class
    // the walk has already entered, which is how jsoncpp's `using iterator =
    // ValueIterator;` ran the discovery unit into the constexpr step limit.
    // These two alias each other, so a walk that recurses through aliases
    // never terminates.
    using iterator = WidgetCursor;

    int count = 2;
    int twice() const { return count * 2; }
};

class EXPORTLIB_API WidgetCursor {
public:
    using container = WidgetList;

    int index = 0;
    int next() { return ++index; }
};

// No macro in sight, and still unreadable to a scan of the text: `alignas`
// is the token after `struct`, so the scan takes it for the class name and
// the module tries to bind a keyword.
struct alignas(64) Wide {
    double first = 1.5;
    double last = 2.5;

    double sum() const { return first + last; }
};

// An attribute between the keyword and the name matches no name at all, so
// the scan drops the class without saying so.
struct [[nodiscard]] Tagged {
    int tag = 11;

    int tagged() const { return tag * 2; }
};

class EXPORTLIB_API Sealed final {
public:
    int serial = 5;

    int next_serial() const { return serial + 1; }
};

enum class Status { Idle, Busy };

// MIRROR_BRIDGE_SKIP
class EXPORTLIB_API Internal {
public:
    int scratch = 0;
};

}  // namespace exportlib
