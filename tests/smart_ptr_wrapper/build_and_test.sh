#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CXX="clang++"
COMMON_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++ -fPIC -shared -O2"
INCLUDES="-I/workspace -I/workspace/tests/smart_ptr_wrapper"
PYTHON_FLAGS=$(python3-config --includes)

echo "Compiling smart_ptr_binding.cpp..."
$CXX $COMMON_FLAGS $INCLUDES $PYTHON_FLAGS smart_ptr_binding.cpp -o smart_ptr_test.so 2>&1

echo "Running smart pointer wrapper tests..."
python3 test_smart_ptr.py
