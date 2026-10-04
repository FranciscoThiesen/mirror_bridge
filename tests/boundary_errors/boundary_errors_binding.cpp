#include "python/mirror_bridge_python.hpp"
#include "boundary_errors.hpp"

MIRROR_BRIDGE_MODULE(boundary_errors,
    mirror_bridge::bind_class<boundary::Thrower>(m, "Thrower");
    mirror_bridge::bind_class<boundary::Point>(m, "Point");
    mirror_bridge::bind_class<boundary::DefaultOnly>(m, "DefaultOnly");
    mirror_bridge::bind_class<boundary::Signatures>(m, "Signatures");
    mirror_bridge::bind_function<&boundary::free_thrower>(m, "free_thrower");
    mirror_bridge::bind_function<&boundary::free_ok>(m, "free_ok");
)
