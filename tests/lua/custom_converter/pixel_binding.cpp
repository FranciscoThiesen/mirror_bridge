// Lua binding for Pixel with custom Color3 converter
// Demonstrates the custom type converter extension point for Lua

#include "color3.hpp"

// Include Lua API first
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

// Forward declare the custom converter BEFORE including mirror_bridge_lua
namespace mirror_bridge::lua {

// Define to_lua/from_lua for Color3 type
// Converts Color3 <-> Lua table {r, g, b} or {[1], [2], [3]}

inline void to_lua(lua_State* L, const Color3& color) {
    // Convert Color3 to Lua table {r=r, g=g, b=b, [1]=r, [2]=g, [3]=b}
    // This allows both named and indexed access
    lua_createtable(L, 3, 3);

    // Indexed access (1-based in Lua)
    lua_pushinteger(L, color.r);
    lua_rawseti(L, -2, 1);
    lua_pushinteger(L, color.g);
    lua_rawseti(L, -2, 2);
    lua_pushinteger(L, color.b);
    lua_rawseti(L, -2, 3);

    // Named access
    lua_pushinteger(L, color.r);
    lua_setfield(L, -2, "r");
    lua_pushinteger(L, color.g);
    lua_setfield(L, -2, "g");
    lua_pushinteger(L, color.b);
    lua_setfield(L, -2, "b");
}

inline bool from_lua(lua_State* L, int idx, Color3& color) {
    if (!lua_istable(L, idx)) return false;

    // Try indexed access first (array-style)
    lua_rawgeti(L, idx, 1);
    if (lua_isinteger(L, -1)) {
        color.r = static_cast<uint8_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);

        lua_rawgeti(L, idx, 2);
        color.g = static_cast<uint8_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);

        lua_rawgeti(L, idx, 3);
        color.b = static_cast<uint8_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);

        return true;
    }
    lua_pop(L, 1);

    // Try named access (dict-style)
    lua_getfield(L, idx, "r");
    if (lua_isinteger(L, -1)) {
        color.r = static_cast<uint8_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, idx, "g");
        color.g = static_cast<uint8_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, idx, "b");
        color.b = static_cast<uint8_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);

        return true;
    }
    lua_pop(L, 1);

    return false;
}

} // namespace mirror_bridge::lua

// Now include mirror_bridge_lua - it will find our custom converters
#include "lua/mirror_bridge_lua.hpp"

MIRROR_BRIDGE_LUA_MODULE(pixel,
    mirror_bridge::lua::bind_class<Pixel>(L, "Pixel");
)
