#!/usr/bin/env python3
"""
Benchmark comparing Python vs C++ (via Mirror Bridge).

This demonstrates two key points:
1. Simple method calls have limited speedup due to cross-language overhead
2. Moving entire hot loops to C++ gives dramatic speedups

Usage (inside Docker container):
    cd /workspace/examples/blog_vec3
    ../../mirror_bridge_auto . --module vec3 -o .
    python3 benchmark.py
"""

import time
import sys
import math

try:
    import vec3
except ImportError:
    print("ERROR: vec3 module not found!")
    print("Build it first:")
    print("  ../../mirror_bridge_auto . --module vec3 -o .")
    sys.exit(1)


# Pure Python implementation for comparison
class PyVec3:
    def __init__(self, x, y, z):
        self.x, self.y, self.z = x, y, z

    def dot(self, other):
        return self.x * other.x + self.y * other.y + self.z * other.z

    def length(self):
        return math.sqrt(self.x**2 + self.y**2 + self.z**2)


def py_hot_loop(n):
    """Python version of the hot loop."""
    direction = PyVec3(1, 1, 1)
    dir_len = direction.length()
    total = 0.0
    for i in range(n):
        v = PyVec3(i * 0.1, i * 0.2, i * 0.3)
        total += v.dot(direction) / dir_len
    return total


def main():
    n = 1_000_000

    print("=" * 60)
    print("Mirror Bridge Benchmark")
    print("=" * 60)

    # =========================================================
    # Benchmark 1: Simple method calls
    # =========================================================
    print("\n--- Benchmark 1: Simple dot() calls (1M iterations) ---")
    print("Shows cross-language overhead on trivial operations\n")

    py_a, py_b = PyVec3(1, 2, 3), PyVec3(4, 5, 6)
    cpp_a, cpp_b = vec3.Vec3(1, 2, 3), vec3.Vec3(4, 5, 6)

    # Warmup
    for _ in range(1000):
        py_a.dot(py_b)
        cpp_a.dot(cpp_b)

    start = time.perf_counter()
    for _ in range(n):
        py_a.dot(py_b)
    py_time = time.perf_counter() - start

    start = time.perf_counter()
    for _ in range(n):
        cpp_a.dot(cpp_b)
    cpp_time = time.perf_counter() - start

    print(f"Python: {py_time:.2f}s")
    print(f"C++:    {cpp_time:.2f}s")
    print(f"Speedup: {py_time/cpp_time:.1f}x")

    # =========================================================
    # Benchmark 2: Hot loop moved to C++
    # =========================================================
    print("\n--- Benchmark 2: hot_loop() - entire loop in C++ ---")
    print("This is the real benchmark!\n")

    # Warmup
    py_hot_loop(1000)
    vec3.Vec3.hot_loop(1000)

    start = time.perf_counter()
    py_result = py_hot_loop(n)
    py_time = time.perf_counter() - start

    start = time.perf_counter()
    cpp_result = vec3.Vec3.hot_loop(n)
    cpp_time = time.perf_counter() - start

    print(f"Python: {py_time:.2f}s")
    print(f"C++:    {cpp_time:.3f}s")
    print(f"Speedup: {py_time/cpp_time:.0f}x")
    print(f"\nResults match: {abs(py_result - cpp_result) < 0.01}")

    # =========================================================
    # Summary
    # =========================================================
    print("\n" + "=" * 60)
    print("KEY INSIGHT")
    print("=" * 60)
    print("""
Don't call C++ methods millions of times from Python.
Instead, move your hot loops entirely to C++.

- 1M calls to C++ from Python: limited by call overhead
- 1 call that does 1M operations in C++: full native speed
""")


if __name__ == "__main__":
    main()
