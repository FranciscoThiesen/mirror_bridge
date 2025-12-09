#!/bin/bash
# Build Vec3 test binding
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$SCRIPT_DIR"

# Get Python include path
PYTHON_INCLUDE=$(python3 -c "import sysconfig; print(sysconfig.get_path('include'))")
PYTHON_LIB=$(python3 -c "import sysconfig; print(sysconfig.get_config_var('LIBDIR'))")
PYTHON_EXT_SUFFIX=$(python3 -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX'))")

echo "Building vec3_test module..."
echo "Python include: $PYTHON_INCLUDE"

clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -shared -fPIC \
    -I"$PROJECT_ROOT" \
    -I"$PYTHON_INCLUDE" \
    -L"$PYTHON_LIB" \
    -o "vec3_test${PYTHON_EXT_SUFFIX}" \
    vec3_binding.cpp \
    -undefined dynamic_lookup

echo "Build complete: vec3_test${PYTHON_EXT_SUFFIX}"
echo ""
echo "Run tests with: python3 test_vec3.py"
