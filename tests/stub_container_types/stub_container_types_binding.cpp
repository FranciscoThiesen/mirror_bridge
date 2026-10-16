#include "python/mirror_bridge_python.hpp"
#include "containers.hpp"

MIRROR_BRIDGE_MODULE(stub_container_types,
    mirror_bridge::bind_class<stubs::Shapes>(m, "Shapes");
)
