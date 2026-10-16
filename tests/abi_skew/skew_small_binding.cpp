#include "python/mirror_bridge_python.hpp"
#include "small/thing.hpp"

MIRROR_BRIDGE_MODULE(skew_small,
    mirror_bridge::bind_class<xm::Thing>(m, "Thing");
)
