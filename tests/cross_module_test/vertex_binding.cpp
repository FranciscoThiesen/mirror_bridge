// Vertex binding - creates vertex.so
// This is bound in a SEPARATE shared library from BoundingBox

#include "python/mirror_bridge_python.hpp"
#include "point.hpp"

MIRROR_BRIDGE_MODULE(vertex,
    mirror_bridge::bind_class<Vertex>(m, "Vertex");
)
