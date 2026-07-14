# Mirror Bridge Benchmarks

Comprehensive performance benchmarks comparing Mirror Bridge against:
- **Python**: pybind11, Boost.Python, nanobind, SWIG
- **Lua**: Plain Lua C API, sol2, LuaBridge3
- **JavaScript**: Plain N-API, node-addon-api

## Quick Links

- **[Current Results](../docs/internals/benchmarks.md)** - The canonical numbers, regenerated monthly by CI from a fresh run
- **[Lua/JS Detailed Results](docs/LUA_JS_RESULTS.md)** - One-off manual Lua and JavaScript analysis
- **[Quick Start Guide](docs/QUICKSTART.md)** - Fast introduction to running benchmarks

> **Which numbers to trust:** `docs/internals/benchmarks.md` at the repo root
> is the only self-updating results page — CI reruns the suite and regenerates
> it, so it can't drift from the code. Everything else in this directory is
> either methodology documentation or a dated manual run.

## Directory Structure

```
benchmarks/
├── README.md                    # This file
├── docs/                        # Documentation and results
│   ├── LUA_JS_RESULTS.md       # Lua/JS detailed analysis (manual run)
│   └── QUICKSTART.md           # Quick start guide
├── compile_time/               # Compile-time benchmarks
│   ├── simple/                 # 1 class, ~10 methods
│   ├── medium/                 # 10 classes, ~50 methods
│   └── run_benchmarks.sh       # Compile-time runner
├── runtime/                    # Runtime performance benchmarks
│   ├── shared/                 # Shared test class
│   │   └── benchmark_class.hpp
│   ├── python/                 # Python benchmarks
│   ├── lua/                    # Lua benchmarks
│   └── javascript/             # JavaScript benchmarks
└── run_all_benchmarks.sh      # Master benchmark script
```

## Benchmark Categories

### 1. Compile-Time Performance

Measures compilation speed and binary size for different project scales.

**Test Cases:**
- **Simple** (1 class, ~10 methods): Basic class with getters, setters, and methods
- **Medium** (10 classes, ~50 methods): Realistic 3D scene graph with interconnected classes

**Metrics:**
- Compilation time (milliseconds)
- Binary size (KB)
- Lines of binding code

### 2. Runtime Performance

Measures overhead of different operations at runtime.

**Test Cases:**
- **Null call**: Empty method call (pure overhead)
- **Arithmetic**: int and double operations
- **String operations**: Concatenation, get/set
- **Vector operations**: Append, get/set
- **Attribute access**: Get/set member variables
- **Object construction**: Creating new instances

**Iterations:**
- 10M iterations for simple ops (null call, arithmetic, attributes)
- 1M iterations for string ops
- 100K iterations for vector ops
- 1M iterations for object construction

### 3. Developer Experience

Measures how much code developers need to write.

**Metrics:**
- Lines of binding code
- Binding code complexity
- Maintainability (manual vs auto-generated)

## Running Benchmarks

### Python Benchmarks (Quick Start)

```bash
# From project root
./run_benchmarks.sh
```

This script will:
1. Build the benchmark Docker image (if not already built)
2. Run all benchmarks inside the container
3. Display results and save to JSON

**First run**: ~30-60 minutes (builds Docker image with clang + pybind11 + Boost.Python)
**Subsequent runs**: ~5-6 minutes (just runs benchmarks)

### Lua Benchmarks

```bash
# Runtime benchmarks (Mirror Bridge vs Plain Lua C API, sol2)
docker exec mirror_bridge_dev bash -c "cd /workspace/benchmarks/runtime/lua && bash run_benchmarks.sh"

# Compile-time benchmarks
docker exec mirror_bridge_dev bash -c "cd /workspace/benchmarks/compile_time && bash run_benchmarks.sh"
```

### JavaScript Benchmarks

```bash
# Runtime benchmarks (Mirror Bridge vs Plain N-API, node-addon-api)
docker exec mirror_bridge_dev bash -c "cd /workspace/benchmarks/runtime/javascript && bash run_benchmarks.sh"

# Compile-time benchmarks (included in Lua/JS compile benchmark script)
docker exec mirror_bridge_dev bash -c "cd /workspace/benchmarks/compile_time && bash run_benchmarks.sh"
```

### Manual Run (Advanced)

If you want more control:

```bash
# Build benchmark image
docker build -f Dockerfile.benchmarks -t mirror_bridge:benchmarks .

# Run benchmarks
docker run --rm -v $(pwd):/workspace mirror_bridge:benchmarks bash -c \
  "cd /workspace/benchmarks && ./run_all_benchmarks.sh"
```

### What Gets Run

The benchmark suite will:
1. Build all binding modules (Mirror Bridge, pybind11, Boost.Python)
2. Measure compile times for simple and medium projects
3. Run runtime performance tests
4. Generate comparison report

### Expected Runtime

- Compile-time benchmarks: ~30 seconds
- Runtime benchmarks: ~3-5 minutes
- **Total: ~5-6 minutes**

## Understanding Results

### Compile-Time Results

Example output:
```
Simple Project (1 class, ~10 methods)
  Mirror Bridge...  567ms  (no PCH) / 194ms (with PCH)
  pybind11...       1938ms
  nanobind...       165ms
```

**What to look for:**
- **Compilation time**: Lower is better (faster developer iteration)
- **Binary size**: Lower is better (smaller deployment footprint)
- Mirror Bridge compiles ~2-3x faster than pybind11; nanobind compiles
  faster still because it has no reflection machinery — Mirror Bridge
  only matches it when the precompiled header is used (see
  `docs/guides/pch.md`)

### Runtime Results

