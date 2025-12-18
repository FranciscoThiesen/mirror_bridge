#!/bin/bash
# Build Mandelbrot bindings for all three languages and run benchmarks
# MUST be run inside Docker container

set -e

# Check if we're in Docker
if [ ! -f /.dockerenv ] && [ -z "$DOCKER_CONTAINER" ]; then
    echo ""
    echo "ERROR: This script must be run inside the Docker container."
    echo ""
    echo "To run:"
    echo "  docker run --rm -v \$(pwd):/workspace -w /workspace mirror_bridge:latest \\"
    echo "    bash -c 'cd blog/demo && bash build_mandelbrot_full.sh'"
    echo ""
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'
BOLD='\033[1m'

echo ""
echo -e "${BOLD}${BLUE}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║     Mandelbrot Performance Benchmark: Pure vs C++        ║${NC}"
echo -e "${BOLD}${BLUE}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""

mkdir -p "$SCRIPT_DIR/build"
mkdir -p "$SCRIPT_DIR/build/frames"

# Common flags
CXX="clang++"
COMMON_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++ -fPIC -shared -O3"
INCLUDES="-I$PROJECT_ROOT -I$SCRIPT_DIR/src"

# ============================================================
# Build all bindings
# ============================================================
echo -e "${CYAN}Building C++ bindings for all three languages...${NC}"
echo ""

# Python
echo -e "  ${YELLOW}[1/3]${NC} Python binding..."
PYTHON_FLAGS=$(python3-config --includes)
$CXX $COMMON_FLAGS $INCLUDES $PYTHON_FLAGS \
    "$SCRIPT_DIR/python/mandelbrot_binding.cpp" \
    -o "$SCRIPT_DIR/build/mandelbrot.so" 2>&1 | grep -v "mixture of designated" || true
echo -e "        ${GREEN}✓${NC} build/mandelbrot.so"

# Lua
echo -e "  ${YELLOW}[2/3]${NC} Lua binding..."
LUA_FLAGS="-I/usr/include/lua5.4"
LUA_LIBS="-llua5.4"
$CXX $COMMON_FLAGS $INCLUDES $LUA_FLAGS \
    "$SCRIPT_DIR/lua/mandelbrot_binding.cpp" \
    -o "$SCRIPT_DIR/build/mandelbrot_lua.so" $LUA_LIBS 2>&1 | grep -v "mixture of designated" || true
cp "$SCRIPT_DIR/build/mandelbrot_lua.so" "$SCRIPT_DIR/build/mandelbrot.so.lua" 2>/dev/null || true
echo -e "        ${GREEN}✓${NC} build/mandelbrot_lua.so"

# JavaScript
echo -e "  ${YELLOW}[3/3]${NC} JavaScript binding..."
NODE_FLAGS="-I/usr/include/node"
$CXX $COMMON_FLAGS $INCLUDES $NODE_FLAGS \
    "$SCRIPT_DIR/javascript/mandelbrot_binding.cpp" \
    -o "$SCRIPT_DIR/build/mandelbrot.node" 2>&1 | grep -v "mixture of designated" || true
echo -e "        ${GREEN}✓${NC} build/mandelbrot.node"

echo ""
echo -e "${GREEN}All bindings built successfully!${NC}"
echo ""

# ============================================================
# Run benchmarks
# ============================================================
echo -e "${BOLD}${BLUE}══════════════════════════════════════════════════════════${NC}"
echo -e "${BOLD}${BLUE}                    PERFORMANCE BENCHMARKS                 ${NC}"
echo -e "${BOLD}${BLUE}══════════════════════════════════════════════════════════${NC}"
echo ""

cd "$SCRIPT_DIR"

# Python benchmark
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━ PYTHON ━━━━━━━━━━━━━━━━━━${NC}"
echo ""
PYTHONPATH=build python3 << 'PYTHON_BENCH'
import time
import sys
sys.path.insert(0, 'build')

# Pure Python
from python.mandelbrot_pure import render_mandelbrot as pure_render

# C++ binding
import mandelbrot

width, height = 800, 600
max_iter = 256
x_min, x_max = -2.5, 1.0
y_min, y_max = -1.25, 1.25

print(f"  Resolution: {width}x{height}, {max_iter} iterations")
print()

