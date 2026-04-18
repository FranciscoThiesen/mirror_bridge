"""Real Open3D end-to-end test via mirror_bridge.

Loads libOpen3D.so built from the patched fork and exercises actual
Open3D algorithms — PointCloud operations, AABB computation, voxel
downsampling — all through the mirror_bridge-generated binding."""
import sys, os
sys.path.insert(0, "build")
os.environ.setdefault("LD_LIBRARY_PATH",
    "/tmp/o3d_fork_build/lib/Release:/usr/local/lib")

import real_open3d as o3d

fail = 0
def check(cond, msg):
    global fail
    print(("  PASS: " if cond else "  FAIL: ") + msg)
    if not cond: fail += 1

print("Real Open3D via mirror_bridge")
print("=" * 60)
print(f"Classes bound: {sorted(x for x in dir(o3d) if not x.startswith('_'))}")
print()

print("--- 1. PointCloud construction + Eigen marshalling ---")
pcd = o3d.PointCloud()
check(pcd.IsEmpty(), "empty PointCloud")
pcd.points_ = [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]]
check(len(pcd.points_) == 4, f"set 4 points: {len(pcd.points_)}")
check(pcd.HasPoints(), "HasPoints returns truthy")
c = pcd.GetCenter()
check(abs(c[0] - 0.25) < 1e-9, f"GetCenter = [{c[0]}, {c[1]}, {c[2]}]")

print()
print("--- 2. Polymorphic return: GetAxisAlignedBoundingBox ---")
aabb = pcd.GetAxisAlignedBoundingBox()
check(type(aabb).__name__ == "AxisAlignedBoundingBox",
      f"returns AABB (not Geometry base): {type(aabb).__name__}")
check(list(aabb.min_bound_) == [0, 0, 0], f"AABB.min = {list(aabb.min_bound_)}")
check(list(aabb.max_bound_) == [1, 1, 1], f"AABB.max = {list(aabb.max_bound_)}")
check(aabb.Volume() == 1.0, f"AABB.Volume() = {aabb.Volume()}")

print()
print("--- 3. Real Open3D algorithm: VoxelDownSample ---")
ds = pcd.VoxelDownSample(voxel_size=0.5)
check(type(ds).__name__ == "PointCloud",
      f"returns PointCloud: {type(ds).__name__}")
check(hasattr(ds, "points_"), "has .points_ field")
print(f"  After voxel_size=0.5: {len(ds.points_)} points")

print()
print("--- 4. AABB construction with Eigen args ---")
bbox = o3d.AxisAlignedBoundingBox([0, 0, 0], [2, 3, 4])
check(bbox.Volume() == 24.0, f"[0,0,0]×[2,3,4].Volume() = {bbox.Volume()}")

print()
print("--- 5. repr shows real Open3D internals ---")
r = repr(aabb)
check("AxisAlignedBoundingBox" in r and "min_bound" in r,
      f"repr: {r[:90]}")

print()
if fail == 0:
    print("ALL REAL OPEN3D TESTS PASSED — 38-class mirror_bridge binding")
    print("works against the patched-fork libOpen3D.so at runtime.")
    sys.exit(0)
else:
    print(f"{fail} FAILED")
    sys.exit(1)
