#include "python/mirror_bridge_python.hpp"
#include "big/thing.hpp"

MIRROR_BRIDGE_MODULE(skew_big,
    mirror_bridge::bind_class<xm::Factory>(m, "Factory");
)
