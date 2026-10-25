#include "javascript/mirror_bridge_javascript.hpp"
#include "const_members.hpp"

// See the Lua sibling: a const member and a member whose type has no default
// constructor each made the whole module uncompilable, and JavaScript had the
// same two unguarded `new T()` calls in its constructor path besides.
MIRROR_BRIDGE_JS_MODULE(const_members_js,
    mirror_bridge::javascript::bind_class<Sensor>(env, exports, "Sensor");
    mirror_bridge::javascript::bind_class<Holder>(env, exports, "Holder");
)
