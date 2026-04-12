#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CXX="clang++"
COMMON_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++ -fPIC -shared -O2"
INCLUDES="-I/workspace -I/workspace/tests/bulk_transfer"
PYTHON_FLAGS=$(python3-config --includes)

echo "Compiling bulk_binding.cpp..."
$CXX $COMMON_FLAGS $INCLUDES $PYTHON_FLAGS bulk_binding.cpp -o bulk_test.so 2>&1

echo "Running bulk transfer tests..."
python3 test_bulk.py
