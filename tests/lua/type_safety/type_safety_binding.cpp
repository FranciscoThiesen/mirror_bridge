#include "lua/mirror_bridge_lua.hpp"
#include "type_safety.hpp"

using namespace ts;

MIRROR_BRIDGE_LUA_MODULE(type_safety,
    mirror_bridge::lua::bind_class<Curve>(L, "Curve");
    mirror_bridge::lua::bind_class<Label>(L, "Label");
    mirror_bridge::lua::bind_class<Base>(L, "Base");
    mirror_bridge::lua::bind_class<Derived>(L, "Derived");
    mirror_bridge::lua::bind_class<First>(L, "First");
    mirror_bridge::lua::bind_class<Second>(L, "Second");
    mirror_bridge::lua::bind_class<Both>(L, "Both");
    mirror_bridge::lua::bind_class<Portfolio>(L, "Portfolio");
)
