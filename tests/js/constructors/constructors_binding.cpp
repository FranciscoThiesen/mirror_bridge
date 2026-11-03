#include "javascript/mirror_bridge_javascript.hpp"
#include "constructors.hpp"

MIRROR_BRIDGE_JS_MODULE(constructors,
    mirror_bridge::javascript::bind_class<Rect>(env, m, "Rect");
    mirror_bridge::javascript::bind_class<Cell>(env, m, "Cell");
    mirror_bridge::javascript::bind_class<Point2>(env, m, "Point2");
    mirror_bridge::javascript::bind_class<Segment>(env, m, "Segment");
    mirror_bridge::javascript::bind_class<Tagged>(env, m, "Tagged");
    mirror_bridge::javascript::bind_class<Celsius>(env, m, "Celsius");
    mirror_bridge::javascript::bind_class<Gauge>(env, m, "Gauge");
    mirror_bridge::javascript::bind_class<FixedGauge>(env, m, "FixedGauge");
    mirror_bridge::javascript::bind_class<Config>(env, m, "Config");
    mirror_bridge::javascript::bind_class<Widget>(env, m, "Widget");
    mirror_bridge::javascript::bind_class<Checked>(env, m, "Checked");
    mirror_bridge::javascript::bind_class<Line>(env, m, "Line");
    mirror_bridge::javascript::bind_class<View>(env, m, "View");
    mirror_bridge::javascript::bind_class<Big>(env, m, "Big");
    mirror_bridge::javascript::bind_class<Mixed>(env, m, "Mixed");
    mirror_bridge::javascript::bind_class<Linked>(env, m, "Linked");
    mirror_bridge::javascript::bind_class<Meas>(env, m, "Meas");
)
