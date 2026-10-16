#include "python/mirror_bridge_python.hpp"
#include "small/thing.hpp"

// The other half of the skew: here both modules bind Thing, rather than one
// binding it and the other only returning it.
MIRROR_BRIDGE_MODULE(both_small,
    mirror_bridge::bind_class<xm::Thing>(m, "Thing");
)
