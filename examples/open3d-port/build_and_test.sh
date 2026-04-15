#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Install Eigen if not present
if [ ! -f /usr/include/eigen3/Eigen/Core ]; then
    echo "Installing Eigen..."
    apt-get update -qq && apt-get install -y -qq libeigen3-dev 2>/dev/null
fi

echo "=============================================="
echo "Open3D Geometry Port — mirror_bridge"
echo "=============================================="
echo ""

# Method 1: Use mirror_bridge generate (auto-discovery)
echo "Method 1: Auto-discovery via 'mirror_bridge generate'"
echo "$ mirror_bridge generate src/ --module open3d_geometry --lang python -v"
echo ""
/workspace/tools/mirror_bridge generate src/ --module open3d_geometry --lang python -v -I /usr/include/eigen3

echo ""
echo "$ python3 test_open3d.py"
python3 test_open3d.py

echo ""
echo "=============================================="
echo "Binding comparison:"
echo "  Open3D pybind11: 1,578 lines across 4 files"
echo "  mirror_bridge:   1 command (zero binding code)"
echo "=============================================="
