#!/bin/bash
# Test auto-discovery of free functions
# This test verifies that mirror_bridge_auto correctly discovers
# and binds free functions in namespaces where classes are found.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Clean previous build
rm -rf "$SCRIPT_DIR/build"

# Run auto-discovery (redirect output to suppress noise)
"$PROJECT_ROOT/mirror_bridge_auto" "$SCRIPT_DIR" --module math_funcs -o "$SCRIPT_DIR/build" > /dev/null 2>&1

# Verify the module was built
if [ ! -f "$SCRIPT_DIR/build/math_funcs.so" ]; then
    echo "Error: math_funcs.so was not built"
    exit 1
fi

# Run Python tests
cd "$SCRIPT_DIR"
python3 test_funcs.py

echo "✓ Auto-discovery free function tests passed"
