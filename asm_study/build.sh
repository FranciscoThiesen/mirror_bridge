#!/bin/bash
# Compile both the mirror_bridge and pybind11 bindings for demo::PointCloud
# to -O3 assembly. Designed to run inside the mirror_bridge:latest Docker image.
set -e
cd "$(dirname "$0")"

pip install pybind11 --quiet
PYBIND_INC=$(python3 -c 'import pybind11; print(pybind11.get_include())')
PY_INC=$(python3-config --prefix)/include/python3.10

echo "=== pybind11: bind_pybind.cpp → bind_pybind.s ==="
clang++ -std=c++17 -stdlib=libc++ -O3 -fPIC -S \
    -I${PYBIND_INC} -I${PY_INC} -I/usr/include/eigen3 \
    bind_pybind.cpp -o bind_pybind.s
wc -l bind_pybind.s

echo "=== mirror_bridge: bind_mb.cpp → bind_mb.s ==="
clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ -O3 -fPIC -S \
    -I/workspace -I/workspace/core -I/workspace/python \
    -I/usr/include/eigen3 -I${PY_INC} \
    bind_mb.cpp -o bind_mb.s
wc -l bind_mb.s

echo
echo "Hot-path dispatcher symbols:"
echo "  mirror_bridge  — $(grep -c 'invoke_with_n_args' bind_mb.s) references"
echo "  pybind11       — $(grep -c 'cpp_function.*initialize' bind_pybind.s) references"
echo
echo "Separately-emitted get_center symbol:"
echo "  mirror_bridge  — $(grep -c 'PointCloud.*get_centerEv' bind_mb.s)"
echo "  pybind11       — $(grep -c 'PointCloud.*get_centerEv' bind_pybind.s)"
