#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CXX="clang++"
CXX_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++ -fPIC -shared -O2"
INCLUDES="-I/workspace -I/workspace/tests/lua/exception_test"

if pkg-config --exists lua5.4 2>/dev/null; then
    LUA_CFLAGS=$(pkg-config --cflags lua5.4)
    LUA_LIBS=$(pkg-config --libs lua5.4)
elif pkg-config --exists lua 2>/dev/null; then
    LUA_CFLAGS=$(pkg-config --cflags lua)
    LUA_LIBS=$(pkg-config --libs lua)
else
    LUA_CFLAGS="-I/usr/include/lua5.4"
    LUA_LIBS="-llua5.4"
fi

echo "Building exception_test.so..."
$CXX $CXX_FLAGS $INCLUDES $LUA_CFLAGS exception_binding.cpp -o exception_test.so $LUA_LIBS

echo "Running Lua exception tests..."
lua5.4 test_exceptions.lua
