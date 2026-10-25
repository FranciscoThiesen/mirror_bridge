#include "javascript/mirror_bridge_javascript.hpp"
#include "type_safety.hpp"

using namespace ts;

MIRROR_BRIDGE_JS_MODULE(type_safety,
    mirror_bridge::javascript::bind_class<Curve>(env, m, "Curve");
    mirror_bridge::javascript::bind_class<Label>(env, m, "Label");
    mirror_bridge::javascript::bind_class<Base>(env, m, "Base");
    mirror_bridge::javascript::bind_class<Derived>(env, m, "Derived");
    mirror_bridge::javascript::bind_class<First>(env, m, "First");
    mirror_bridge::javascript::bind_class<Second>(env, m, "Second");
    mirror_bridge::javascript::bind_class<Both>(env, m, "Both");
    mirror_bridge::javascript::bind_class<Portfolio>(env, m, "Portfolio");
)
