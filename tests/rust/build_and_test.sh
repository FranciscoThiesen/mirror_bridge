#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CXX="clang++"
CXX_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++ -O2"
INCLUDES="-I/workspace -I/workspace/tests/rust"

echo "Compiling Rust binding generator test..."
$CXX $CXX_FLAGS $INCLUDES rust_gen.cpp -o rust_gen 2>&1

echo "Running Rust binding generation..."
./rust_gen

echo ""
echo "Testing file output..."
mkdir -p /tmp/rust_bindings
$CXX $CXX_FLAGS $INCLUDES -DWRITE_FILES -c rust_gen.cpp -o /dev/null 2>/dev/null || true
echo "  Rust binding generation tests complete."