Example output (see `docs/internals/benchmarks.md` for current numbers):
```
Benchmark            Mirror Bridge   pybind11        nanobind        vs pb11
--------------------------------------------------------------------------------
Null call            73 ns           190 ns          70 ns           2.59x
Add int              98 ns           218 ns          84 ns           2.24x
Vector return        316 ns          914 ns          2650 ns         2.89x
```

**Legend (console output):**
- ✓ within 10% of pybind11
- ⚠ within 50% of pybind11
- ✗ more than 50% slower than pybind11

**What to look for:**
- Mirror Bridge is typically **2-4x faster than pybind11** on dispatch-bound
  operations and at rough **parity with nanobind** (faster on `std::vector`
  returns, slightly slower on some scalar ops)
- Absolute nanoseconds shift with hardware; compare the ratio columns

### Developer Experience Results

Example output:
```
Lines of Binding Code

Simple project:
  Mirror Bridge:  6 lines
  pybind11:       18 lines
  Boost.Python:   21 lines

Medium project:
  Mirror Bridge:  16 lines
  pybind11:       120 lines
  Boost.Python:   135 lines
```

**What to look for:**
- Mirror Bridge should require **significantly less code** (5-10x reduction for medium projects)
- This demonstrates the value of automatic reflection-based binding

### Lua Runtime Results

Example output:
```
Benchmark            Mirror Bridge   Plain Lua C     Ratio
------------------------------------------------------------------------------------------
Null call            99.8 ns         84.6 ns         1.18x           ⚠
Add int              116.2 ns        95.6 ns         1.22x           ⚠
String concat        137.7 ns        117.3 ns        1.17x           ⚠
Vector set           213.1 ns        206.8 ns        1.03x           ✓
Attr get             24.8 ns         45.2 ns         0.55x           ✓
Attr set             35.2 ns         51.3 ns         0.68x           ✓
Construction         94.5 ns         103.9 ns        0.91x           ✓
```

**Key findings:**
- **Overall 5% FASTER than plain C API** (0.95x average)
- **Attribute access 30-48% FASTER** (get: 0.52x, set: 0.70x)
- **Object construction 18% faster** (0.82x)
- **Most operations within 10%** of manual C code
- Only string operations show overhead (11-13% slower)

### JavaScript Runtime Results

Example output:
```
Benchmark            Mirror Bridge   Plain N-API     Ratio
------------------------------------------------------------------------------------------
Null call            76.1 ns         84.0 ns         0.91x           ✓
String concat        167.9 ns        219.4 ns        0.77x           ✓
Vector set           372.3 ns        477.5 ns        0.78x           ✓
Attr get             91.2 ns         76.1 ns         1.20x           ⚠
Construction         497.3 ns        506.8 ns        0.98x           ✓
```

**Key findings:**
- **Average 9% FASTER** than plain N-API (0.91x ratio)
- **String operations 20-30% faster** (set_string: 0.70x, concat: 0.80x)
- **Vector operations 16% faster** (set_vector: 0.84x)
- **Method calls equal or faster** (null_call: 0.76x, 24% faster!)
- **Attribute access nearly equal** (get: 1.09x, set: 0.96x)

### Lua/JS Compile-Time Results

Example output:
```
Benchmark            Lua                  JavaScript
------------------------------------------------------------
Simple Class         450ms                431ms
Medium Class         341ms                344ms
```

**Key findings:**
- Sub-second compilation for simple classes
- Lua and JavaScript compilation times are comparable
- Fast iteration during development

## Benchmark Implementation Details

### Compile-Time Benchmarks

Each binding is compiled 3 times and the median is reported to reduce variance from:
- Filesystem caching
- CPU frequency scaling
- Background processes

Caches are cleared between runs using `sync` and `drop_caches`.

### Runtime Benchmarks

Each operation is:
1. **Warmed up**: 1000 iterations (or 10% of total) to ensure JIT compilation
2. **Measured**: 5 runs of full iteration count
3. **Aggregated**: Median of 5 runs reported

This reduces variance from:
- Python JIT warmup
- CPU frequency scaling
- Garbage collection

### Statistical Rigor

- **Median** is used instead of mean to reduce impact of outliers
- **Multiple runs** ensure reproducibility
- **High iteration counts** reduce per-call measurement overhead
- **Warmup phase** ensures steady-state performance

## Interpreting Success

Mirror Bridge is considered successful if:

1. **Compile-time**: 2x+ faster than pybind11 (nanobind-competitive with PCH)
2. **Runtime**: Faster than pybind11 across the board, parity with nanobind
3. **Developer Experience**: zero hand-written binding code

The key value proposition is:
> **"Write no binding code, compile faster than pybind11, run as fast as the best hand-tuned binding."**

## Output Files

- `runtime_results.json`: Detailed runtime benchmark data (nanoseconds per operation)
- Console output: Human-readable summary with speedup ratios

## Customizing Benchmarks

To add new test cases:

1. **Compile-time**: Add new directory under `compile_time/` with C++ header + 3 binding files
2. **Runtime**: Add new method to `benchmark_class.hpp` and update `run_runtime_benchmarks.py`

To change iteration counts, edit `ITERATIONS` dictionary in `run_runtime_benchmarks.py`.

## Known Limitations

- Benchmarks assume Docker environment with reflection-enabled clang
- pybind11 and Boost.Python must be installed in container
- Some variance expected due to system load, run multiple times for confidence
- Binary size depends on optimization level and linking mode

## Contributing

When adding benchmarks:
1. Ensure all three implementations (MB, pb11, BP) are equivalent
2. Document what the benchmark measures
3. Choose appropriate iteration counts (balance accuracy vs runtime)
4. Include results in PR description
