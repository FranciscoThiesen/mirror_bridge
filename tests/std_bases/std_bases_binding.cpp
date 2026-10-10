#include "python/mirror_bridge_python.hpp"
#include "std_bases.hpp"

MIRROR_BRIDGE_MODULE(std_bases,
    mirror_bridge::bind_class<sb::Customize>(m, "Customize");
    mirror_bridge::bind_class<sb::Derived>(m, "Derived");
)
