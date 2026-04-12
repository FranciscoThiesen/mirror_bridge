#include "javascript/mirror_bridge_javascript.hpp"
#include "exception_class.hpp"

MIRROR_BRIDGE_JS_MODULE(exception_test,
    mirror_bridge::javascript::bind_class<ThrowingService>(env, exports, "ThrowingService");
)
