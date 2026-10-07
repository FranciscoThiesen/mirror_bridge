#pragma once

// A vendored third-party header, reached through -I and nothing else. It
// reopens the namespace the project's own header opens and it is called
// config.h, which is the most common header name there is. Neither is
// unusual, and together they are enough: discovery matches a header by the
// include spelling the driver recorded, and "config.h" is the end of this
// path too.
namespace lib {

struct VendorInternal {
    int v = 9;
    int nine() const { return v; }
};

struct VendorOther {
    int w = 8;
};

}  // namespace lib
