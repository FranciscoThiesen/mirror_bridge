"""Performance comparison: mirror_bridge vs pybind11.

Both modules bind the same geometry classes (PointCloud, LineSet,
AxisAlignedBoundingBox, TriangleMesh). This script measures the overhead
of the binding layer itself on common operations.
"""

import sys
import os
import time
import statistics

# Add build dirs to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'build'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'pybind11_equivalent/build'))

import open3d_geometry as mb
import open3d_geometry_pybind as pb

def bench(name, fn, iters=100000):
    """Run fn() iters times, return ns/op (median of 5 runs)."""
    times = []
    for _ in range(5):
        t0 = time.perf_counter_ns()
        for _ in range(iters):
            fn()
        t1 = time.perf_counter_ns()
        times.append((t1 - t0) / iters)
    return statistics.median(times)

def main():
    print("=" * 70)
    print("Binding overhead benchmark: mirror_bridge vs pybind11")
    print("=" * 70)
    print()
    print(f"{'Operation':<42} {'pybind11':>10} {'mirror_br':>10} {'speedup':>8}")
    print("-" * 72)

    # -- 1. Object construction --
    def mb_construct(): return mb.PointCloud()
    def pb_construct(): return pb.PointCloud()
    m_ns = bench("construct", mb_construct)
    p_ns = bench("construct", pb_construct)
    print(f"{'PointCloud()  (empty construction)':<42} {p_ns:>7.0f} ns {m_ns:>7.0f} ns {p_ns/m_ns:>6.2f}x")

    # -- 2. Method call (no args, returns bool) --
    pc_mb = mb.PointCloud()
    pc_pb = pb.PointCloud()
    m_ns = bench("has_points", lambda: pc_mb.has_points())
    p_ns = bench("has_points", lambda: pc_pb.has_points())
    print(f"{'.has_points()  (bool method)':<42} {p_ns:>7.0f} ns {m_ns:>7.0f} ns {p_ns/m_ns:>6.2f}x")

    # -- 3. Method call (returns Eigen::Vector3d) --
    pc_mb.points = [[i, i, i] for i in range(100)]
    pc_pb.points = [[i, i, i] for i in range(100)]
    m_ns = bench("get_center", lambda: pc_mb.get_center())
    p_ns = bench("get_center", lambda: pc_pb.get_center())
    print(f"{'.get_center()  (100 pts, Vector3d out)':<42} {p_ns:>7.0f} ns {m_ns:>7.0f} ns {p_ns/m_ns:>6.2f}x")

    # -- 4. Attribute get (scalar) --
    bbox_mb = mb.AxisAlignedBoundingBox([0,0,0], [1,1,1])
    bbox_pb = pb.AxisAlignedBoundingBox([0,0,0], [1,1,1])
    m_ns = bench("bbox.min_bound", lambda: bbox_mb.min_bound)
    p_ns = bench("bbox.min_bound", lambda: bbox_pb.min_bound)
    print(f"{'.min_bound     (attribute get, Vector3d)':<42} {p_ns:>7.0f} ns {m_ns:>7.0f} ns {p_ns/m_ns:>6.2f}x")

    # -- 5. Attribute set (Eigen::Vector3d) --
    vec = [1.5, 2.5, 3.5]
    def mb_set():
        bbox_mb.min_bound = vec
    def pb_set():
        bbox_pb.min_bound = vec
    m_ns = bench("bbox.min_bound =", mb_set)
    p_ns = bench("bbox.min_bound =", pb_set)
    print(f"{'.min_bound =   (attribute set, Vector3d)':<42} {p_ns:>7.0f} ns {m_ns:>7.0f} ns {p_ns/m_ns:>6.2f}x")

    # -- 6. Method with Eigen input (translate) --
    pc_mb2 = mb.PointCloud(); pc_mb2.points = [[i,i,i] for i in range(10)]
    pc_pb2 = pb.PointCloud(); pc_pb2.points = [[i,i,i] for i in range(10)]
    m_ns = bench("translate", lambda: pc_mb2.translate([0.1, 0.2, 0.3]))
    p_ns = bench("translate", lambda: pc_pb2.translate([0.1, 0.2, 0.3]))
    print(f"{'.translate(v)  (Vector3d arg, mutate)':<42} {p_ns:>7.0f} ns {m_ns:>7.0f} ns {p_ns/m_ns:>6.2f}x")

    # -- 7. Method with multiple args --
    m_ns = bench("scale", lambda: pc_mb2.scale(1.01, [0, 0, 0]))
    p_ns = bench("scale", lambda: pc_pb2.scale(1.01, [0, 0, 0]))
    print(f"{'.scale(s, c)   (double + Vector3d args)':<42} {p_ns:>7.0f} ns {m_ns:>7.0f} ns {p_ns/m_ns:>6.2f}x")

    # -- 8. Container assignment (vector<Vector3d>) --
    big_points = [[i, i, i] for i in range(1000)]
    def mb_setpts():
        pc_mb.points = big_points
    def pb_setpts():
        pc_pb.points = big_points
    m_ns = bench("points =", mb_setpts, iters=1000)
    p_ns = bench("points =", pb_setpts, iters=1000)
    print(f"{'.points = [1000 Vector3d]  (bulk assign)':<42} {p_ns:>7.0f} ns {m_ns:>7.0f} ns {p_ns/m_ns:>6.2f}x")

    print()
    print("Lower is better. Times are ns/call (median of 5 runs, 100k iters each).")

if __name__ == "__main__":
    main()
