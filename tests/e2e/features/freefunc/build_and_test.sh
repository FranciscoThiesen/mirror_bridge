#!/bin/bash
# Build and test the free function binding feature
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Detect compiler
if command -v clang++ &> /dev/null && clang++ --version | grep -q "clang"; then
    CXX="clang++"
    CXX_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++"
else
    echo "Error: Clang with reflection support required"
    exit 1
fi

# Get Python flags
PYTHON_INCLUDES=$(python3-config --includes)
PYTHON_LDFLAGS=$(python3-config --ldflags --embed 2>/dev/null || python3-config --ldflags)

# Build the module
echo "Building freefunc_test.so..."
$CXX $CXX_FLAGS -fPIC -shared \
    -I"$SCRIPT_DIR/../../../.." \
    $PYTHON_INCLUDES \
    freefunc_binding.cpp \
    -o freefunc_test.so \
    $PYTHON_LDFLAGS

echo "✓ Built freefunc_test.so"

# Run the test
echo ""
echo "Running free function tests..."
python3 test_freefunc.py

echo ""
echo "✓ All free function tests passed"
