#!/bin/bash
# ============================================================================
# C++17 Codegen Integration Test
# ============================================================================
#
# This script tests the full codegen workflow:
# 1. Compile codegen driver with reflection compiler
# 2. Run to generate C++17 binding code
# 3. Compile generated code with standard g++ (C++17)
# 4. Run Python tests against the compiled module
#
# ============================================================================

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

echo -e "${BLUE}============================================${NC}"
echo -e "${BLUE}  C++17 Codegen Integration Test${NC}"
echo -e "${BLUE}============================================${NC}"
echo ""

# Create build directory
mkdir -p "$BUILD_DIR"

# Step 1: Compile the codegen driver with reflection compiler
echo -e "${YELLOW}[Step 1/4] Compiling codegen driver with reflection compiler...${NC}"

CODEGEN_BINARY="$BUILD_DIR/codegen_driver"

clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -I"$PROJECT_ROOT" \
    -I"$SCRIPT_DIR" \
    "$SCRIPT_DIR/codegen_driver.cpp" \
    -o "$CODEGEN_BINARY" 2>&1 | grep -v "mixture of designated\|warning.*__cpp_reflection" || true

if [ ! -x "$CODEGEN_BINARY" ]; then
    echo -e "${RED}Error: Failed to compile codegen driver${NC}"
    exit 1
fi

echo -e "${GREEN}  Compiled: $CODEGEN_BINARY${NC}"
echo ""

# Step 2: Run codegen to generate C++17 code
echo -e "${YELLOW}[Step 2/4] Generating C++17 binding code...${NC}"

GENERATED_CPP="$BUILD_DIR/sample_codegen_generated.cpp"
"$CODEGEN_BINARY" > "$GENERATED_CPP"

if [ ! -s "$GENERATED_CPP" ]; then
    echo -e "${RED}Error: Generated file is empty${NC}"
    exit 1
fi

LINE_COUNT=$(wc -l < "$GENERATED_CPP")
echo -e "${GREEN}  Generated: $GENERATED_CPP ($LINE_COUNT lines)${NC}"
echo ""

# Step 3: Compile the generated C++17 code with standard g++
echo -e "${YELLOW}[Step 3/4] Compiling generated code with g++ (C++17)...${NC}"

# First, we need to prepend the include for the sample class header
FINAL_CPP="$BUILD_DIR/sample_codegen_final.cpp"
cat > "$FINAL_CPP" << 'EOF'
// Include the original class definitions
// This would normally be done by the user
EOF

# Add the sample class header content directly (since we need the actual definitions)
cat "$SCRIPT_DIR/sample_class.hpp" >> "$FINAL_CPP"
echo "" >> "$FINAL_CPP"
echo "// Generated binding code follows:" >> "$FINAL_CPP"
# Skip the first few lines that contain "include your header" comments
tail -n +10 "$GENERATED_CPP" >> "$FINAL_CPP"

OUTPUT_SO="$BUILD_DIR/sample_codegen.so"

# Use g++ if available, otherwise fall back to clang++ with C++17
if command -v g++ &> /dev/null; then
    COMPILER="g++"
else
    COMPILER="clang++"
fi

# Compile with C++17 standard (NOT reflection flags!)
$COMPILER -std=c++17 -shared -fPIC \
    -I"$SCRIPT_DIR" \
    $(python3-config --includes) \
    "$FINAL_CPP" \
    -o "$OUTPUT_SO" \
    $(python3-config --ldflags --embed 2>/dev/null || python3-config --ldflags) \
    2>&1

if [ ! -f "$OUTPUT_SO" ]; then
    echo -e "${RED}Error: Failed to compile generated code with C++17${NC}"
    exit 1
fi

echo -e "${GREEN}  Compiled: $OUTPUT_SO${NC}"
echo -e "${GREEN}  Compiler: $COMPILER -std=c++17${NC}"
echo ""

# Step 4: Run Python tests
echo -e "${YELLOW}[Step 4/4] Running Python tests...${NC}"
echo ""

cd "$SCRIPT_DIR"
python3 test_codegen.py

echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  C++17 Codegen Integration Test PASSED!${NC}"
echo -e "${GREEN}============================================${NC}"
