#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Install Eigen if not present
if [ ! -f /usr/include/eigen3/Eigen/Core ]; then
    echo "Installing Eigen..."
    apt-get update -qq && apt-get install -y -qq libeigen3-dev 2>/dev/null
fi

CXX="clang++"
FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++ -fPIC -shared -O2"
FLAGS="$FLAGS -Wno-deprecated-declarations"  # Eigen has deprecation warnings with libc++
INCLUDES="-I/workspace -I/workspace/examples/open3d-port -I/usr/include/eigen3"
PYTHON_FLAGS=$(python3-config --includes)

echo "=============================================="
echo "Open3D Geometry Port — mirror_bridge"
echo "=============================================="
echo ""
echo "Compiling 4 geometry classes (PointCloud, LineSet,"
echo "AxisAlignedBoundingBox, TriangleMesh)..."
echo ""

$CXX $FLAGS $INCLUDES $PYTHON_FLAGS open3d_binding.cpp -o open3d_geometry.so 2>&1

if [ -f "open3d_geometry.so" ]; then
    echo "Build successful: open3d_geometry.so"
else
    echo "Build failed"
    exit 1
fi

echo ""
echo "Running tests..."
echo ""
python3 test_open3d.py

echo ""
echo "=============================================="
echo "Binding comparison:"
echo "  Open3D pybind11: ~1,915 lines across 4 files"
echo "  mirror_bridge:   4 bind_class<> calls"
echo "=============================================="
