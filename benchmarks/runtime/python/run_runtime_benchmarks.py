#!/usr/bin/env python3
"""
Runtime Performance Benchmarks for Python Bindings
Compares Mirror Bridge, pybind11, and Boost.Python
"""

import sys
import time
import statistics

# Test iterations
ITERATIONS = {
    'null_call': 10_000_000,      # 10M iterations for empty call
    'arithmetic': 10_000_000,      # 10M for int/double ops
    'string': 1_000_000,           # 1M for string ops
    'vector': 100_000,             # 100K for vector ops
    'attribute': 10_000_000,       # 10M for attribute access
    'construction': 1_000_000,     # 1M for object creation
}

def benchmark(name, func, iterations):
    """Run benchmark and return average time in nanoseconds"""
    # Warmup
    for _ in range(min(1000, iterations // 10)):
        func()

    # Measure
    times = []
    for _ in range(5):  # 5 runs
        start = time.perf_counter()
        for _ in range(iterations):
            func()
        end = time.perf_counter()
        times.append((end - start) * 1e9 / iterations)  # ns per operation

    return statistics.median(times)

def run_benchmarks(module_name, obj_class):
    """Run all benchmarks on a module"""
    results = {}

    # Create test object
    obj = obj_class()

    # 1. Null call overhead
    results['null_call'] = benchmark(
        'null_call',
        lambda: obj.null_call(),
        ITERATIONS['null_call']
    )

    # 2. Arithmetic - int
    results['arithmetic_int'] = benchmark(
        'arithmetic_int',
        lambda: obj.add_int(42),
        ITERATIONS['arithmetic']
    )

    # 3. Arithmetic - double
    results['arithmetic_double'] = benchmark(
        'arithmetic_double',
        lambda: obj.multiply_double(3.14),
        ITERATIONS['arithmetic']
    )

    # 4. String operations - concatenate
    results['string_concat'] = benchmark(
        'string_concat',
        lambda: obj.concat_string("_test"),
        ITERATIONS['string']
    )

    # 5. String operations - get
    results['string_get'] = benchmark(
        'string_get',
        lambda: obj.get_string(),
        ITERATIONS['string']
    )

    # 6. String operations - set
    results['string_set'] = benchmark(
        'string_set',
        lambda: obj.set_string("benchmark"),
        ITERATIONS['string']
    )

    # 7. Vector operations - append
    try:
        obj.data = []  # Reset (works for pybind11/nanobind/Mirror Bridge)
    except (TypeError, AttributeError):
        # SWIG doesn't support direct assignment, use method instead
        obj.set_vector([])
    results['vector_append'] = benchmark(
        'vector_append',
        lambda: obj.add_to_vector(1.0),
        ITERATIONS['vector']
    )

    # 8. Vector operations - get
    try:
        obj.data = [1.0] * 100  # Works for pybind11/nanobind/Mirror Bridge
    except (TypeError, AttributeError):
        # SWIG doesn't support direct assignment
        obj.set_vector([1.0] * 100)
    results['vector_get'] = benchmark(
        'vector_get',
        lambda: obj.get_vector(),
        ITERATIONS['vector']
    )

    # 9. Vector operations - set
    test_vec = [1.0, 2.0, 3.0, 4.0, 5.0]
    results['vector_set'] = benchmark(
        'vector_set',
        lambda: obj.set_vector(test_vec),
        ITERATIONS['vector']
    )

    # 10. Attribute access - get
    results['attr_get'] = benchmark(
        'attr_get',
        lambda: obj.counter,
        ITERATIONS['attribute']
    )

    # 11. Attribute access - set
    counter = [0]
    def set_attr():
        obj.counter = counter[0]
        counter[0] += 1
    results['attr_set'] = benchmark(
        'attr_set',
        set_attr,
        ITERATIONS['attribute']
    )

    # 12. Object construction
    results['construction'] = benchmark(
        'construction',
        lambda: obj_class(),
        ITERATIONS['construction']
    )

    return results

def format_time(ns):
    """Format time in appropriate unit"""
    if ns < 1000:
        return f"{ns:.1f} ns"
    elif ns < 1_000_000:
        return f"{ns/1000:.1f} µs"
    else:
        return f"{ns/1_000_000:.1f} ms"

def print_results(results_mb, results_pb, results_nb, results_swig, results_bp):
    """Print comparison table"""
    benchmarks = [
        ('Null call', 'null_call'),
        ('Add int', 'arithmetic_int'),
        ('Multiply double', 'arithmetic_double'),
        ('Concat string', 'string_concat'),
        ('Get string', 'string_get'),
        ('Set string', 'string_set'),
        ('Vector append', 'vector_append'),
        ('Vector get', 'vector_get'),
        ('Vector set', 'vector_set'),
        ('Attr get', 'attr_get'),
        ('Attr set', 'attr_set'),
        ('Construction', 'construction'),
    ]

    print("\n" + "="*120)
    print(" Runtime Performance Benchmarks (lower is better)")
    print("="*120)
    print(f"\n{'Benchmark':<20} {'MB':<12} {'pybind11':<12} {'nanobind':<12} {'SWIG':<12} {'Boost.Py':<12} {'vs pb11':<10}")
    print("-"*120)

    for name, key in benchmarks:
        mb = results_mb[key]
        pb = results_pb[key]
        nb = results_nb[key]
        sw = results_swig[key] if results_swig else None
        bp = results_bp[key] if results_bp else None
        ratio_pb = mb / pb if pb > 0 else 0
        marker = "✓" if ratio_pb <= 1.1 else ("⚠" if ratio_pb <= 1.5 else "✗")

        swig_str = format_time(sw) if sw else "N/A"
        bp_str = format_time(bp) if bp else "N/A"

        print(f"{name:<20} {format_time(mb):<12} {format_time(pb):<12} {format_time(nb):<12} {swig_str:<12} {bp_str:<12} {ratio_pb:.2f}x {marker}")

    print("\n" + "="*120)
    print("Legend: ✓ within 10%  |  ⚠ within 50%  |  ✗ slower than 50%  |  N/A = not available")
    print("="*120)

if __name__ == '__main__':
    if len(sys.argv) != 2 or sys.argv[1] not in ['run', 'summary']:
        print("Usage: python3 run_runtime_benchmarks.py [run|summary]")
        print("  run     - Run benchmarks and show results")
        print("  summary - Show summary comparison")
        sys.exit(1)

    mode = sys.argv[1]

    if mode == 'run':
        print("Loading modules...")
        # Use absolute path to build directory
        import os
        script_dir = os.path.dirname(os.path.abspath(__file__))
        build_dir = os.path.join(script_dir, '..', '..', '..', 'build')
        sys.path.insert(0, os.path.abspath(build_dir))

        try:
            import bench_mb
            import bench_pb
            import bench_nb
        except ImportError as e:
            print(f"Error: Failed to import benchmark modules: {e}")
            print("Please run the compile-time benchmarks first to build the modules.")
            sys.exit(1)

        print("Running Mirror Bridge benchmarks...")
        results_mb = run_benchmarks('bench_mb', bench_mb.BenchmarkClass)

        print("Running pybind11 benchmarks...")
        results_pb = run_benchmarks('bench_pb', bench_pb.BenchmarkClass)

        print("Running nanobind benchmarks...")
        results_nb = run_benchmarks('bench_nb', bench_nb.BenchmarkClass)

        # Optional frameworks: record only what actually ran. Writing
        # zero-filled placeholders would masquerade as measurements in the
        # saved JSON, so absent frameworks are omitted entirely.
        try:
            import bench_swig
            print("Running SWIG benchmarks...")
            results_swig = run_benchmarks('bench_swig', bench_swig.BenchmarkClass)
        except ImportError:
            print("SWIG module not available, skipping...")
            results_swig = None

        try:
            import bench_bp
            print("Running Boost.Python benchmarks...")
            results_bp = run_benchmarks('bench_bp', bench_bp.BenchmarkClass)
        except ImportError:
            print("Boost.Python module not available, skipping...")
            results_bp = None

        print_results(results_mb, results_pb, results_nb, results_swig, results_bp)

        # Save results
        import json
        all_results = {
            'mirror_bridge': results_mb,
            'pybind11': results_pb,
            'nanobind': results_nb,
            'swig': results_swig,
            'boost_python': results_bp
        }
        with open('runtime_results.json', 'w') as f:
            json.dump({k: v for k, v in all_results.items() if v is not None},
                      f, indent=2)
        print("\nResults saved to runtime_results.json")
