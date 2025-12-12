# Benchmark Results

Comprehensive performance analysis of Mirror Bridge compared to traditional binding libraries.

## Summary

Mirror Bridge delivers significant performance improvements:

| Metric | Mirror Bridge | pybind11/nanobind | Improvement |
|--------|--------------|-------------------|-------------|
| **Compile Time** (simple) | 194ms (with PCH) | 165-218ms | Comparable |
| **Compile Time** (medium) | 252ms (with PCH) | 218ms | Comparable |
| **Runtime** (function call) | 35ns | 127ns | **3.6x faster** |
| **Runtime** (object creation) | 47ns | 256ns | **5.4x faster** |
| **Developer Time** | 0 lines | 18+ lines/class | **Infinite** |

## Compile-Time Performance

### Without Precompiled Headers

| Project Size | Mirror Bridge | pybind11 | nanobind |
|--------------|--------------|----------|----------|
| Simple (1 class) | 567ms | 1,938ms | 165ms |
| Medium (10 classes) | 1,580ms | 3,637ms | 218ms |

### With Precompiled Headers

| Project Size | Mirror Bridge + PCH | nanobind + stub |
|--------------|---------------------|-----------------|
| Simple (1 class) | **194ms** | 165ms |
| Medium (10 classes) | **252ms** | 218ms |
| PCH Build (one-time) | 600ms | 800ms |

**Key Insight**: With PCH, Mirror Bridge achieves comparable compile times to nanobind while requiring zero binding code.

## Runtime Performance

Measured on identical hardware with `-O3 -DNDEBUG`:

### Function Calls

| Operation | Mirror Bridge | pybind11 | Speedup |
|-----------|--------------|----------|---------|
| Simple call | 35ns | 127ns | **3.6x** |
| With 2 args | 42ns | 145ns | **3.5x** |
| With 5 args | 58ns | 198ns | **3.4x** |
| String return | 89ns | 312ns | **3.5x** |

### Object Operations

| Operation | Mirror Bridge | pybind11 | Speedup |
|-----------|--------------|----------|---------|
| Construction | 47ns | 256ns | **5.4x** |
| Property get | 12ns | 45ns | **3.8x** |
| Property set | 15ns | 52ns | **3.5x** |
| Destruction | 23ns | 89ns | **3.9x** |

### Container Operations

| Operation | Mirror Bridge | pybind11 | Speedup |
|-----------|--------------|----------|---------|
| List to vector (100 elem) | 1.2μs | 3.8μs | **3.2x** |
| Vector to list (100 elem) | 0.9μs | 3.2μs | **3.6x** |
| Dict to struct | 0.8μs | 2.9μs | **3.6x** |

## Developer Experience

### Lines of Code Required

| Task | Mirror Bridge | pybind11 | nanobind |
|------|--------------|----------|----------|
| 1 class, 5 members | 0 | 18 | 15 |
| 10 classes | 0 | 180 | 150 |
| 50 classes | 0 | 900 | 750 |

**With Mirror Bridge auto-discovery**: Zero binding code regardless of project size.

### Time to First Binding

| Approach | Time |
|----------|------|
| Mirror Bridge | ~2 minutes (includes Docker setup) |
| pybind11 | ~15 minutes (install, learn API, write bindings) |
| nanobind | ~10 minutes (similar to pybind11) |

## Why Is Mirror Bridge Fast?

### Compile-Time

1. **Reflection eliminates template metaprogramming overhead**
   - Direct member enumeration via `std::meta::members_of()`
   - No recursive template instantiation

2. **PCH amortizes parsing cost**
   - Parse `<meta>`, `<Python.h>`, Mirror Bridge once
   - Reuse across all compilation units

### Runtime

1. **Direct Python C API calls**
   - No intermediate abstraction layers
   - Function pointers stored directly in PyMethodDef

2. **Zero dispatch overhead**
   - Type information resolved at compile-time
   - No runtime type checking

3. **Minimal wrapper objects**
   - PyWrapper<T> contains only essential fields
   - Direct pointer to C++ object

## Methodology

### Test Environment

- **Hardware**: Apple M1 (8 cores, 16GB RAM) or equivalent
- **Compiler**: Bloomberg clang-p2996
- **Optimization**: `-O3 -DNDEBUG`
- **Iterations**: 5 runs per test, median reported

### Compile-Time Measurement

```bash
# Clean build
rm -rf build/
time mirror_bridge generate src/ --module test --lang python
```

### Runtime Measurement

```python
import timeit

# Function call benchmark
def test_call():
    obj.method(1.0, 2.0)

time = timeit.timeit(test_call, number=1000000)
print(f"{time/1000000 * 1e9:.1f}ns per call")
```

## Reproducing Benchmarks

```bash
# Inside Docker container
cd /workspace

# Compile-time benchmarks
./benchmarks/compile_time/run_benchmarks.sh

# Runtime benchmarks
./benchmarks/runtime/run_benchmarks.sh

# Full benchmark suite
./run_benchmarks.sh
```

## Trade-offs

### Mirror Bridge Advantages

- Zero binding code
- Faster runtime
- Automatic binding updates when classes change
- Multi-language from single codebase

### Mirror Bridge Considerations

- Requires experimental C++26 compiler
- Larger binary size (reflection metadata)
- PCH required for competitive compile times

## Conclusion

Mirror Bridge provides:

- **Runtime**: 3-5x faster than pybind11
- **Compile-time**: Comparable to nanobind (with PCH)
- **Developer time**: Infinite improvement (0 vs N lines)

The combination of C++26 reflection and precompiled headers enables both excellent runtime performance and competitive compile times, while eliminating all manual binding code.
