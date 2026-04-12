#include "mirror_bridge.hpp"
#include "smart_ptr_class.hpp"

MIRROR_BRIDGE_MODULE(smart_ptr_test,
    mirror_bridge::bind_class<Point>(m, "Point");
    mirror_bridge::bind_class<Shape>(m, "Shape");
)
