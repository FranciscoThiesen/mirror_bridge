#include "python/mirror_bridge_python.hpp"
#include "c_api_shapes.hpp"

MIRROR_BRIDGE_MODULE(c_api_shapes,
    mirror_bridge::bind_class<capi::Glyph>(m, "Glyph");
    mirror_bridge::bind_class<capi::StoragePair>(m, "StoragePair");
    mirror_bridge::bind_class_when_bindable<capi::HoldsOpaque>(m, "HoldsOpaque");
)
