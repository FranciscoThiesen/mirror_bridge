#include "lua/mirror_bridge_lua.hpp"
#include "smart_ptr_class.hpp"

MIRROR_BRIDGE_LUA_MODULE(smart_ptr_test,
    mirror_bridge::lua::bind_class<Data>(L, "Data");
    mirror_bridge::lua::bind_class<ResourceMgr>(L, "ResourceMgr");
    mirror_bridge::lua::bind_class<ShapeHolder>(L, "ShapeHolder");
)
