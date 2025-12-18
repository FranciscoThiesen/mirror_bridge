#include "lua/mirror_bridge_lua.hpp"
#include "mandelbrot.hpp"

// Use mandelbrot_lua to match the .so filename
MIRROR_BRIDGE_LUA_MODULE(mandelbrot_lua,
    mirror_bridge::lua::bind_class<Mandelbrot>(L, "Mandelbrot");
)
