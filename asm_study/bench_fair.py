#!/usr/bin/env python3
"""
Fair binding-layer benchmark: pybind11 vs mirror_bridge on IDENTICAL C++.

Both pc_pybind.so and pc_mb.so wrap the same demo::PointCloud class from
geometry_min.hpp, compiled with the same clang-p2996, same -O3 -stdlib=libc++,
same libraries. The only axis of variation is the binding layer.

Operations measured:
  1. get_center() on a 1M-point cloud  — hot-path method call overhead
  2. PointCloud(list_of_1M_points)     — container-in construction cost
  3. size()                             — trivial getter (lower bound on dispatch)

All timings: warmup once, then median of 5 (or 3 for construction).
"""

import os
import random
import statistics
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import pc_mb
import pc_pybind

N_POINTS = 1_000_000
GET_CENTER_RUNS = 5
SIZE_RUNS = 5
CTOR_RUNS = 3

# Keep a single deterministic point list that both bindings will consume.
# Each Eigen::Vector3d is represented as a Python list [x, y, z]; both
# pybind11's py::stl+eigen converters and mirror_bridge's Eigen converter
# accept a list of length-3 sequences.
random.seed(0xC0FFEE)
PTS = [[random.random(), random.random(), random.random()] for _ in range(N_POINTS)]


def median_ms(fn, runs):
    # One untimed warmup to page in code/caches.
    fn()
    times = []
    for _ in range(runs):
        t0 = time.perf_counter()
        fn()
        t1 = time.perf_counter()
        times.append((t1 - t0) * 1000.0)
    return statistics.median(times)


def bench_get_center(module):
    pc = module.PointCloud(PTS)

    def call():
        pc.get_center()

    return median_ms(call, GET_CENTER_RUNS)


def bench_size(module):
    pc = module.PointCloud(PTS)

    def call():
        pc.size()

    return median_ms(call, SIZE_RUNS)


def bench_ctor(module):
    def call():
        module.PointCloud(PTS)

    return median_ms(call, CTOR_RUNS)


def main():
    # Sanity: both modules produce identical results.
    mb_pc = pc_mb.PointCloud(PTS)
    py_pc = pc_pybind.PointCloud(PTS)
    assert mb_pc.size() == py_pc.size() == N_POINTS
    mb_c = list(mb_pc.get_center())
    py_c = list(py_pc.get_center())
    assert all(abs(a - b) < 1e-9 for a, b in zip(mb_c, py_c)), (mb_c, py_c)

    rows = []
    for label, fn in [
        ("get_center (1M pts)", bench_get_center),
        ("ctor (list of 1M pts)", bench_ctor),
        ("size()", bench_size),
    ]:
        py_ms = fn(pc_pybind)
        mb_ms = fn(pc_mb)
        ratio = py_ms / mb_ms if mb_ms > 0 else float("inf")
        rows.append((label, py_ms, mb_ms, ratio))

    print()
    print(f"{'op':<26} | {'pybind11 (ms)':>14} | {'mirror_bridge (ms)':>19} | {'ratio':>7}")
    print("-" * 78)
    for label, py_ms, mb_ms, ratio in rows:
        # size() can be sub-microsecond — print enough precision for both scales.
        print(f"{label:<26} | {py_ms:>14.6f} | {mb_ms:>19.6f} | {ratio:>7.2f}x")
    print()


if __name__ == "__main__":
    main()
