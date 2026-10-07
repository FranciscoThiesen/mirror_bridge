#include "python/mirror_bridge_python.hpp"
#include "pyi_keywords.hpp"

MIRROR_BRIDGE_MODULE(pyi_keywords,
    mirror_bridge::bind_class<kwtest::Greeks>(m, "Greeks");
    mirror_bridge::bind_function<&kwtest::ratio>(m, "ratio");
)
