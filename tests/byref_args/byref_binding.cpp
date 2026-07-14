#include "python/mirror_bridge_python.hpp"
#include "byref.hpp"

MIRROR_BRIDGE_MODULE(byref,
    mirror_bridge::bind_class<Tracked>(m, "Tracked");
    mirror_bridge::bind_class<Mutator>(m, "Mutator");
)
