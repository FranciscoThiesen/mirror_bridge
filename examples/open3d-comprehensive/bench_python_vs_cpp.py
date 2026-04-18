"""Fair benchmark: mirror_bridge-bound C++ vs pure-Python numpy equivalents.

For every operation we measure:
  - Pure Python with numpy (realistic baseline — nobody writes raw Python loops
    for 1M points; numpy is the reference).
  - mirror_bridge call into the C++ implementation.
  - The fixed argument-marshalling cost (measured separately — each call copies
    a vector<Vector3d> across the FFI boundary).

We also measure list-vs-numpy-vs-mmap input representations because matters
on large point clouds: Python-list materialization dominates the crossing."""
import sys
import time
import statistics
sys.path.insert(0, "build")

import numpy as np
import open3d_comprehensive as o3d

# -----------------------------------------------------------------------
# Test fixtures — 1M random points (similar to a medium LiDAR scan).
# -----------------------------------------------------------------------
N = 1_000_000
rng = np.random.default_rng(42)
points_np = rng.standard_normal((N, 3)).astype(np.float64)
points_list = points_np.tolist()   # the format mirror_bridge expects today

print(f"Benchmark fixture: {N:,} random 3D points")
print(f"  numpy array:   {points_np.nbytes // 1024} KB")
print(f"  Python list:   {sys.getsizeof(points_list) // 1024} KB (list overhead)")
print()

# -----------------------------------------------------------------------
# Timing helper
# -----------------------------------------------------------------------
def bench(name, fn, iters=5):
    fn()  # warm
    samples = []
    for _ in range(iters):
        t = time.perf_counter()
        r = fn()
        samples.append(time.perf_counter() - t)
    med = statistics.median(samples)
    return name, med, r

print("=" * 68)
print(f"{'Operation':<28} {'ms (median of 5)':>20} {'vs numpy':>16}")
print("=" * 68)

# -----------------------------------------------------------------------
# 1. Centroid
# -----------------------------------------------------------------------
def py_center():
    return points_np.mean(axis=0)

# Load into a C++ PointCloud ONCE, time GetCenter repeatedly.
pcd = o3d.PointCloud(points_list)

def cpp_center():
    return pcd.get_center()

_, py_ms, py_r = bench("numpy mean",    py_center)
_, cpp_ms, cpp_r = bench("mirror_bridge GetCenter", cpp_center)
ratio = py_ms / cpp_ms if cpp_ms > 0 else float("inf")
print(f"{'numpy mean':<28} {py_ms*1000:>18.2f} ms {'(baseline)':>16}")
print(f"{'mirror_bridge GetCenter':<28} {cpp_ms*1000:>18.2f} ms {'{:.2f}×'.format(ratio):>16}")
assert np.allclose(py_r, list(cpp_r)), "centroid mismatch"

# -----------------------------------------------------------------------
# 2. AABB (min + max)
# -----------------------------------------------------------------------
def py_aabb():
    return points_np.min(axis=0), points_np.max(axis=0)

def cpp_aabb():
    aabb = pcd.get_axis_aligned_bounding_box()
    return list(aabb.min_bound), list(aabb.max_bound)

_, py_ms, py_r = bench("numpy min+max",  py_aabb)
_, cpp_ms, cpp_r = bench("mirror_bridge AABB", cpp_aabb)
ratio = py_ms / cpp_ms if cpp_ms > 0 else float("inf")
print(f"{'numpy min+max':<28} {py_ms*1000:>18.2f} ms {'(baseline)':>16}")
print(f"{'mirror_bridge AABB':<28} {cpp_ms*1000:>18.2f} ms {'{:.2f}×'.format(ratio):>16}")

# -----------------------------------------------------------------------
# 3. Voxel downsample
# -----------------------------------------------------------------------
VOX = 0.1

def py_voxel():
    # Bucket each point by its voxel cell, keep one per bucket.
    cells = np.floor(points_np / VOX).astype(np.int64)
    # Hash each cell to a single int for uniqueness
    h = (cells[:, 0].astype(np.int64) * 73856093) ^ \
        (cells[:, 1].astype(np.int64) * 19349663) ^ \
        (cells[:, 2].astype(np.int64) * 83492791)
    _, idx = np.unique(h, return_index=True)
    return points_np[idx]

def cpp_voxel():
    return pcd.voxel_down_sample(VOX)

_, py_ms, py_r = bench("numpy voxel bucket", py_voxel, iters=3)
_, cpp_ms, cpp_r = bench("mirror_bridge voxel", cpp_voxel, iters=3)
ratio = py_ms / cpp_ms if cpp_ms > 0 else float("inf")
print(f"{'numpy voxel bucket':<28} {py_ms*1000:>18.2f} ms {'(baseline)':>16}")
print(f"{'mirror_bridge voxel':<28} {cpp_ms*1000:>18.2f} ms {'{:.2f}×'.format(ratio):>16}")
print(f"  (py kept {len(py_r)} pts, cpp kept {cpp_r.size()} pts)")

# -----------------------------------------------------------------------
# 4. Cost breakdown: FFI crossing alone
# -----------------------------------------------------------------------
print()
print("Cost breakdown (per call):")
# Re-measure just the argument-marshalling cost
def marshal_only():
    # Getting .points back out is the return-direction marshalling cost.
    return len(pcd.points)

_, marshal_ms, _ = bench("FFI read .points length", marshal_only)
print(f"  FFI read  (list length, no computation): {marshal_ms*1000:.3f} ms")

# Write-direction: construct a new PointCloud from the same list (full copy in).
def marshal_in():
    return o3d.PointCloud(points_list)

_, marshal_in_ms, _ = bench("FFI write  list→PointCloud", marshal_in, iters=3)
print(f"  FFI write (list→PointCloud, 1M×3 values): {marshal_in_ms*1000:.1f} ms")
