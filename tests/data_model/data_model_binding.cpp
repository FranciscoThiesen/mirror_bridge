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
    mirror_bridge::bind_class<Countdown::Cursor>(m, "CountdownCursor");
    mirror_bridge::bind_class<Countdown>(m, "Countdown");
    mirror_bridge::bind_class<Flags>(m, "Flags");
    mirror_bridge::bind_class<Secretive>(m, "Secretive");
    mirror_bridge::bind_class<WithBase>(m, "WithBase");
    mirror_bridge::bind_class<Derived>(m, "Derived");
    mirror_bridge::bind_class<Underflowed>(m, "Underflowed");
    mirror_bridge::bind_class<Sneaky>(m, "Sneaky");
    mirror_bridge::bind_class<Point>(m, "Point");
)
