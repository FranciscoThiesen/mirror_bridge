#include "lua/mirror_bridge_lua.hpp"
#include "const_members.hpp"

// A class with a const data member, or with a member whose type has no default
// constructor, used to take the whole Lua module down at compile time: the
// setter declared a local of the member's type to convert into, which is
// ill-formed for both. tests/const_members had only a Python test, which is
// exactly why nobody saw it.
MIRROR_BRIDGE_LUA_MODULE(const_members_lua,
    mirror_bridge::lua::bind_class<Sensor>(L, "Sensor");
    mirror_bridge::lua::bind_class<Holder>(L, "Holder");
)
