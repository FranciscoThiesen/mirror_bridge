#pragma once

#include <vendorlib/config.h>

namespace lib {

struct MyConfig {
    int n = 1;
    int twice() const { return n * 2; }
};

}  // namespace lib
