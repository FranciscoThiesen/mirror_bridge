// The same two classes, bound a second time in a second module, which is
// what a user gets when two independently generated modules happen to share
// a header.
#include "python/mirror_bridge_python.hpp"
#include "part.hpp"

MIRROR_BRIDGE_MODULE(ident_b,
    mirror_bridge::bind_class<om::Part>(m, "Part");
    mirror_bridge::bind_class<om::Maker>(m, "Maker");
)
