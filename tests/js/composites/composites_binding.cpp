#include "javascript/mirror_bridge_javascript.hpp"
#include "composites.hpp"

MIRROR_BRIDGE_JS_MODULE(composites,
    mirror_bridge::javascript::bind_class<Point2>(env, m, "Point2");
    mirror_bridge::javascript::bind_class<Cell>(env, m, "Cell");
    mirror_bridge::javascript::bind_class<Shape>(env, m, "Shape");
    mirror_bridge::javascript::bind_class<Circle>(env, m, "Circle");
    mirror_bridge::javascript::bind_class<Scene>(env, m, "Scene");
    mirror_bridge::javascript::bind_class<Body>(env, m, "Body");
    mirror_bridge::javascript::bind_class<Collections>(env, m, "Collections");
    mirror_bridge::javascript::bind_class<Color>(env, m, "Color");
    mirror_bridge::javascript::bind_class<Mark>(env, m, "Mark");
    mirror_bridge::javascript::bind_class<Choices>(env, m, "Choices");
    mirror_bridge::javascript::bind_class<Node>(env, m, "Node");
    mirror_bridge::javascript::bind_class<Owner>(env, m, "Owner");
    mirror_bridge::javascript::bind_class<Labelled>(env, m, "Labelled");
    mirror_bridge::javascript::bind_class<Maps>(env, m, "Maps");
)
