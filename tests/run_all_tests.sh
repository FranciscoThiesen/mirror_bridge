#!/bin/bash
# Mirror Bridge - Automated Test Runner
# Builds all bindings and runs all tests in sequence

set -e  # Exit on first error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
TEST_DIR="$PROJECT_ROOT/tests"
BINDING_SCRIPT="$PROJECT_ROOT/build_bindings.sh"

# Set up environment for libc++ (needed for reflection support)
export LD_LIBRARY_PATH=/usr/local/lib/aarch64-unknown-linux-gnu:/usr/local/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH

# Set up Lua module path to find built .so files
# Check _lua suffix first (point2d_lua.so), then direct names (robot_lua.so)
# Order matters: _lua suffix first avoids loading Python .so files by mistake
export LUA_CPATH="$BUILD_DIR/?_lua.so;$BUILD_DIR/?.so;;"

# Counters
TOTAL_BINDINGS=0
BUILT_BINDINGS=0
FAILED_BINDINGS=0
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

echo -e "${BLUE}=======================================${NC}"
echo -e "${BLUE}  Mirror Bridge - Automated Test Suite${NC}"
echo -e "${BLUE}=======================================${NC}"
echo ""

# Detect which reflection-enabled compiler is available
if command -v clang++ &> /dev/null; then
    CXX_COMPILER="clang++"
    CXX_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++"
    echo -e "${BLUE}Using Clang compiler${NC}"
elif command -v g++ &> /dev/null; then
    CXX_COMPILER="g++"
    CXX_FLAGS="-std=c++26 -freflection"
    echo -e "${BLUE}Using GCC compiler${NC}"
else
    echo -e "${RED}Error: No C++ compiler found (clang++ or g++)${NC}"
    echo "This script must run inside a mirror_bridge Docker container."
    echo ""
    echo "To run tests:"
    echo "  ./start_dev_container.sh"
    echo "  cd /workspace"
    echo "  ./tests/run_all_tests.sh"
    exit 1
fi

# Check if compiler supports reflection (Clang-specific check)
if [[ "$CXX_COMPILER" == "clang++" ]]; then
    if ! clang++ --version 2>&1 | grep -q "reflection\|p2996\|bloomberg"; then
        echo -e "${YELLOW}Warning: Compiler may not support C++26 reflection${NC}"
        echo "This script requires a reflection-enabled compiler."
        echo ""
        read -p "Continue anyway? [y/N] " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
fi

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Step 1: Build all bindings
echo -e "${YELLOW}[STEP 1/5] Building all bindings...${NC}"
echo ""

cd "$TEST_DIR"

