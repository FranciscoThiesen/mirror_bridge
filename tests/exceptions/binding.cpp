#include "mirror_bridge.hpp"
#include "src/lib.hpp"
using namespace api;
MIRROR_BRIDGE_MODULE(exceptions_test,
    mirror_bridge::bind_class<Container>(m, "Container");
)
