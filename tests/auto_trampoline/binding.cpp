// Auto-trampoline: NO hand-written trampoline class. Reflection + vtable
// swap handles Python overrides of every virtual automatically.
#include "mirror_bridge.hpp"
#include "mirror_bridge_auto_trampoline.hpp"
#include "src/shapes.hpp"

using namespace shapes;

MIRROR_BRIDGE_MODULE(auto_trampoline_test,
    mirror_bridge::bind_class_auto<Animal>(m, "Animal");
    mirror_bridge::bind_class<Zoo>(m, "Zoo");
)
