#include "mirror_bridge.hpp"
#include "expected_class.hpp"

MIRROR_BRIDGE_MODULE(expected_test,
    mirror_bridge::bind_class<MathService>(m, "MathService");
)
