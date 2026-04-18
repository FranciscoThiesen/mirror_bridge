// Comprehensive end-to-end demo: zero binding glue for an Open3D-shape library.
// Single bind_class_auto call per class enables: inheritance, trampolines for
// Python subclass overrides, operators, keyword-args-with-defaults, Eigen type
// conversion, and polymorphic method dispatch.
#include "mirror_bridge.hpp"
#include "mirror_bridge_auto_trampoline.hpp"
#include "src/geometry.hpp"

using namespace open3d::geometry;

MIRROR_BRIDGE_MODULE(open3d_comprehensive,
    // bind_class: regular binding for concrete classes
    mirror_bridge::bind_class<AxisAlignedBoundingBox>(m, "AxisAlignedBoundingBox");
    // bind_class_auto: Python subclasses can override any virtual via
    // reflection-driven vtable swap. No hand-written trampoline class.
    mirror_bridge::bind_class_auto<PointCloud>(m, "PointCloud");
)
