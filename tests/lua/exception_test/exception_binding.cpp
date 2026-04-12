#include "lua/mirror_bridge_lua.hpp"
#include "exception_class.hpp"

MIRROR_BRIDGE_LUA_MODULE(exception_test,
    mirror_bridge::lua::bind_class<ThrowingService>(L, "ThrowingService");
)
