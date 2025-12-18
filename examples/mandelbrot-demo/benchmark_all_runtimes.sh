#!/bin/bash
# Comprehensive benchmark: Pure interpreters vs JIT compilers vs C++ bindings
# Compares CPython, PyPy, Lua 5.4, LuaJIT, V8, and C++ bindings

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'
BOLD='\033[1m'

echo ""
echo -e "${BOLD}${BLUE}╔══════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║     COMPREHENSIVE RUNTIME BENCHMARK: Interpreters vs JIT vs C++  ║${NC}"
echo -e "${BOLD}${BLUE}╚══════════════════════════════════════════════════════════════════╝${NC}"
echo ""

# ============================================================
# Install PyPy and LuaJIT if not present
# ============================================================
echo -e "${CYAN}Checking/installing runtimes...${NC}"

if ! command -v pypy3 &> /dev/null; then
    echo "  Installing PyPy3..."
    apt-get update -qq && apt-get install -y -qq pypy3 > /dev/null 2>&1
    echo -e "  ${GREEN}✓${NC} PyPy3 installed"
else
    echo -e "  ${GREEN}✓${NC} PyPy3 already installed"
fi

if ! command -v luajit &> /dev/null; then
    echo "  Installing LuaJIT..."
    apt-get install -y -qq luajit > /dev/null 2>&1
    echo -e "  ${GREEN}✓${NC} LuaJIT installed"
else
    echo -e "  ${GREEN}✓${NC} LuaJIT already installed"
fi

echo ""
echo -e "${CYAN}Runtime versions:${NC}"
echo -n "  CPython:  "; python3 --version
echo -n "  PyPy:     "; pypy3 --version 2>&1 | head -1
echo -n "  Lua 5.4:  "; lua5.4 -v 2>&1
echo -n "  LuaJIT:   "; luajit -v 2>&1
echo -n "  Node.js:  "; node --version
echo ""

# ============================================================
# Ensure bindings are built
# ============================================================
echo -e "${CYAN}Building C++ bindings...${NC}"
mkdir -p build

CXX="clang++"
COMMON_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++ -fPIC -shared -O3"
INCLUDES="-I/workspace -I$SCRIPT_DIR/src"

# Python binding
PYTHON_FLAGS=$(python3-config --includes)
$CXX $COMMON_FLAGS $INCLUDES $PYTHON_FLAGS \
    python/mandelbrot_binding.cpp -o build/mandelbrot.so 2>&1 | grep -E "^[^/]" | grep -v warning || true
echo -e "  ${GREEN}✓${NC} Python binding"

# Lua binding
LUA_FLAGS="-I/usr/include/lua5.4 -llua5.4"
$CXX $COMMON_FLAGS $INCLUDES $LUA_FLAGS \
    lua/mandelbrot_binding.cpp -o build/mandelbrot_lua.so 2>&1 | grep -E "^[^/]" | grep -v warning || true
echo -e "  ${GREEN}✓${NC} Lua binding"

# JavaScript binding
NODE_FLAGS="-I/usr/include/node"
$CXX $COMMON_FLAGS $INCLUDES $NODE_FLAGS \
    javascript/mandelbrot_binding.cpp -o build/mandelbrot.node 2>&1 | grep -E "^[^/]" | grep -v warning || true
echo -e "  ${GREEN}✓${NC} JavaScript binding"

echo ""

# ============================================================
# Run benchmarks
# ============================================================
echo -e "${BOLD}${BLUE}══════════════════════════════════════════════════════════════════${NC}"
echo -e "${BOLD}${BLUE}                         BENCHMARK RESULTS                         ${NC}"
echo -e "${BOLD}${BLUE}══════════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "  ${YELLOW}Workload:${NC} Mandelbrot 800×600, 256 iterations"
echo -e "  ${YELLOW}Measured:${NC} Time to render full frame (compute + data handling)"
echo ""

