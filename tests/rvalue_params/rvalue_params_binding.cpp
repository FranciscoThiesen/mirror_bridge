#include "python/mirror_bridge_python.hpp"
#include "rvalue_params.hpp"

MIRROR_BRIDGE_MODULE(rvalue_params,
    mirror_bridge::bind_function<&rv::consume_bool>(m, "consume_bool");
    mirror_bridge::bind_function<&rv::consume_string>(m, "consume_string");
    mirror_bridge::bind_function<&rv::consume_vector>(m, "consume_vector");
    mirror_bridge::bind_function<&rv::mixed>(m, "mixed");
    mirror_bridge::bind_class<rv::Counter>(m, "Counter");
)
