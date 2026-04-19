#include "mirror_bridge.hpp"
#include "geometry_min.hpp"
using namespace demo;
MIRROR_BRIDGE_MODULE(pc_mb,
    mirror_bridge::bind_class<PointCloud>(m, "PointCloud");
)
