#!/bin/bash
# Build and test new Mirror Bridge features
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=============================================="
echo "Building New Features Test Module"
echo "=============================================="
echo

# Compiler settings
CXX="clang++"
COMMON_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++ -fPIC -shared -O2"
INCLUDES="-I/workspace -I/workspace/tests/test_new_features"
PYTHON_FLAGS=$(python3-config --includes)

# Build
echo "Compiling test_features_binding.cpp..."
$CXX $COMMON_FLAGS $INCLUDES $PYTHON_FLAGS \
    test_features_binding.cpp -o test_features.so 2>&1

if [ -f "test_features.so" ]; then
    echo "✓ Build successful: test_features.so"
else
    echo "✗ Build failed"
    exit 1
fi

echo
echo "=============================================="
echo "Running Tests"
echo "=============================================="
echo

# Run Python tests
python3 test_features.py
TEST_RESULT=$?

if [ $TEST_RESULT -eq 0 ]; then
    echo
    echo "All tests passed!"
else
    echo
    echo "Some tests failed!"
    exit 1
fi
