#include "python/mirror_bridge_python.hpp"
#include "wrapper_alloc.hpp"

MIRROR_BRIDGE_MODULE(wrapper_alloc,
    mirror_bridge::bind_class<alloc::Counted>(m, "Counted");
    mirror_bridge::bind_class<alloc::Fragile>(m, "Fragile");
    mirror_bridge::bind_class<alloc::Snug>(m, "Snug");
    mirror_bridge::bind_class<alloc::Wide>(m, "Wide");
    mirror_bridge::bind_class<alloc::WideNest>(m, "WideNest");
    mirror_bridge::bind_class<alloc::Holder>(m, "Holder");
)
