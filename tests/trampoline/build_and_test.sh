#!/bin/bash
# Test that Python subclasses can override C++ virtual methods, with the
# override being called when C++ code invokes the virtual.
set -e
cd "$(dirname "$0")"

rm -rf build && mkdir build

clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -shared -fPIC -O2 \
    -I.. -I../../ -I../../core -I../../python \
    -I$(python3-config --prefix)/include/python3.10 \
    binding.cpp \
    -o build/trampoline_test.so

python3 test_trampoline.py
