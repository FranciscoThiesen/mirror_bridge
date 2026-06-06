#include "javascript/mirror_bridge_javascript.hpp"
#include "smart_ptr_class.hpp"

MIRROR_BRIDGE_JS_MODULE(smart_ptr_test,
    mirror_bridge::javascript::bind_class<Data>(env, m, "Data");
    mirror_bridge::javascript::bind_class<ResourceMgr>(env, m, "ResourceMgr");
    mirror_bridge::javascript::bind_class<ShapeHolder>(env, m, "ShapeHolder");
)