# Benchmark pure Python
print("  Pure Python:")
start = time.perf_counter()
_ = pure_render(width, height, max_iter, x_min, x_max, y_min, y_max)
pure_time = time.perf_counter() - start
print(f"    Time: {pure_time:.3f}s")

# Benchmark C++ binding
print("  C++ Binding:")
m = mandelbrot.Mandelbrot(width, height, max_iter)
start = time.perf_counter()
_ = m.render_rgb()
cpp_time = time.perf_counter() - start
print(f"    Time: {cpp_time:.3f}s")

speedup = pure_time / cpp_time
print()
print(f"  {chr(0x2192)} Speedup: {speedup:.1f}x faster with C++")
print()
PYTHON_BENCH

# Lua benchmark
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━ LUA ━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Create Lua benchmark script
cat > /tmp/lua_bench.lua << 'LUA_BENCH'
package.cpath = "/workspace/blog/demo/build/?.so;/workspace/blog/demo/build/mandelbrot_lua.so;;"
package.path = "/workspace/blog/demo/lua/?.lua;;"

local pure = require("mandelbrot_pure")

local width, height = 800, 600
local max_iter = 256
local x_min, x_max = -2.5, 1.0
local y_min, y_max = -1.25, 1.25

print(string.format("  Resolution: %dx%d, %d iterations", width, height, max_iter))
print()

-- Benchmark pure Lua
print("  Pure Lua:")
local start = os.clock()
local _ = pure.render_mandelbrot(width, height, max_iter, x_min, x_max, y_min, y_max)
local pure_time = os.clock() - start
print(string.format("    Time: %.3fs", pure_time))

-- Benchmark C++ binding
print("  C++ Binding:")
-- Load C++ binding - need to rename for Lua's require
local cpp_path = "/workspace/blog/demo/build/mandelbrot_lua.so"
package.cpath = cpp_path .. ";;"
package.loaded["mandelbrot_lua"] = nil  -- Clear cache

local ok, mb = pcall(function()
    -- Lua require expects module name without path
    package.cpath = "/workspace/blog/demo/build/?.so;;"
    return require("mandelbrot_lua")
end)

if ok and mb and mb.Mandelbrot then
    local m = mb.Mandelbrot(width, height, max_iter)
    start = os.clock()
    local _ = m:render_rgb()
    local cpp_time = os.clock() - start
    print(string.format("    Time: %.3fs", cpp_time))

    local speedup = pure_time / cpp_time
    print()
    print(string.format("  → Speedup: %.1fx faster with C++", speedup))
else
    print("    Error: " .. tostring(mb))
end
print()
LUA_BENCH

lua5.4 /tmp/lua_bench.lua

# JavaScript benchmark
echo -e "${CYAN}━━━━━━━━━━━━━━━━━ JAVASCRIPT ━━━━━━━━━━━━━━━━${NC}"
echo ""

cat > /tmp/js_bench.js << 'JS_BENCH'
const { performance } = require('perf_hooks');
const pure = require('/workspace/blog/demo/javascript/mandelbrot_pure.js');
const cpp = require('/workspace/blog/demo/build/mandelbrot.node');

const width = 800, height = 600;
const maxIter = 256;
const xMin = -2.5, xMax = 1.0;
const yMin = -1.25, yMax = 1.25;

console.log(`  Resolution: ${width}x${height}, ${maxIter} iterations`);
console.log();

// Benchmark pure JavaScript
console.log("  Pure JavaScript:");
let start = performance.now();
let _ = pure.renderMandelbrot(width, height, maxIter, xMin, xMax, yMin, yMax);
const pureTime = (performance.now() - start) / 1000;
console.log(`    Time: ${pureTime.toFixed(3)}s`);

// Benchmark C++ binding
console.log("  C++ Binding:");
const m = new cpp.Mandelbrot(width, height, maxIter);
start = performance.now();
_ = m.render_rgb();
const cppTime = (performance.now() - start) / 1000;
console.log(`    Time: ${cppTime.toFixed(3)}s`);

const speedup = pureTime / cppTime;
console.log();
console.log(`  → Speedup: ${speedup.toFixed(1)}x faster with C++`);
console.log();
JS_BENCH

node /tmp/js_bench.js

echo -e "${BOLD}${BLUE}══════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "${GREEN}${BOLD}Benchmarks complete!${NC}"
echo ""
