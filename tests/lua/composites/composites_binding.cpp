#include "lua/mirror_bridge_lua.hpp"
#include "composites.hpp"

MIRROR_BRIDGE_LUA_MODULE(composites,
    mirror_bridge::lua::bind_class<Point2>(L, "Point2");
    mirror_bridge::lua::bind_class<Cell>(L, "Cell");
    mirror_bridge::lua::bind_class<Shape>(L, "Shape");
    mirror_bridge::lua::bind_class<Circle>(L, "Circle");
    mirror_bridge::lua::bind_class<Scene>(L, "Scene");
    mirror_bridge::lua::bind_class<Body>(L, "Body");
    mirror_bridge::lua::bind_class<Collections>(L, "Collections");
    mirror_bridge::lua::bind_class<Color>(L, "Color");
    mirror_bridge::lua::bind_class<Mark>(L, "Mark");
    mirror_bridge::lua::bind_class<Choices>(L, "Choices");
    mirror_bridge::lua::bind_class<Node>(L, "Node");
    mirror_bridge::lua::bind_class<Owner>(L, "Owner");
    mirror_bridge::lua::bind_class<Labelled>(L, "Labelled");
    mirror_bridge::lua::bind_class<Maps>(L, "Maps");
)
