# Mandelbrot Multi-Language Demo

This demo shows the same C++ Mandelbrot renderer working in **Python**, **Lua**, and **JavaScript** - all from a single class definition using Mirror Bridge.

## Prerequisites

Run inside the Mirror Bridge dev container:

```bash
cd /path/to/mirror_bridge
./start_dev_container.sh
```

## Quick Start

Build all bindings and run benchmarks:

```bash
cd examples/mandelbrot-demo
bash build_mandelbrot_full.sh
```

This will:
1. Build Python, Lua, and JavaScript bindings
2. Run comprehensive benchmarks (CPython, PyPy, Lua 5.4, LuaJIT, V8)
3. Generate a zoom animation

## Benchmark Results

C++ bindings beat all runtimes, including JIT compilers:

| Runtime | Time | vs C++ Binding |
|---------|------|----------------|
| Pure CPython | 2.54s | 42x slower |
| Pure PyPy (JIT) | 0.14s | 2.3x slower |
| Pure Lua 5.4 | 0.76s | 11x slower |
| Pure LuaJIT | 0.24s | 3.4x slower |
| Pure V8 (JIT) | 0.09s | 1.6x slower |
| **C++ Binding** | 0.06s | baseline |

## Files

- `src/mandelbrot.hpp` - The C++ Mandelbrot renderer
- `python/` - Python binding and pure Python implementation
- `lua/` - Lua binding and pure Lua implementation
- `javascript/` - JavaScript binding and pure JS implementation
- `benchmark_all_runtimes.sh` - Comprehensive benchmark script
- `generate_animation.py` - Creates zoom animation frames

## Blog Post

See the full write-up: [One C++ Engine, Three Languages](https://chico.dev/Mirror-Bridge-Multi-Language/)
