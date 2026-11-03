#include "lua/mirror_bridge_lua.hpp"
#include "constructors.hpp"

MIRROR_BRIDGE_LUA_MODULE(constructors,
    mirror_bridge::lua::bind_class<Rect>(L, "Rect");
    mirror_bridge::lua::bind_class<Cell>(L, "Cell");
    mirror_bridge::lua::bind_class<Point2>(L, "Point2");
    mirror_bridge::lua::bind_class<Segment>(L, "Segment");
    mirror_bridge::lua::bind_class<Tagged>(L, "Tagged");
    mirror_bridge::lua::bind_class<Celsius>(L, "Celsius");
    mirror_bridge::lua::bind_class<Gauge>(L, "Gauge");
    mirror_bridge::lua::bind_class<FixedGauge>(L, "FixedGauge");
    mirror_bridge::lua::bind_class<Config>(L, "Config");
    mirror_bridge::lua::bind_class<Widget>(L, "Widget");
    mirror_bridge::lua::bind_class<Checked>(L, "Checked");
    mirror_bridge::lua::bind_class<Line>(L, "Line");
    mirror_bridge::lua::bind_class<View>(L, "View");
    mirror_bridge::lua::bind_class<Big>(L, "Big");
    mirror_bridge::lua::bind_class<Mixed>(L, "Mixed");
    mirror_bridge::lua::bind_class<Linked>(L, "Linked");
    mirror_bridge::lua::bind_class<Meas>(L, "Meas");
)
