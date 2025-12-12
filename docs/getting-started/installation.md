# Installation Guide

Mirror Bridge requires a C++26 compiler with reflection support. The easiest way to get started is using Docker.

## Docker Setup (Recommended)

We provide a Docker environment with everything pre-configured.

### Quick Start

```bash
git clone https://github.com/FranciscoThiesen/mirror_bridge
cd mirror_bridge
./start_dev_container.sh
```

Choose an option:
1. **Pull pre-built image** (~2 minutes) - Recommended for most users
2. **Build from source** (~60 minutes) - For customization or offline use

### What's Included

The Docker container includes:
- Bloomberg clang-p2996 with C++26 reflection support
- libc++ with reflection enabled (`<meta>` header)
- Python 3.10+ development headers
- Lua 5.4 development headers
- Node.js with N-API headers
- All your changes preserved between sessions

### Container Management

```bash
# Start/attach to container
./start_dev_container.sh

# Stop container (preserves state)
docker stop mirror_bridge_dev

# Remove container (lose state)
docker rm mirror_bridge_dev

# Remove image (forces re-download/rebuild)
docker rmi mirror_bridge:latest
```

### Persistent Storage

| Location | Persisted? |
|----------|-----------|
| `/workspace` (source code) | Yes (volume mount) |
| Compiled bindings | Yes |
| Installed packages | Yes |
| Shell history | Yes |
| `/tmp` contents | No |
| Running processes | No |

## Manual Installation

If you need to build without Docker, you'll need Bloomberg's clang-p2996.

### Prerequisites

- CMake 3.16+
- Python 3.7+ development headers
- Lua 5.4 development headers (optional)
- Node.js 14+ with N-API (optional)

### Build clang-p2996

```bash
git clone https://github.com/bloomberg/clang-p2996
cd clang-p2996
mkdir build && cd build
cmake -G Ninja \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
    -DCMAKE_BUILD_TYPE=Release \
    ../llvm
ninja clang
ninja cxx cxxabi
```

### Configure Environment

```bash
export CXX=/path/to/clang-p2996/build/bin/clang++
export LD_LIBRARY_PATH=/path/to/clang-p2996/build/lib:$LD_LIBRARY_PATH
```

### Verify Installation

```bash
cd mirror_bridge
./tests/run_all_tests.sh
```

## CMake Integration

For projects using CMake, Mirror Bridge provides helper functions:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_project)

# Add Mirror Bridge
add_subdirectory(mirror_bridge)

# Create a Python module
mirror_bridge_python_module(my_module
    SOURCES src/my_class.hpp
)

# Create a Lua module
mirror_bridge_lua_module(my_module_lua
    SOURCES src/my_class.hpp
)
```

See the [CMake Integration Guide](../guides/cmake.md) for full details.

## Troubleshooting

### "libc++.so.1: cannot open shared object file"

The libc++ library path isn't set. Add to your environment:

```bash
export LD_LIBRARY_PATH=/usr/local/lib/aarch64-unknown-linux-gnu:$LD_LIBRARY_PATH
```

The Docker container and test scripts set this automatically.

### "error: no member named 'meta' in namespace 'std'"

Your compiler doesn't have reflection support. Ensure you're using Bloomberg's clang-p2996.

### "reflection support is experimental"

This warning is expected. The P2996 proposal is still being standardized.

## Next Steps

- **[Quick Start](quickstart.md)** - Get running in 5 minutes
- **[First Binding](first-binding.md)** - Create your first binding
