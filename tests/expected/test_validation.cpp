// Compile-time validation test: This file should FAIL to compile because
// BadClass contains a function pointer member which mirror_bridge cannot convert.
//
// Expected error:
//   "bind_class<T>: T contains members with types that mirror_bridge cannot convert."

#include "mirror_bridge.hpp"

struct BadClass {
    int good_member = 42;
    void (*callback)(int);  // Function pointer - not convertible by mirror_bridge
};

MIRROR_BRIDGE_MODULE(validation_test,
    mirror_bridge::bind_class<BadClass>(m, "BadClass");
)
