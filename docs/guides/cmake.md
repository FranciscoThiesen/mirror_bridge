# CMake Integration Guide

Mirror Bridge is a header-only library with first-class CMake support. There
are three ways to consume it, and all three deliver the same API: the
`mirror_bridge::mirror_bridge` target plus the `mirror_bridge_<lang>_module()`
helper functions.

## Consuming Mirror Bridge

### Option 1: add_subdirectory (vendored checkout)

```cmake
add_subdirectory(third_party/mirror_bridge)
```

### Option 2: FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(mirror_bridge
    GIT_REPOSITORY https://github.com/FranciscoThiesen/mirror_bridge
    GIT_TAG main
)
FetchContent_MakeAvailable(mirror_bridge)
```

### Option 3: find_package (installed)

```bash
# One-time install of the package
cmake -S mirror_bridge -B build
cmake --install build --prefix /opt/mirror_bridge
```

```cmake
find_package(mirror_bridge REQUIRED)
# if installed to a custom prefix:
#   cmake -Dmirror_bridge_DIR=/opt/mirror_bridge/lib/cmake/mirror_bridge ...
```

As a dependency, Mirror Bridge never pushes compile flags into your own
targets — the reflection flags travel only on the `mirror_bridge::mirror_bridge`
interface target and whatever you build with the helpers below.

## Creating binding modules

Write a small binding translation unit:

```cpp
// my_binding.cpp
#include "mirror_bridge.hpp"
#include "my_class.hpp"

MIRROR_BRIDGE_MODULE(my_module,
    mirror_bridge::bind_class<MyClass>(m, "MyClass");
)
```

Then declare one target per language:

```cmake
mirror_bridge_python_module(my_module my_binding.cpp)      # -> my_module.so
mirror_bridge_lua_module(my_module_lua my_binding_lua.cpp) # -> my_module_lua.so
mirror_bridge_js_module(my_module_js my_binding_js.cpp)    # -> my_module_js.node
```

All three helpers accept the same keyword arguments:

| Argument | Meaning |
|---|---|
| positional / `SOURCES` | The binding `.cpp` file(s) |
| `INCLUDE_DIRS` | Extra include directories (your library headers) |
| `OUTPUT_DIRECTORY` | Where to place the built module (default: current binary dir) |

Dependencies are found lazily: the Python helper runs `find_package(Python3)`
on first use, the Lua helper locates Lua via pkg-config or manual search, and
the JS helper looks for `node_api.h`. Missing dependencies are a configure
error only for the languages you actually request.

## Hand-rolled targets

If you prefer not to use the helpers, linking the interface target is all
that's required — it carries the reflection flags as usage requirements:

```cmake
Python3_add_library(my_module MODULE my_binding.cpp)
target_link_libraries(my_module PRIVATE mirror_bridge::mirror_bridge)
set_target_properties(my_module PROPERTIES PREFIX "")
```

## Compiler support

The flags are chosen per consumer compiler via generator expressions:

| Compiler | Flags applied |
|---|---|
| clang-p2996 | `-std=c++2c -freflection -freflection-latest -stdlib=libc++` |
| GCC 16+ | `-std=c++26 -freflection` |

Any other compiler gets a configure-time warning: there is no way to build
reflection-based bindings without one of these two.

## Precompiled headers

When building inside the Mirror Bridge repo (tests, examples), configure with
`-DMIRROR_BRIDGE_USE_PCH=ON` to cut binding compile times roughly 3-4x. The
helpers pick up the PCH automatically when it's enabled.

## See also

- [Installation](../getting-started/installation.md) — toolchain setup
- [Packaging guide](packaging.md) — shipping the generated modules
- [PCH guide](pch.md) — precompiled header details
