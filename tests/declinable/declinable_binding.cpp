#include "python/mirror_bridge_python.hpp"
#include "declinable.hpp"

MIRROR_BRIDGE_MODULE(declinable,
    mirror_bridge::bind_class_when_bindable<decl::Plain>(m, "Plain");
    mirror_bridge::bind_class_when_bindable<decl::Undestroyable>(m, "Undestroyable");
    mirror_bridge::bind_class_when_bindable<decl::Consumer>(m, "Consumer");
)
