#!/bin/bash
# Quick compile-time benchmark - runs locally without Docker
# Compares mirror_bridge compilation times for simple and medium projects

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'
BOLD='\033[1m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build/benchmarks"

# Default settings
RUNS=5
WARMUP=1
OUTPUT_JSON=""
USE_PCH=0
PCH_PATH=""
VERBOSE=0

show_help() {
    cat << EOF
Quick Compile-Time Benchmark for Mirror Bridge

Usage: $(basename "$0") [options]

Options:
  -r, --runs N        Number of benchmark runs (default: 5)
  -w, --warmup N      Number of warmup runs (default: 1)
  --pch [PATH]        Use precompiled header (optional path)
  -o, --output FILE   Write results to JSON file
  -v, --verbose       Show compilation commands
  -h, --help          Show this help

Examples:
  # Basic benchmark
  ./quick_benchmark.sh

  # With PCH for realistic incremental compile times
  ./quick_benchmark.sh --pch

  # More runs for statistical confidence
  ./quick_benchmark.sh --runs 10

  # Save results
  ./quick_benchmark.sh --output results.json

EOF
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -r|--runs)
            RUNS="$2"
            shift 2
            ;;
        -w|--warmup)
            WARMUP="$2"
            shift 2
            ;;
        --pch)
            USE_PCH=1
            if [[ $# -gt 1 && ! "$2" =~ ^- ]]; then
                PCH_PATH="$2"
                shift 2
            else
                shift
            fi
            ;;
        -o|--output)
            OUTPUT_JSON="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            show_help
            exit 1
            ;;
    esac
done

# Check for clang with reflection
if ! command -v clang++ &> /dev/null; then
    echo -e "${RED}Error: clang++ not found${NC}"
    exit 1
fi

# Check for reflection support
if ! clang++ --help 2>&1 | grep -q "freflection"; then
    echo -e "${YELLOW}Warning: clang++ may not have reflection support${NC}"
fi

mkdir -p "$BUILD_DIR"

# Get Python config
PYTHON_INCLUDES=$(python3-config --includes 2>/dev/null || echo "-I/usr/include/python3")
PYTHON_LDFLAGS=$(python3-config --ldflags 2>/dev/null || echo "")

# Compile flags
BASE_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++ -fPIC -shared -O3 -DNDEBUG"
INCLUDES="-I$PROJECT_ROOT $PYTHON_INCLUDES"

echo -e "${BOLD}${BLUE}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║     Mirror Bridge Compile-Time Benchmark           ║${NC}"
echo -e "${BOLD}${BLUE}╚════════════════════════════════════════════════════╝${NC}"
echo ""

# Setup PCH if requested
if [ $USE_PCH -eq 1 ]; then
    if [ -z "$PCH_PATH" ]; then
        PCH_PATH="$BUILD_DIR/mirror_bridge_pch.hpp.gch"
    fi

    if [ ! -f "$PCH_PATH" ]; then
        echo -e "${YELLOW}Creating precompiled header...${NC}"
        clang++ $BASE_FLAGS $INCLUDES \
            -x c++-header "$PROJECT_ROOT/mirror_bridge_pch.hpp" \
            -o "$PCH_PATH" 2>&1 | grep -v "mixture of designated" || true
        echo -e "${GREEN}✓ PCH created: $PCH_PATH${NC}"
    else
        echo -e "${GREEN}✓ Using existing PCH: $PCH_PATH${NC}"
    fi
    echo ""
    PCH_FLAGS="-include-pch $PCH_PATH"
else
    PCH_FLAGS=""
fi

echo -e "${CYAN}Configuration:${NC}"
echo -e "  Runs: $RUNS (+ $WARMUP warmup)"
echo -e "  PCH: $([ $USE_PCH -eq 1 ] && echo 'Enabled' || echo 'Disabled')"
echo ""

# Function to measure compile time
measure_compile_time() {
    local name="$1"
    local source="$2"
    local output="$3"
    local extra_includes="$4"

    local times=()

    # Warmup runs
    for ((i=1; i<=WARMUP; i++)); do
        rm -f "$output"
        clang++ $BASE_FLAGS $INCLUDES $extra_includes $PCH_FLAGS \
            "$source" -o "$output" 2>&1 | grep -v "mixture of designated" > /dev/null || true
    done

    # Actual runs
    for ((i=1; i<=RUNS; i++)); do
        rm -f "$output"

        local start=$(python3 -c 'import time; print(int(time.time() * 1000))')
        clang++ $BASE_FLAGS $INCLUDES $extra_includes $PCH_FLAGS \
            "$source" -o "$output" 2>&1 | grep -v "mixture of designated" > /dev/null || true
        local end=$(python3 -c 'import time; print(int(time.time() * 1000))')

        local elapsed=$((end - start))
        times+=($elapsed)

        if [ $VERBOSE -eq 1 ]; then
            echo "    Run $i: ${elapsed}ms"
        fi
    done

    # Calculate statistics
    local sorted=($(printf '%s\n' "${times[@]}" | sort -n))
    local median_idx=$(( (RUNS - 1) / 2 ))
    local median="${sorted[$median_idx]}"

    # Calculate mean and stddev
    local sum=0
    for t in "${times[@]}"; do
        sum=$((sum + t))
    done
    local mean=$((sum / RUNS))

    local sq_sum=0
    for t in "${times[@]}"; do
        local diff=$((t - mean))
        sq_sum=$((sq_sum + diff * diff))
    done
    local variance=$((sq_sum / RUNS))
    local stddev=$(awk "BEGIN {printf \"%.0f\", sqrt($variance)}")

    # Get binary size
    local size=0
    if [ -f "$output" ]; then
        size=$(stat -f%z "$output" 2>/dev/null || stat -c%s "$output" 2>/dev/null || echo 0)
        size=$((size / 1024))
    fi

    # Return: median stddev size
    echo "$median $stddev $size"
}

format_time() {
    local ms=$1
    if [ "$ms" -lt 1000 ]; then
        echo "${ms}ms"
    else
        awk "BEGIN {printf \"%.2fs\", $ms/1000}"
    fi
}

# Arrays to store results
declare -a BENCHMARK_NAMES
declare -a MEDIANS
declare -a STDDEVS
declare -a SIZES

# Run benchmarks
echo -e "${YELLOW}Running benchmarks...${NC}"
echo ""

# Simple class benchmark
echo -e "${GREEN}[1/4] Simple Class (Python binding)${NC}"
result=$(measure_compile_time "simple_python" \
    "$SCRIPT_DIR/simple/mirror_bridge_binding.cpp" \
    "$BUILD_DIR/simple_mb.so" \
    "-I$SCRIPT_DIR/simple")
read median stddev size <<< "$result"
BENCHMARK_NAMES+=("Simple (Python)")
MEDIANS+=($median)
STDDEVS+=($stddev)
SIZES+=($size)
echo -e "  Compile: $(format_time $median) ± $(format_time $stddev) | Size: ${size}KB"

# Medium class benchmark
echo -e "${GREEN}[2/4] Medium Classes (Python binding)${NC}"
result=$(measure_compile_time "medium_python" \
    "$SCRIPT_DIR/medium/mirror_bridge_binding.cpp" \
    "$BUILD_DIR/medium_mb.so" \
    "-I$SCRIPT_DIR/medium")
read median stddev size <<< "$result"
BENCHMARK_NAMES+=("Medium (Python)")
MEDIANS+=($median)
STDDEVS+=($stddev)
SIZES+=($size)
echo -e "  Compile: $(format_time $median) ± $(format_time $stddev) | Size: ${size}KB"

# Lua binding if available
if [ -f "$SCRIPT_DIR/simple/mirror_bridge_binding_lua.cpp" ]; then
    echo -e "${GREEN}[3/4] Simple Class (Lua binding)${NC}"
    result=$(measure_compile_time "simple_lua" \
        "$SCRIPT_DIR/simple/mirror_bridge_binding_lua.cpp" \
        "$BUILD_DIR/simple_mb_lua.so" \
        "-I$SCRIPT_DIR/simple -I/usr/include/lua5.4" 2>/dev/null) || result="0 0 0"
    read median stddev size <<< "$result"
    if [ "$median" -gt 0 ]; then
        BENCHMARK_NAMES+=("Simple (Lua)")
        MEDIANS+=($median)
        STDDEVS+=($stddev)
        SIZES+=($size)
        echo -e "  Compile: $(format_time $median) ± $(format_time $stddev) | Size: ${size}KB"
    else
        echo -e "  ${YELLOW}Skipped (Lua headers not found)${NC}"
    fi
else
    echo -e "${GREEN}[3/4] Simple Class (Lua binding)${NC}"
    echo -e "  ${YELLOW}Skipped (binding file not found)${NC}"
fi

# JavaScript binding if available
if [ -f "$SCRIPT_DIR/simple/mirror_bridge_binding_js.cpp" ]; then
    echo -e "${GREEN}[4/4] Simple Class (JavaScript binding)${NC}"
    result=$(measure_compile_time "simple_js" \
        "$SCRIPT_DIR/simple/mirror_bridge_binding_js.cpp" \
        "$BUILD_DIR/simple_mb_js.node" \
        "-I$SCRIPT_DIR/simple -I/usr/include/node" 2>/dev/null) || result="0 0 0"
    read median stddev size <<< "$result"
    if [ "$median" -gt 0 ]; then
        BENCHMARK_NAMES+=("Simple (JS)")
        MEDIANS+=($median)
        STDDEVS+=($stddev)
        SIZES+=($size)
        echo -e "  Compile: $(format_time $median) ± $(format_time $stddev) | Size: ${size}KB"
    else
        echo -e "  ${YELLOW}Skipped (Node headers not found)${NC}"
    fi
else
    echo -e "${GREEN}[4/4] Simple Class (JavaScript binding)${NC}"
    echo -e "  ${YELLOW}Skipped (binding file not found)${NC}"
fi

echo ""

# Print summary table
echo -e "${BOLD}${BLUE}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║                    Summary                         ║${NC}"
echo -e "${BOLD}${BLUE}╚════════════════════════════════════════════════════╝${NC}"
echo ""

printf "${BOLD}%-20s %-15s %-12s${NC}\n" "Benchmark" "Compile Time" "Binary Size"
echo "────────────────────────────────────────────────────"

for i in "${!BENCHMARK_NAMES[@]}"; do
    name="${BENCHMARK_NAMES[$i]}"
    median="${MEDIANS[$i]}"
    size="${SIZES[$i]}"
    printf "%-20s %-15s %-12s\n" "$name" "$(format_time $median)" "${size}KB"
done

echo ""

# PCH comparison
if [ $USE_PCH -eq 1 ]; then
    echo -e "${CYAN}Note: Times above use precompiled header (PCH).${NC}"
    echo -e "${CYAN}First-time builds without PCH will be 3-6x slower.${NC}"
else
    echo -e "${CYAN}Tip: Use --pch for faster incremental builds (3-6x speedup).${NC}"
fi

echo ""

# Output JSON if requested
if [ -n "$OUTPUT_JSON" ]; then
    cat > "$OUTPUT_JSON" << EOF
{
  "benchmark": "mirror_bridge_compile_time",
  "timestamp": "$(date -Iseconds)",
  "pch_enabled": $USE_PCH,
  "runs": $RUNS,
  "results": [
EOF

    for i in "${!BENCHMARK_NAMES[@]}"; do
        name="${BENCHMARK_NAMES[$i]}"
        median="${MEDIANS[$i]}"
        stddev="${STDDEVS[$i]}"
        size="${SIZES[$i]}"
        comma=$([ $i -lt $((${#BENCHMARK_NAMES[@]} - 1)) ] && echo "," || echo "")
        cat >> "$OUTPUT_JSON" << EOF
    {
      "name": "$name",
      "compile_time_ms": $median,
      "stddev_ms": $stddev,
      "binary_size_kb": $size
    }$comma
EOF
    done

    cat >> "$OUTPUT_JSON" << EOF
  ]
}
EOF
    echo -e "${GREEN}Results saved to: $OUTPUT_JSON${NC}"
fi

echo -e "${GREEN}✓ Benchmark complete${NC}"
