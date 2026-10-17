#include "python/mirror_bridge_python.hpp"
#include "sequence_views.hpp"

MIRROR_BRIDGE_MODULE(sequence_views,
    mirror_bridge::bind_class<seqv::Point>(m, "Point");
    mirror_bridge::bind_class<seqv::Bag>(m, "Bag");
)
