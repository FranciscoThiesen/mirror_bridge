#include "python/mirror_bridge_python.hpp"
#include "const_members.hpp"

MIRROR_BRIDGE_MODULE(const_members,
    mirror_bridge::bind_class<Sensor>(m, "Sensor");
    mirror_bridge::bind_class<Fixed>(m, "Fixed");
    mirror_bridge::bind_class<Holder>(m, "Holder");
)
