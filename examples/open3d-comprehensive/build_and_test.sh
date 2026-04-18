#!/bin/bash
set -e
cd "$(dirname "$0")"
rm -rf build && mkdir build

clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -shared -fPIC -O0 \
    -I.. -I../../ -I../../core -I../../python \
    -I/usr/include/eigen3 \
    -I$(python3-config --prefix)/include/python3.10 \
    binding.cpp \
    -o build/open3d_comprehensive.so

python3 test_comprehensive.py
