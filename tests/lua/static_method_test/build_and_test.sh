#!/bin/bash
# Build and test the Lua Vec3 module
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Detect compiler
if command -v clang++ &> /dev/null && clang++ --version | grep -q "clang"; then
    CXX="clang++"
    CXX_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++"
elif command -v g++ &> /dev/null; then
    CXX="g++"
    CXX_FLAGS="-std=c++26 -freflection"
else
    echo "Error: No suitable C++ compiler found"
    exit 1
fi

# Get Lua flags
if pkg-config --exists lua5.4 2>/dev/null; then
    LUA_CFLAGS=$(pkg-config --cflags lua5.4)
    LUA_LIBS=$(pkg-config --libs lua5.4)
elif pkg-config --exists lua 2>/dev/null; then
    LUA_CFLAGS=$(pkg-config --cflags lua)
    LUA_LIBS=$(pkg-config --libs lua)
else
    # Fallback for systems without pkg-config
    LUA_CFLAGS="-I/usr/include/lua5.4"
    LUA_LIBS="-llua5.4"
fi

# Build the module
echo "Building vec3.so with $CXX..."
$CXX $CXX_FLAGS -fPIC -shared \
    -I"$SCRIPT_DIR/../../.." \
    $LUA_CFLAGS \
    vec3_binding.cpp \
    -o vec3.so \
    $LUA_LIBS

echo "✓ Built vec3.so"

# Run the test
echo ""
echo "Running Lua tests..."
lua5.4 test_vec3.lua

echo ""
echo "✓ All Lua tests passed"
