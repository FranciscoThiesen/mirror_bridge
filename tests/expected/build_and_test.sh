#!/bin/bash
# Build and test std::expected type conversion support
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=============================================="
echo "Building std::expected Test Module"
echo "=============================================="
echo

CXX="clang++"
COMMON_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++ -fPIC -shared -O2"
INCLUDES="-I/workspace -I/workspace/tests/expected"
PYTHON_FLAGS=$(python3-config --includes)

echo "Compiling expected_binding.cpp..."
$CXX $COMMON_FLAGS $INCLUDES $PYTHON_FLAGS \
    expected_binding.cpp -o expected_test.so 2>&1

if [ -f "expected_test.so" ]; then
    echo "  Build successful: expected_test.so"
else
    echo "  Build failed"
    exit 1
fi

echo
echo "=============================================="
echo "Running Tests"
echo "=============================================="
echo

python3 test_expected.py
TEST_RESULT=$?

if [ $TEST_RESULT -eq 0 ]; then
    echo
    echo "All std::expected tests passed!"
else
    echo
    echo "Some tests failed!"
    exit 1
fi
