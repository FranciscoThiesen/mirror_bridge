"""Three-way benchmark:

  A. Pure numpy (C++ via BLAS/ufuncs, hand-tuned)
  B. Open3D 0.18 shipped pybind11 binding (real Open3D C++ + Intel's
     production-quality hand-written .def() layer)
  C. mirror_bridge + our Open3D-shape C++ (same algorithms, naive loop,
     reflection-generated Python binding)

For centroid / AABB / voxel downsample on 1M random 3-D points.

mirror_bridge is built against our comprehensive example's C++, NOT
against libOpen3D.so — we're measuring binding overhead at the surface
level. Open3D's pybind11 layer dispatches into their own production
C++ (with OpenMP, tuned memory layouts, etc.), so if we come close
that's already a meaningful result, and if the gap widens at higher
point counts the cause is the underlying algorithm — not the
binding."""
import sys
import time
import statistics
sys.path.insert(0, "build")

import numpy as np
import open3d as ext_open3d                    # pip'd Open3D 0.18
import open3d_comprehensive as mb_o3d          # mirror_bridge binding

N = 1_000_000
rng = np.random.default_rng(42)
points_np = rng.standard_normal((N, 3)).astype(np.float64)
points_list = points_np.tolist()

print(f"Fixture: {N:,} random 3-D points (numpy array: {points_np.nbytes // 1024} KB)")
print()

# -----------------------------------------------------------------------
# Load into each backend ONCE (FFI cost is a fixed one-time overhead).
# -----------------------------------------------------------------------
mb_pcd = mb_o3d.PointCloud(points_list)

ext_pcd = ext_open3d.geometry.PointCloud()
ext_pcd.points = ext_open3d.utility.Vector3dVector(points_np)

# -----------------------------------------------------------------------
# Timing helper
# -----------------------------------------------------------------------
def bench(fn, iters=5, warm=1):
    for _ in range(warm):
        fn()
    samples = [0.0] * iters
    for i in range(iters):
        t = time.perf_counter()
        fn()
        samples[i] = time.perf_counter() - t
    return statistics.median(samples)

# -----------------------------------------------------------------------
# Operation 1: Centroid
# -----------------------------------------------------------------------
np_center = lambda: points_np.mean(axis=0)
ext_center = lambda: ext_pcd.get_center()
mb_center = lambda: mb_pcd.get_center()

t_np   = bench(np_center)
t_ext  = bench(ext_center)
t_mb   = bench(mb_center)

# -----------------------------------------------------------------------
# Operation 2: AABB (min + max)
# -----------------------------------------------------------------------
np_aabb  = lambda: (points_np.min(axis=0), points_np.max(axis=0))
ext_aabb = lambda: ext_pcd.get_axis_aligned_bounding_box()
mb_aabb  = lambda: mb_pcd.get_axis_aligned_bounding_box()

t_np_a  = bench(np_aabb)
t_ext_a = bench(ext_aabb)
t_mb_a  = bench(mb_aabb)

# -----------------------------------------------------------------------
# Operation 3: Voxel downsample
# -----------------------------------------------------------------------
VOX = 0.1
def np_voxel():
    cells = np.floor(points_np / VOX).astype(np.int64)
    h = (cells[:, 0] * 73856093) ^ (cells[:, 1] * 19349663) ^ (cells[:, 2] * 83492791)
    _, idx = np.unique(h, return_index=True)
    return points_np[idx]

ext_voxel = lambda: ext_pcd.voxel_down_sample(VOX)
mb_voxel  = lambda: mb_pcd.voxel_down_sample(VOX)

t_np_v  = bench(np_voxel, iters=3)
t_ext_v = bench(ext_voxel, iters=3)
t_mb_v  = bench(mb_voxel, iters=3)

# -----------------------------------------------------------------------
# Report
# -----------------------------------------------------------------------
print("="*78)
print(f"{'Operation':<30} {'numpy':>10} {'Open3D (pybind11)':>20} {'mirror_bridge':>14}")
print("="*78)

def row(label, tn, te, tm):
    print(f"{label:<30} {tn*1000:>7.2f} ms {te*1000:>17.2f} ms {tm*1000:>11.2f} ms")

row("Centroid",          t_np,  t_ext,  t_mb)
row("AABB (min + max)",  t_np_a, t_ext_a, t_mb_a)
row("Voxel downsample",  t_np_v, t_ext_v, t_mb_v)
print()
print("Relative to Open3D pybind11 (smaller is better for mirror_bridge):")
print(f"  Centroid:         {t_mb/t_ext:.2f}× Open3D   ({t_ext/t_mb:.2f}× the other way)")
print(f"  AABB:             {t_mb_a/t_ext_a:.2f}× Open3D   ({t_ext_a/t_mb_a:.2f}× the other way)")
print(f"  Voxel downsample: {t_mb_v/t_ext_v:.2f}× Open3D   ({t_ext_v/t_mb_v:.2f}× the other way)")
