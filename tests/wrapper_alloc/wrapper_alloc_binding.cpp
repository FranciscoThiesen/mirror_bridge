#include "python/mirror_bridge_python.hpp"
#include "wrapper_alloc.hpp"

MIRROR_BRIDGE_MODULE(wrapper_alloc,
    mirror_bridge::bind_class<alloc::Counted>(m, "Counted");
    mirror_bridge::bind_class<alloc::Wide>(m, "Wide");
    mirror_bridge::bind_class<alloc::Holder>(m, "Holder");
)
