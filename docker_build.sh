#!/bin/bash
# Build and run the mirror_bridge Docker container with reflection compiler

set -e

echo "=== Mirror Bridge Docker Setup ==="
echo ""

# Build Docker image
echo "Building Docker image with clang-p2996..."
echo "Note: First build is ~20 minutes on a modern laptop (the bulk is"
echo "compiling clang-p2996 + libc++ from source). Subsequent rebuilds"
echo "hit the Docker layer cache and are nearly instant."
docker build -t mirror_bridge:latest .

echo ""
echo "=== Build Complete ==="
echo ""
echo "To run the container:"
echo "  docker run -v \$(pwd):/workspace -it mirror_bridge:latest"
echo ""
echo "Inside the container, you can:"
echo "  - Compile C++ code with reflection: clang++ -std=c++2c -freflection -freflection-latest"
echo "  - Build bindings: ./build_bindings.sh"
echo "  - Run tests: cd tests && python3 test_bindings.py"