# Find all binding .cpp files recursively
while IFS= read -r -d '' binding_file; do
    [ -f "$binding_file" ] || continue

    TOTAL_BINDINGS=$((TOTAL_BINDINGS + 1))

    # Extract module name from file (remove _binding.cpp or just .cpp)
    module_name=$(basename "$binding_file" _binding.cpp)
    module_name=$(basename "$module_name" .cpp)

    # Get the directory of the binding file for proper includes
    binding_dir=$(dirname "$binding_file")

    # Determine binding type and set appropriate flags
    if [[ "$binding_file" == */js/* ]] || [[ "$binding_file" == *_js.cpp ]]; then
        # JavaScript binding
        # Only add _js suffix if not already present
        if [[ "$module_name" == *_js ]]; then
            output_name="${module_name}"
        else
            output_name="${module_name}_js"
        fi
        output_ext=".node"
        includes="-I/usr/include/node"
        echo -e "${BLUE}Building ${module_name} (JavaScript)...${NC}"
    elif [[ "$binding_file" == */lua/* ]] || [[ "$binding_file" == *_lua.cpp ]]; then
        # Lua binding - use _lua suffix to avoid collision with Python bindings
        # Only add _lua suffix if not already present
        if [[ "$module_name" == *_lua ]]; then
            output_name="${module_name}"
        else
            output_name="${module_name}_lua"
        fi
        output_ext=".so"
        includes="-I/usr/include/lua5.4 -llua5.4"
        echo -e "${BLUE}Building ${module_name} (Lua)...${NC}"
    else
        # Python binding (default)
        output_name="${module_name}"
        output_ext=".so"
        includes="$(python3-config --includes --ldflags)"
        echo -e "${BLUE}Building ${module_name} (Python)...${NC}"
    fi

    # Compile the binding - capture output
    # Add both project root and binding dir to include paths
    # Use || true to prevent set -e from exiting on compile failure
    compile_output=$(cd "$binding_dir" && $CXX_COMPILER $CXX_FLAGS \
        -I"$PROJECT_ROOT" -I. -fPIC -shared \
        $includes \
        "$(basename "$binding_file")" -o "$BUILD_DIR/${output_name}${output_ext}" 2>&1) || true

    # Check if output file was created (more reliable than exit code with || true)
    if [ -f "$BUILD_DIR/${output_name}${output_ext}" ]; then
        compile_exit=0
    else
        compile_exit=1
    fi

    # Filter out the designated initializer warning
    filtered_output=$(echo "$compile_output" | grep -v "mixture of designated" || true)

    # Check if compilation succeeded
    if [ $compile_exit -eq 0 ]; then
        echo -e "${GREEN}✓ Built: ${output_name}${output_ext}${NC}"
        BUILT_BINDINGS=$((BUILT_BINDINGS + 1))
    else
        echo -e "${RED}✗ Failed: ${module_name}${NC}"
        FAILED_BINDINGS=$((FAILED_BINDINGS + 1))
        # Show compilation errors
        if [ -n "$filtered_output" ]; then
            echo "$filtered_output" | sed 's/^/  /'
        fi
    fi
    echo ""
# Exclude v8/ directory - V8 tests are built separately in Step 2d
done < <(find . -name "*.cpp" -type f -not -path "*/v8/*" -print0)

# Step 2: Run all Python tests
echo -e "${YELLOW}[STEP 2/5] Running Python tests...${NC}"
echo ""

# Set PYTHONPATH to include build directory
export PYTHONPATH="$BUILD_DIR:$PYTHONPATH"

# Tests to skip (with reasons)
# Format: "path_pattern:reason" - pattern matches against the full path
SKIP_TESTS=(
    # All tests should now pass!
)

# Function to check if test should be skipped (uses path pattern matching)
should_skip_test() {
    local test_path=$1
    for skip_entry in "${SKIP_TESTS[@]}"; do
        local skip_pattern=$(echo "$skip_entry" | cut -d: -f1)
        if [[ "$test_path" == *"$skip_pattern"* ]]; then
            return 0  # Should skip
        fi
    done
    return 1  # Should not skip
}

# Function to get skip reason
get_skip_reason() {
    local test_path=$1
    for skip_entry in "${SKIP_TESTS[@]}"; do
        local skip_pattern=$(echo "$skip_entry" | cut -d: -f1)
        local skip_reason=$(echo "$skip_entry" | cut -d: -f2-)
        if [[ "$test_path" == *"$skip_pattern"* ]]; then
            echo "$skip_reason"
            return
        fi
    done
}

# Find all test_*.py files recursively in the test directory
while IFS= read -r -d '' test_file; do
    [ -f "$test_file" ] || continue

    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    test_name=$(basename "$test_file")

    # Check if test should be skipped (pass full path for pattern matching)
    if should_skip_test "$test_file"; then
        echo -e "${YELLOW}Skipping ${test_name}...${NC}"
        echo -e "${YELLOW}  Reason: $(get_skip_reason "$test_file")${NC}"
        echo ""
        continue
    fi

    echo -e "${BLUE}Running ${test_name}...${NC}"

    # Run the test from the test directory so relative imports work
    if (cd "$TEST_DIR" && python3 "$test_file") > /tmp/test_output.txt 2>&1; then
        echo -e "${GREEN}✓ Passed: ${test_name}${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))

        # Show test output if verbose
        if [ -n "$VERBOSE" ]; then
            cat /tmp/test_output.txt | sed 's/^/  /'
        fi
    else
        echo -e "${RED}✗ Failed: ${test_name}${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))

        # Always show failed test output
        echo -e "${RED}Error output:${NC}"
        cat /tmp/test_output.txt | sed 's/^/  /'
    fi
    echo ""
done < <(find "$TEST_DIR" -name "test_*.py" -type f -print0)

# Step 2b: Run JavaScript tests (Node.js)
echo -e "${YELLOW}[STEP 2b/5] Running JavaScript tests (Node.js)...${NC}"
echo ""

while IFS= read -r -d '' test_file; do
    [ -f "$test_file" ] || continue

    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    test_name=$(basename "$test_file")

    echo -e "${BLUE}Running ${test_name}...${NC}"

    # Run the test from the test directory
    if (cd "$(dirname "$test_file")" && node "$(basename "$test_file")") > /tmp/test_output.txt 2>&1; then
        echo -e "${GREEN}✓ Passed: ${test_name}${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}✗ Failed: ${test_name}${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        cat /tmp/test_output.txt | sed 's/^/  /'
    fi
    echo ""
done < <(find "$TEST_DIR/js" "$TEST_DIR/single_header_test" -name "test_*.js" -type f -print0 2>/dev/null)

# Step 2c: Run Lua tests
echo -e "${YELLOW}[STEP 2c/5] Running Lua tests...${NC}"
echo ""

while IFS= read -r -d '' test_file; do
    [ -f "$test_file" ] || continue

    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    test_name=$(basename "$test_file")

    echo -e "${BLUE}Running ${test_name}...${NC}"

    # Run the test from the test directory
    if (cd "$(dirname "$test_file")" && lua5.4 "$(basename "$test_file")") > /tmp/test_output.txt 2>&1; then
        echo -e "${GREEN}✓ Passed: ${test_name}${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}✗ Failed: ${test_name}${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        cat /tmp/test_output.txt | sed 's/^/  /'
    fi
    echo ""
done < <(find "$TEST_DIR/lua" "$TEST_DIR/single_header_test" -name "test_*.lua" -type f -print0 2>/dev/null)

# Step 2d: Build and run V8 tests (if libv8-dev is installed)
echo -e "${YELLOW}[STEP 2d/5] Running V8 tests (embedded V8)...${NC}"
echo ""

# Check if V8 development files are available
if [ -f "/usr/include/v8.h" ] || [ -f "/usr/include/libv8/v8.h" ] || pkg-config --exists v8 2>/dev/null; then
    # Find V8 compile flags
    if pkg-config --exists v8 2>/dev/null; then
        V8_CFLAGS=$(pkg-config --cflags v8)
        V8_LIBS=$(pkg-config --libs v8)
    else
        # Fallback for Ubuntu's libv8-dev
        V8_CFLAGS="-I/usr/include"
        V8_LIBS="-lv8 -lv8_libplatform"
    fi

    # Find all V8 test main.cpp files
    while IFS= read -r -d '' main_file; do
        [ -f "$main_file" ] || continue

        test_dir=$(dirname "$main_file")
        test_name=$(basename "$test_dir")

        # Check if test.js exists in the same directory
        if [ ! -f "$test_dir/test.js" ]; then
            echo -e "${YELLOW}Skipping $test_name (no test.js)${NC}"
            continue
        fi

        TOTAL_TESTS=$((TOTAL_TESTS + 1))

        echo -e "${BLUE}Building V8 test: ${test_name}...${NC}"

        # Compile the V8 test executable
        executable="$BUILD_DIR/v8_test_${test_name}"
        compile_output=$(cd "$test_dir" && $CXX_COMPILER $CXX_FLAGS \
            -I"$PROJECT_ROOT" -I. \
            $V8_CFLAGS \
            main.cpp \
            -o "$executable" \
            $V8_LIBS 2>&1) || true

        if [ -f "$executable" ]; then
            echo -e "${BLUE}Running V8 test: ${test_name}...${NC}"

            # Run the test from the test directory so test.js is found
            if (cd "$test_dir" && "$executable" test.js) > /tmp/test_output.txt 2>&1; then
                echo -e "${GREEN}✓ Passed: ${test_name} (V8)${NC}"
                PASSED_TESTS=$((PASSED_TESTS + 1))
            else
                echo -e "${RED}✗ Failed: ${test_name} (V8)${NC}"
                FAILED_TESTS=$((FAILED_TESTS + 1))
                cat /tmp/test_output.txt | sed 's/^/  /'
            fi
        else
            echo -e "${RED}✗ Failed to build: ${test_name} (V8)${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            echo "$compile_output" | sed 's/^/  /'
        fi
        echo ""
    done < <(find "$TEST_DIR/v8" -name "main.cpp" -type f -print0 2>/dev/null)
else
    echo -e "${YELLOW}Skipping V8 tests (libv8-dev not installed)${NC}"
    echo -e "${YELLOW}To enable V8 tests, install: apt-get install libv8-dev${NC}"
    echo ""
fi

# Step 3: Run shell-based tests (auto-discovery, config-file)
echo -e "${YELLOW}[STEP 3/5] Running CLI tool tests...${NC}"
echo ""

# Find all test_*.sh files
while IFS= read -r -d '' test_script; do
    [ -f "$test_script" ] || continue

    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    test_name=$(basename "$test_script")

    echo -e "${BLUE}Running ${test_name}...${NC}"

    # Run the shell test script
    if bash "$test_script" > /tmp/test_output.txt 2>&1; then
        echo -e "${GREEN}✓ Passed: ${test_name}${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))

        # Show test output if verbose
        if [ -n "$VERBOSE" ]; then
            cat /tmp/test_output.txt | sed 's/^/  /'
        fi
    else
        echo -e "${RED}✗ Failed: ${test_name}${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))

        # Always show failed test output
        echo -e "${RED}Error output:${NC}"
        cat /tmp/test_output.txt | sed 's/^/  /'
    fi
    echo ""
done < <(find "$TEST_DIR" -name "test_*.sh" -type f -print0)

# Summary
echo -e "${BLUE}=======================================${NC}"
echo -e "${BLUE}  Test Summary${NC}"
echo -e "${BLUE}=======================================${NC}"
echo ""

echo -e "${YELLOW}Bindings:${NC}"
echo -e "  Total:   $TOTAL_BINDINGS"
echo -e "  ${GREEN}Built:   $BUILT_BINDINGS${NC}"
if [ $FAILED_BINDINGS -gt 0 ]; then
    echo -e "  ${RED}Failed:  $FAILED_BINDINGS${NC}"
fi
echo ""

echo -e "${YELLOW}Tests:${NC}"
echo -e "  Total:   $TOTAL_TESTS"
echo -e "  ${GREEN}Passed:  $PASSED_TESTS${NC}"
if [ $FAILED_TESTS -gt 0 ]; then
    echo -e "  ${RED}Failed:  $FAILED_TESTS${NC}"
fi
echo ""

# Exit status
if [ $FAILED_BINDINGS -eq 0 ] && [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}✓ ALL TESTS PASSED!${NC}"
    exit 0
else
    echo -e "${RED}✗ SOME TESTS FAILED${NC}"
    exit 1
fi
