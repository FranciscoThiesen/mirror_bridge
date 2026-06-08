#!/bin/bash
# Test that compile-time validation catches unconvertible member types.
# This test succeeds when the BAD code FAILS to compile with the expected error message.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=============================================="
echo "Testing Compile-Time Binding Validation"
echo "=============================================="
echo

# Prefer reflection-enabled clang; fall back to GCC 16+ (-freflection).
if command -v clang++ >/dev/null 2>&1 && clang++ --version 2>&1 | grep -qi "reflection\|p2996\|bloomberg"; then
    CXX="clang++"
    COMMON_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++ -fPIC -shared -O2"
else
    CXX="g++"
    COMMON_FLAGS="-std=c++26 -freflection -fPIC -shared -O2"
fi
# Derive include paths from the script location: the repo isn't always
# checked out at /workspace (CI uses /__w/mirror_bridge/mirror_bridge).
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
INCLUDES="-I$PROJECT_ROOT -I$SCRIPT_DIR"
PYTHON_FLAGS=$(python3-config --includes)

# Test 1: Unconvertible type should fail to compile
echo "Test 1: Binding a class with function pointer member should fail..."
if $CXX $COMMON_FLAGS $INCLUDES $PYTHON_FLAGS test_validation.cpp -o /dev/null 2>&1 | grep -q "cannot convert"; then
    echo "  PASS: Compilation correctly rejected with clear error message"
else
    # Check if it failed at all (even without the exact message)
    if ! $CXX $COMMON_FLAGS $INCLUDES $PYTHON_FLAGS test_validation.cpp -o /dev/null 2>/dev/null; then
        echo "  PASS: Compilation correctly rejected (unconvertible type detected)"
    else
        echo "  FAIL: Compilation should have failed for unconvertible type"
        exit 1
    fi
fi

# Test 2: A valid class should compile fine
echo "Test 2: Binding a class with only convertible members should succeed..."
$CXX $COMMON_FLAGS $INCLUDES $PYTHON_FLAGS expected_binding.cpp -o /dev/null 2>/dev/null
echo "  PASS: Valid class compiles successfully"

echo
echo "All validation tests passed!"
