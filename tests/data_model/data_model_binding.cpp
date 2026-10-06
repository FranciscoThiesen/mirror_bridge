#include "python/mirror_bridge_python.hpp"
#include "data_model.hpp"

MIRROR_BRIDGE_MODULE(data_model,
    mirror_bridge::bind_class<Bag>(m, "Bag");
    mirror_bridge::bind_class<Shelf>(m, "Shelf");
    mirror_bridge::bind_class<Doc>(m, "Doc");
    mirror_bridge::bind_class<Gauge>(m, "Gauge");
    mirror_bridge::bind_class<Frozen>(m, "Frozen");
    mirror_bridge::bind_class<Stream::Cursor>(m, "Cursor");
    mirror_bridge::bind_class<Stream>(m, "Stream");
    mirror_bridge::bind_class<Tracked>(m, "Tracked");
)
