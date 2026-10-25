#include "python/mirror_bridge_python.hpp"
#include "part.hpp"

MIRROR_BRIDGE_MODULE(ident_a,
    mirror_bridge::bind_class<om::Part>(m, "Part");
    mirror_bridge::bind_class<om::Maker>(m, "Maker");
)
