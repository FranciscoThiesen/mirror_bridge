// mirror_bridge binding for Open3D geometry classes.
//
// This is the ENTIRE binding layer for 4 geometry classes.
// Compare with Open3D's pybind11 bindings:
//   - pointcloud.cpp:      428 lines
//   - lineset.cpp:          135 lines
//   - boundingvolume.cpp:   217 lines
//   - trianglemesh.cpp:     798 lines
//   Total pybind11:        1,578 lines
//
// mirror_bridge version: the 4 bind_class<> calls below.
// Eigen types are handled automatically — no custom converter needed.

#include "mirror_bridge.hpp"
#include "src/geometry.hpp"

using namespace open3d::geometry;

MIRROR_BRIDGE_MODULE(open3d_geometry,
    mirror_bridge::bind_class<AxisAlignedBoundingBox>(m, "AxisAlignedBoundingBox");
    mirror_bridge::bind_class<PointCloud>(m, "PointCloud");
    mirror_bridge::bind_class<LineSet>(m, "LineSet");
    mirror_bridge::bind_class<TriangleMesh>(m, "TriangleMesh");
)
