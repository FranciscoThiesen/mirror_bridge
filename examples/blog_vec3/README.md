# Vec3 Example (from Blog Post)

This example accompanies the blog post ["mirror_bridge - making Python bindings frictionless"](https://chico.dev/Mirror-Bridge/).

It demonstrates:
1. How to use `mirror_bridge_auto` to generate Python bindings
2. Why simple method calls have limited speedup (cross-language overhead)
3. How moving hot loops to C++ gives dramatic speedups (67x on M3 Max)

## Files

- `vec3.hpp` - The C++ Vec3 class with a `hot_loop` static method
- `benchmark.py` - Python script comparing Python vs C++ performance

## Running the Example

Inside the Docker container:

```bash
cd /workspace/examples/blog_vec3

# Generate bindings (no binding code needed!)
../../mirror_bridge_auto . --module vec3 -o .

# Run the benchmark
python3 benchmark.py
```

## Expected Output

```
============================================================
Mirror Bridge Benchmark
============================================================

--- Benchmark 1: Simple dot() calls (1M iterations) ---
Shows cross-language overhead on trivial operations

Python: 0.11s
C++:    0.04s
Speedup: 2.9x

--- Benchmark 2: hot_loop() - entire loop in C++ ---
This is the real benchmark!

Python: 0.26s
C++:    0.004s
Speedup: 67x

Results match: True
```

*(Times measured on M3 Max MacBook Pro)*

## The Key Insight

The 2.9x speedup on simple calls is limited by Python→C++ call overhead.

The 67x speedup on `hot_loop` shows what happens when you move the entire loop to C++: you pay the call overhead once, and the loop runs at full native speed.

**Rule of thumb:** Don't wrap individual methods and call them millions of times. Identify your hot loops and move them entirely to C++.
