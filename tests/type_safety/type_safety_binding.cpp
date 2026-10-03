#include "python/mirror_bridge_python.hpp"
#include "type_safety.hpp"

using namespace ts;

MIRROR_BRIDGE_MODULE(type_safety,
    mirror_bridge::bind_class<Curve>(m, "Curve");
    mirror_bridge::bind_class<Label>(m, "Label");
    mirror_bridge::bind_class<Base>(m, "Base");
    mirror_bridge::bind_class<Derived>(m, "Derived");
    mirror_bridge::bind_class<First>(m, "First");
    mirror_bridge::bind_class<Second>(m, "Second");
    mirror_bridge::bind_class<Both>(m, "Both");
    mirror_bridge::bind_class<Square>(m, "Square");
    mirror_bridge::bind_class<Handle>(m, "Handle");
    mirror_bridge::bind_class<Portfolio>(m, "Portfolio");
    mirror_bridge::bind_class<Inspector>(m, "Inspector");
    mirror_bridge::bind_class<Ticks>(m, "Ticks");
    mirror_bridge::bind_class<VBase>(m, "VBase");
    mirror_bridge::bind_class<VDiamond>(m, "VDiamond");

    mirror_bridge::bind_function<&take_curve_cref>(m, "take_curve_cref");
    mirror_bridge::bind_function<&take_curve_value>(m, "take_curve_value");
    mirror_bridge::bind_function<&take_label>(m, "take_label");
    mirror_bridge::bind_function<&take_base>(m, "take_base");
    mirror_bridge::bind_function<&take_first>(m, "take_first");
    mirror_bridge::bind_function<&take_second>(m, "take_second");
    mirror_bridge::bind_function<&take_vbase>(m, "take_vbase");
    mirror_bridge::bind_function<&take_derived>(m, "take_derived");
    mirror_bridge::bind_function<&sum_curves>(m, "sum_curves");
)
