#include "python/mirror_bridge_python.hpp"
#include "numeric_conversion.hpp"

MIRROR_BRIDGE_MODULE(numeric_conversion,
    mirror_bridge::bind_class<numconv::Numbers>(m, "Numbers");
)
