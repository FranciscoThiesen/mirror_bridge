#include "python/mirror_bridge_python.hpp"
#include "big/thing.hpp"

MIRROR_BRIDGE_MODULE(both_big,
    mirror_bridge::bind_class<xm::Thing>(m, "Thing");
)
