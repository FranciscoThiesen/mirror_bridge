#!/bin/bash
# Build the Vec3 test module for Python
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Detect compiler
if command -v clang++ &> /dev/null && clang++ --version | grep -q "clang"; then
    CXX="clang++"
    CXX_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++"
elif command -v g++ &> /dev/null; then
    CXX="g++"
    CXX_FLAGS="-std=c++26 -freflection"
else
    echo "Error: No suitable C++ compiler found"
    exit 1
fi

# Get Python flags
PYTHON_INCLUDES=$(python3-config --includes)
PYTHON_LDFLAGS=$(python3-config --ldflags --embed 2>/dev/null || python3-config --ldflags)

# Build the module
echo "Building vec3_test.so with $CXX..."
$CXX $CXX_FLAGS -fPIC -shared \
    -I"$SCRIPT_DIR/../.." \
    $PYTHON_INCLUDES \
    vec3_binding.cpp \
    -o vec3_test.so \
    $PYTHON_LDFLAGS

echo "✓ Built vec3_test.so"
