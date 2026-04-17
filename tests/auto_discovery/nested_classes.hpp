// Regression test for nested-class auto-discovery.
//
// The generator must emit fully-qualified C++ names for classes declared
// inside other classes (Outer::Inner), while using the simple name as the
// Python/Lua/JS identifier. Fails before the fix because:
//   - bind_class<MaterialParameter> wouldn't compile (name not in scope)
//   - nested classes would shadow one another or collide with top-level names
#pragma once

#include <string>

namespace shapes {

struct Vehicle {
    int wheels = 4;
    std::string brand = "unknown";

    struct Engine {
        double horsepower = 0.0;
        int cylinders = 4;

        struct Turbo {
            bool enabled = false;
            double boost_psi = 0.0;
        };

        Turbo turbo;
    };

    Engine engine;
};

struct Bicycle {
    int wheels = 2;
    std::string frame = "steel";
};

}  // namespace shapes