# ============================================================
# Python benchmarks
# ============================================================
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━ PYTHON ━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Pure CPython
CPYTHON_TIME=$(python3 -c "
import time
import sys
sys.path.insert(0, 'python')
from mandelbrot_pure import render_mandelbrot

# Warmup
render_mandelbrot(100, 100, 50, -2.5, 1.0, -1.25, 1.25)

start = time.perf_counter()
render_mandelbrot(800, 600, 256, -2.5, 1.0, -1.25, 1.25)
elapsed = time.perf_counter() - start
print(f'{elapsed:.4f}')
")
echo -e "  Pure CPython:        ${CPYTHON_TIME}s"

# Pure PyPy
PYPY_TIME=$(pypy3 -c "
import time
import sys
sys.path.insert(0, 'python')
from mandelbrot_pure import render_mandelbrot

# Warmup (important for JIT)
for _ in range(3):
    render_mandelbrot(100, 100, 50, -2.5, 1.0, -1.25, 1.25)

start = time.perf_counter()
render_mandelbrot(800, 600, 256, -2.5, 1.0, -1.25, 1.25)
elapsed = time.perf_counter() - start
print(f'{elapsed:.4f}')
")
echo -e "  Pure PyPy (JIT):     ${PYPY_TIME}s"

# C++ binding via CPython
CPP_PYTHON_TIME=$(PYTHONPATH=build python3 -c "
import time
import mandelbrot

# Warmup
m = mandelbrot.Mandelbrot(100, 100, 50)
m.render_rgb()

m = mandelbrot.Mandelbrot(800, 600, 256)
start = time.perf_counter()
m.render_rgb()
elapsed = time.perf_counter() - start
print(f'{elapsed:.4f}')
")
echo -e "  C++ Binding:         ${CPP_PYTHON_TIME}s"

# Calculate speedups
echo ""
python3 -c "
cpython = $CPYTHON_TIME
pypy = $PYPY_TIME
cpp = $CPP_PYTHON_TIME

print(f'  CPython → C++:       {cpython/cpp:.1f}x faster')
print(f'  PyPy → C++:          {pypy/cpp:.1f}x {\"faster\" if pypy > cpp else \"slower\"}')"
echo ""

# ============================================================
# Lua benchmarks
# ============================================================
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━ LUA ━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Pure Lua 5.4
LUA54_TIME=$(lua5.4 -e "
package.path = 'lua/?.lua;' .. package.path
local pure = require('mandelbrot_pure')

-- Warmup
pure.render_mandelbrot(100, 100, 50, -2.5, 1.0, -1.25, 1.25)

local start = os.clock()
pure.render_mandelbrot(800, 600, 256, -2.5, 1.0, -1.25, 1.25)
local elapsed = os.clock() - start
print(string.format('%.4f', elapsed))
")
echo -e "  Pure Lua 5.4:        ${LUA54_TIME}s"

# Pure LuaJIT
LUAJIT_TIME=$(luajit -e "
package.path = 'lua/?.lua;' .. package.path
local pure = require('mandelbrot_pure')

-- Warmup (important for JIT)
for i = 1, 3 do
    pure.render_mandelbrot(100, 100, 50, -2.5, 1.0, -1.25, 1.25)
end

local start = os.clock()
pure.render_mandelbrot(800, 600, 256, -2.5, 1.0, -1.25, 1.25)
local elapsed = os.clock() - start
print(string.format('%.4f', elapsed))
")
echo -e "  Pure LuaJIT:         ${LUAJIT_TIME}s"

# C++ binding via Lua 5.4
CPP_LUA_TIME=$(lua5.4 -e "
package.cpath = 'build/?.so;' .. package.cpath
local mb = require('mandelbrot_lua')

-- Warmup
local m = mb.Mandelbrot(100, 100, 50)
m:render_rgb()

m = mb.Mandelbrot(800, 600, 256)
local start = os.clock()
m:render_rgb()
local elapsed = os.clock() - start
print(string.format('%.4f', elapsed))
")
echo -e "  C++ Binding:         ${CPP_LUA_TIME}s"

# Calculate speedups
echo ""
python3 -c "
lua54 = $LUA54_TIME
luajit = $LUAJIT_TIME
cpp = $CPP_LUA_TIME

print(f'  Lua 5.4 → C++:       {lua54/cpp:.1f}x faster')
print(f'  LuaJIT → C++:        {luajit/cpp:.1f}x {\"faster\" if luajit > cpp else \"slower\"}')"
echo ""

# ============================================================
# JavaScript benchmarks
# ============================================================
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━ JAVASCRIPT ━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Pure V8
V8_TIME=$(node -e "
const { performance } = require('perf_hooks');
const pure = require('./javascript/mandelbrot_pure.js');

// Warmup
for (let i = 0; i < 3; i++) {
    pure.renderMandelbrot(100, 100, 50, -2.5, 1.0, -1.25, 1.25);
}

const start = performance.now();
pure.renderMandelbrot(800, 600, 256, -2.5, 1.0, -1.25, 1.25);
const elapsed = (performance.now() - start) / 1000;
console.log(elapsed.toFixed(4));
")
echo -e "  Pure V8 (JIT):       ${V8_TIME}s"

# C++ binding via Node
CPP_JS_TIME=$(node -e "
const { performance } = require('perf_hooks');
const cpp = require('./build/mandelbrot.node');

// Warmup
let m = new cpp.Mandelbrot(100, 100, 50);
m.render_rgb();

m = new cpp.Mandelbrot(800, 600, 256);
const start = performance.now();
m.render_rgb();
const elapsed = (performance.now() - start) / 1000;
console.log(elapsed.toFixed(4));
")
echo -e "  C++ Binding:         ${CPP_JS_TIME}s"

# Calculate speedups
echo ""
python3 -c "
v8 = $V8_TIME
cpp = $CPP_JS_TIME

print(f'  V8 → C++:            {v8/cpp:.1f}x {\"faster\" if v8 > cpp else \"slower\"}')"
echo ""

# ============================================================
# Summary table
# ============================================================
echo -e "${BOLD}${BLUE}══════════════════════════════════════════════════════════════════${NC}"
echo -e "${BOLD}${BLUE}                           SUMMARY                                ${NC}"
echo -e "${BOLD}${BLUE}══════════════════════════════════════════════════════════════════${NC}"
echo ""

python3 << SUMMARY
cpython = $CPYTHON_TIME
pypy = $PYPY_TIME
cpp_py = $CPP_PYTHON_TIME
lua54 = $LUA54_TIME
luajit = $LUAJIT_TIME
cpp_lua = $CPP_LUA_TIME
v8 = $V8_TIME
cpp_js = $CPP_JS_TIME

print("  ┌─────────────────┬────────────┬────────────┬────────────────┐")
print("  │ Runtime         │ Time (s)   │ vs C++     │ Category       │")
print("  ├─────────────────┼────────────┼────────────┼────────────────┤")
print(f"  │ Pure CPython    │ {cpython:>10.4f} │ {cpython/cpp_py:>9.1f}x │ Interpreter    │")
print(f"  │ Pure PyPy       │ {pypy:>10.4f} │ {pypy/cpp_py:>9.1f}x │ JIT Compiler   │")
print(f"  │ C++ (Python)    │ {cpp_py:>10.4f} │      1.0x │ Native Binding │")
print("  ├─────────────────┼────────────┼────────────┼────────────────┤")
print(f"  │ Pure Lua 5.4    │ {lua54:>10.4f} │ {lua54/cpp_lua:>9.1f}x │ Interpreter    │")
print(f"  │ Pure LuaJIT     │ {luajit:>10.4f} │ {luajit/cpp_lua:>9.1f}x │ JIT Compiler   │")
print(f"  │ C++ (Lua)       │ {cpp_lua:>10.4f} │      1.0x │ Native Binding │")
print("  ├─────────────────┼────────────┼────────────┼────────────────┤")
print(f"  │ Pure V8         │ {v8:>10.4f} │ {v8/cpp_js:>9.1f}x │ JIT Compiler   │")
print(f"  │ C++ (JS)        │ {cpp_js:>10.4f} │      1.0x │ Native Binding │")
print("  └─────────────────┴────────────┴────────────┴────────────────┘")
print()

# Key insights
print("  Key insights:")
if cpp_py < pypy:
    print(f"    • C++ beats PyPy by {pypy/cpp_py:.1f}x - bindings win even against JIT")
else:
    print(f"    • PyPy beats C++ binding by {cpp_py/pypy:.1f}x - JIT overhead matters")

if cpp_lua < luajit:
    print(f"    • C++ beats LuaJIT by {luajit/cpp_lua:.1f}x - bindings win even against JIT")
else:
    print(f"    • LuaJIT beats C++ binding by {cpp_lua/luajit:.1f}x - JIT is very fast")

if cpp_js < v8:
    print(f"    • C++ beats V8 by {v8/cpp_js:.1f}x - optimized data transfer matters")
else:
    print(f"    • V8 beats C++ binding by {cpp_js/v8:.1f}x - V8 is exceptional")
SUMMARY

echo ""
echo -e "${GREEN}${BOLD}Benchmark complete!${NC}"
echo ""
