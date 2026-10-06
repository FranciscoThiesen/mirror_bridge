#include "python/mirror_bridge_python.hpp"
#include "member_views.hpp"

MIRROR_BRIDGE_MODULE(member_views,
    mirror_bridge::bind_class<views::Leaf>(m, "Leaf");
    mirror_bridge::bind_class<views::Middle>(m, "Middle");
    mirror_bridge::bind_class<views::Base>(m, "Base");
    mirror_bridge::bind_class<views::Owner>(m, "Owner");
)
