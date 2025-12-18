// Python binding for Vec2
#include "mirror_bridge.hpp"
#include "vec2.hpp"

MIRROR_BRIDGE_MODULE(vec2,
    mirror_bridge::bind_class<Vec2>(m, "Vec2");
)
