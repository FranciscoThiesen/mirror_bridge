#include "mirror_bridge.hpp"
#include "bulk_class.hpp"

MIRROR_BRIDGE_MODULE(bulk_test,
    mirror_bridge::bind_class<DataGenerator>(m, "DataGenerator");
)
