#!/bin/bash
# Build and test the static constexpr feature
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
echo "Building constexpr.so..."
$CXX $CXX_FLAGS -fPIC -shared \
    -I"$SCRIPT_DIR/../../../.." \
    $PYTHON_INCLUDES \
    constexpr_binding.cpp \
    -o constexpr.so \
    $PYTHON_LDFLAGS

echo "✓ Built constexpr.so"

# Run the test
echo ""
echo "Running constexpr tests..."
python3 test_constexpr.py

echo ""
echo "✓ All constexpr tests passed"
