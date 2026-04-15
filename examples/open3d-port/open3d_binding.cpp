// mirror_bridge binding for Open3D geometry classes.
//
// This is the ENTIRE binding layer for 4 geometry classes.
// Compare with Open3D's pybind11 bindings:
//   - pointcloud.cpp:      510 lines
//   - lineset.cpp:          155 lines
//   - boundingvolume.cpp:   240 lines
//   - trianglemesh.cpp:    1010 lines
//   Total pybind11:        1915 lines
//
// mirror_bridge version:    8 lines (below)

#include "src/eigen_converter.hpp"
#include "mirror_bridge.hpp"
#include "src/geometry.hpp"

using namespace open3d::geometry;

MIRROR_BRIDGE_MODULE(open3d_geometry,
    mirror_bridge::bind_class<AxisAlignedBoundingBox>(m, "AxisAlignedBoundingBox");
    mirror_bridge::bind_class<PointCloud>(m, "PointCloud");
    mirror_bridge::bind_class<LineSet>(m, "LineSet");
    mirror_bridge::bind_class<TriangleMesh>(m, "TriangleMesh");
)
