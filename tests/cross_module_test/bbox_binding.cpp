// BoundingBox binding - creates bbox.so
// This is bound in a SEPARATE shared library from Vertex
// The key test: BoundingBox.get_top_left() should return a proper Vertex wrapper

#include "python/mirror_bridge_python.hpp"
#include "rectangle.hpp"

MIRROR_BRIDGE_MODULE(bbox,
    mirror_bridge::bind_class<BoundingBox>(m, "BoundingBox");
)
