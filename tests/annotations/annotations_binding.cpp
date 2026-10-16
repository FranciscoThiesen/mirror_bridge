#include "python/mirror_bridge_python.hpp"
#include "annotated.hpp"

MIRROR_BRIDGE_MODULE(annotated,
    mirror_bridge::bind_class<UserProfile>(m, "UserProfile");
    mirror_bridge::bind_class<PlainProfile>(m, "PlainProfile");
)
