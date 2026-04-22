#!/bin/bash
# Fair binding-layer comparison: build real .so modules for both pybind11 and
# mirror_bridge using IDENTICAL flags (-O3 -stdlib=libc++ -fPIC -shared),
# the SAME clang (clang-p2996 in mirror_bridge:benchmarks-fast), and the SAME
# underlying C++ (demo::PointCloud in geometry_min.hpp).
#
# The only differences between pc_pybind.so and pc_mb.so should be the binding
# layer itself — no compiler, no libc++/libstdc++, no OpenMP variability.
set -e
cd "$(dirname "$0")"

pip install pybind11 --quiet
PYBIND_INC=$(python3 -c 'import pybind11; print(pybind11.get_include())')
PY_PREFIX=$(python3-config --prefix)
PY_INC=${PY_PREFIX}/include/python3.10
PY_LDFLAGS=$(python3-config --ldflags)

# Embed rpath to the libc++ shipped with clang-p2996 so the resulting .so files
# can find libc++.so.1 at import time without needing LD_LIBRARY_PATH.
LIBCXX_DIR=$(dirname "$(clang++ -stdlib=libc++ --print-file-name=libc++.so)")
COMMON_FLAGS="-O3 -stdlib=libc++ -fPIC -shared -I/usr/include/eigen3 -I${PY_INC} -Wl,-rpath,${LIBCXX_DIR}"

echo "=== pybind11: bind_pybind.cpp -> pc_pybind.so ==="
clang++ -std=c++17 ${COMMON_FLAGS} \
    -I${PYBIND_INC} \
    bind_pybind.cpp -o pc_pybind.so ${PY_LDFLAGS}
ls -l pc_pybind.so

echo "=== mirror_bridge: bind_mb.cpp -> pc_mb.so ==="
clang++ -std=c++2c -freflection -freflection-latest ${COMMON_FLAGS} \
    -I/workspace -I/workspace/core -I/workspace/python \
    bind_mb.cpp -o pc_mb.so ${PY_LDFLAGS}
ls -l pc_mb.so

echo
echo "Both .so modules built with identical flags:"
echo "  ${COMMON_FLAGS}"
echo "Clang: $(clang++ --version | head -1)"
