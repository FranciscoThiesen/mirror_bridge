#include "mirror_bridge.hpp"
#include "src/api.hpp"
using namespace api;
MIRROR_BRIDGE_MODULE(kwargs_defaults_test,
    mirror_bridge::bind_class<PointCloud>(m, "PointCloud");
)
