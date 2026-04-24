#!/bin/bash
# Run inside the mirror_bridge docker image after building the Open3D fork.
# See README.md for prerequisites.
set -e
cd "$(dirname "$0")"
rm -rf build && mkdir build

O3D_SRC="${O3D_SRC:-../open3d-full-port/Open3D/cpp}"
O3D_BUILD="${O3D_BUILD:-/tmp/o3d_fork_build}"

clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -shared -fPIC -O2 \
    -I.. -I../.. -I../../core -I../../python \
    -I"$O3D_SRC" -I/usr/include/eigen3 \
    -I$(python3-config --prefix)/include/python3.10 \
    binding.cpp \
    -L"$O3D_BUILD/lib/Release" -lOpen3D \
    -Wl,-rpath,"$O3D_BUILD/lib/Release" \
    -o build/real_open3d.so

# LD_PRELOAD note: Open3D's vendored 3rdparty deps (curl, zmq, some
# VTK pieces) compile with libstdc++ even when we request libc++ for
# the top-level build — they don't honour CMAKE_CXX_FLAGS cleanly.
# The resulting libOpen3D.so carries unresolved libstdc++ symbols
# (std::__cxx11::*) that need to be satisfied at load time. libc++
# and libstdc++ use different symbol namespaces (std::__1:: vs
# std::__cxx11::), so they coexist peacefully in the same process
# — preloading libstdc++ simply gives the ELF loader a place to
# find the missing symbols without changing our binding's ABI.
LD_PRELOAD="${LD_PRELOAD:+$LD_PRELOAD:}libstdc++.so.6" python3 test_real_open3d.py
