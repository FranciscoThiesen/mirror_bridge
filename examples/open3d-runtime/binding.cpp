// REAL Open3D binding — links against libOpen3D.so built from the patched
// fork (commit mirror_bridge-fixes). This is the runtime proof that every
// mirror_bridge feature works against production Open3D code.
#include "mirror_bridge.hpp"
#include "open3d/geometry/BoundingVolume.h"
#include "open3d/geometry/PointCloud.h"
#include "open3d/geometry/TriangleMesh.h"
#include "open3d/geometry/LineSet.h"
#include "open3d/geometry/VoxelGrid.h"
#include "open3d/geometry/Octree.h"

using namespace open3d::geometry;

MIRROR_BRIDGE_MODULE(real_open3d,
    mirror_bridge::bind_class<AxisAlignedBoundingBox>(m, "AxisAlignedBoundingBox");
    mirror_bridge::bind_class<OrientedBoundingBox>(m, "OrientedBoundingBox");
    mirror_bridge::bind_class<PointCloud>(m, "PointCloud");
    mirror_bridge::bind_class<TriangleMesh>(m, "TriangleMesh");
    mirror_bridge::bind_class<LineSet>(m, "LineSet");
    mirror_bridge::bind_class<VoxelGrid>(m, "VoxelGrid");
)
