// Lua binding for Vec2
#include "lua/mirror_bridge_lua.hpp"
#include "vec2.hpp"

MIRROR_BRIDGE_LUA_MODULE(vec2,
    mirror_bridge::lua::bind_class<Vec2>(L, "Vec2");
)
