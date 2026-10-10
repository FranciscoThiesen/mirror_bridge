#include "python/mirror_bridge_python.hpp"
#include "ctor_defaults.hpp"

MIRROR_BRIDGE_MODULE(ctor_defaults,
    mirror_bridge::bind_class<cd::Conn>(m, "Conn");
    mirror_bridge::bind_class<cd::Mix>(m, "Mix");
    mirror_bridge::bind_class<cd::Strict>(m, "Strict");
)
