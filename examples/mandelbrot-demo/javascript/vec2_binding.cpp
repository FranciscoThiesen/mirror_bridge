// JavaScript binding for Vec2
#include "javascript/mirror_bridge_javascript.hpp"
#include "vec2.hpp"

MIRROR_BRIDGE_JS_MODULE(vec2,
    mirror_bridge::javascript::bind_class<Vec2>(env, m, "Vec2");
)
