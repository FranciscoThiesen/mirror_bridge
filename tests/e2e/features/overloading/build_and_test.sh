#!/bin/bash
# Build and test the overloading feature
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
echo "Building overload.so..."
$CXX $CXX_FLAGS -fPIC -shared \
    -I"$SCRIPT_DIR/../../../.." \
    $PYTHON_INCLUDES \
    overload_binding.cpp \
    -o overload.so \
    $PYTHON_LDFLAGS

echo "✓ Built overload.so"

# Run the test
echo ""
echo "Running overloading tests..."
python3 test_overloading.py

echo ""
echo "✓ All overloading tests passed"
