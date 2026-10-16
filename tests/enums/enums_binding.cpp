#include "python/mirror_bridge_python.hpp"
#include "enums.hpp"

MIRROR_BRIDGE_MODULE(enums_test,
    mirror_bridge::bind_enum<en::Color>(m, "Color");
    mirror_bridge::bind_enum<en::Signal>(m, "Signal");
    mirror_bridge::bind_enum<en::Plain>(m, "Plain");
    mirror_bridge::bind_enum<en::Permission>(m, "Permission");
    mirror_bridge::bind_enum<en::Awkward>(m, "Awkward");
    mirror_bridge::bind_enum<en::Outer::Kind>(m, "Kind");
    mirror_bridge::bind_class<en::Outer>(m, "Outer");
    mirror_bridge::bind_class<en::Light>(m, "Light");
    mirror_bridge::bind_function<&en::name_of>(m, "name_of");
    mirror_bridge::bind_function<&en::brighter>(m, "brighter");
    mirror_bridge::bind_function<&en::permission_bits>(m, "permission_bits");
)
