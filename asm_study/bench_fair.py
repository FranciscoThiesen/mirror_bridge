#!/usr/bin/env python3
"""
Fair binding-layer benchmark: pybind11 vs mirror_bridge on IDENTICAL C++.

Both pc_pybind.so and pc_mb.so wrap the same demo::PointCloud class from
geometry_min.hpp, compiled with the same clang-p2996, same -O3 -stdlib=libc++,
same libraries. The only axis of variation is the binding layer.

Two tables are printed:

  1. Headline study: constructor scaling.
     Time to convert `list of [x, y, z]` -> `std::vector<Eigen::Vector3d>`
     at several N. This is the ingest primitive most Python data pipelines
     hit (loading a point cloud, parsing dicts, etc.), and it's where
     mirror_bridge's reflection-specialised container converter
     dramatically beats pybind11's generic per-element type_caster.

  2. Context operations at N = 1_000_000.
     A compute-heavy method call (`get_center`) and a trivial getter
     (`size`) to show where the binding layer does and doesn't matter.

All timings: one untimed warmup call, then median of runs below.
"""

from __future__ import annotations

import os
import random
import statistics
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import pc_mb        # type: ignore  # built by build_fair.sh
import pc_pybind    # type: ignore  # built by build_fair.sh

# Scaling sizes for the constructor study.
CTOR_SIZES   = [10_000, 100_000, 1_000_000]
CTOR_RUNS    = 3
CALL_N       = 1_000_000
CALL_RUNS    = 5


def fresh_points(n: int):
    random.seed(0xC0FFEE)
    return [[random.random(), random.random(), random.random()] for _ in range(n)]


def median_ms(fn, runs: int) -> float:
    fn()  # warmup
    samples = []
    for _ in range(runs):
        t0 = time.perf_counter()
        fn()
        t1 = time.perf_counter()
        samples.append((t1 - t0) * 1000.0)
    return statistics.median(samples)


def bench_ctor_scaling():
    print()
    print("1. Constructor scaling: list of [x, y, z]  ->  std::vector<Eigen::Vector3d>")
    print(f"   {'N':>10} | {'pybind11 (ms)':>16} | {'mirror_bridge (ms)':>21} | {'ratio':>7}")
    print("   " + "-" * 66)
    for n in CTOR_SIZES:
        pts = fresh_points(n)
        py_ms = median_ms(lambda: pc_pybind.PointCloud(pts), CTOR_RUNS)
        mb_ms = median_ms(lambda: pc_mb.PointCloud(pts),     CTOR_RUNS)
        ratio = py_ms / mb_ms if mb_ms > 0 else float("inf")
        print(f"   {n:>10,} | {py_ms:>16.3f} | {mb_ms:>21.3f} | {ratio:>6.1f}x")


def bench_call_context():
    pts = fresh_points(CALL_N)
    py_pc = pc_pybind.PointCloud(pts)
    mb_pc = pc_mb.PointCloud(pts)

    # Correctness check: both bindings see the same object.
    assert py_pc.size() == mb_pc.size() == CALL_N
    a = list(py_pc.get_center()); b = list(mb_pc.get_center())
    assert all(abs(x - y) < 1e-9 for x, y in zip(a, b)), (a, b)

    print()
    print(f"2. Context ops at N = {CALL_N:,}:")
    print(f"   {'op':<26} | {'pybind11 (ms)':>14} | {'mirror_bridge (ms)':>19} | {'ratio':>7}")
    print("   " + "-" * 74)

    py_ms = median_ms(lambda: py_pc.get_center(), CALL_RUNS)
    mb_ms = median_ms(lambda: mb_pc.get_center(), CALL_RUNS)
    ratio = py_ms / mb_ms if mb_ms > 0 else float("inf")
    print(f"   {'get_center (compute-heavy)':<26} | {py_ms:>14.4f} | {mb_ms:>19.4f} | {ratio:>6.2f}x")

    # size() is sub-microsecond on mirror_bridge, so run many loops per sample
    # to pull the signal out of the clock resolution.
    INNER = 100_000
    def py_size_loop():
        for _ in range(INNER):
            py_pc.size()
    def mb_size_loop():
        for _ in range(INNER):
            mb_pc.size()
    py_ms = median_ms(py_size_loop, CALL_RUNS) / INNER * 1000  # per-call microseconds
    mb_ms = median_ms(mb_size_loop, CALL_RUNS) / INNER * 1000
    ratio = py_ms / mb_ms if mb_ms > 0 else float("inf")
    print(f"   {'size() (pure dispatch)':<26} | {py_ms:>12.4f} µs | {mb_ms:>17.4f} µs | {ratio:>6.2f}x")


def main():
    bench_ctor_scaling()
    bench_call_context()
    print()


if __name__ == "__main__":
    main()
