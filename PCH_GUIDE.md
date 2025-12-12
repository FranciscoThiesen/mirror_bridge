# Mirror Bridge - Precompiled Header Guide

> **Note**: This document has moved to [docs/guides/pch-optimization.md](docs/guides/pch-optimization.md).
> This file is kept for backward compatibility.

## TL;DR - Quick Start

**Want 66-84% faster compilation?** Use precompiled headers!

```bash
# Step 1: Build the PCH once (one-time setup)
clang-21 -std=c++2c -freflection -freflection-latest -fPIC -stdlib=libc++ \
  -I/usr/include/python3.10 -O3 -DNDEBUG \
  -x c++-header mirror_bridge_pch.hpp -o mirror_bridge_pch.hpp.gch

# Step 2: Use the PCH in your bindings
clang-21 -std=c++2c -freflection -freflection-latest -shared -fPIC -stdlib=libc++ \
  -I/usr/include/python3.10 -O3 -DNDEBUG \
  -include-pch mirror_bridge_pch.hpp.gch \
  your_bindings.cpp -o your_module.so
```

---

## Performance Impact

Measured on Bloomberg clang-p2996 with `-O3 -DNDEBUG`:

| Project Size | Without PCH | With PCH | Speedup |
|--------------|-------------|----------|---------|
| **Simple** (1 class, ~10 methods) | 567ms | 194ms | **2.9x faster** (66% reduction) |
| **Medium** (10 classes, ~50 methods) | 1580ms | 252ms | **6.3x faster** (84% reduction) |

**Key insight**: The larger your project, the bigger the PCH benefit!

---

## What is a Precompiled Header?

A precompiled header (PCH) is a pre-parsed version of header files that the compiler can reuse across multiple source files. Instead of reparsing `mirror_bridge.hpp`, `<Python.h>`, and `<meta>` every time, the compiler loads the pre-parsed version instantly.

**Benefits:**
- ✅ Dramatically faster compilation (2-6x speedup)
- ✅ Especially helpful for large projects
- ✅ No runtime cost - purely a compilation optimization
- ✅ Compatible with C++26 reflection

**Trade-offs:**
- ⚠️ Must rebuild PCH if you change compiler flags
- ⚠️ Need separate PCH for Debug vs Release builds
- ⚠️ PCH files are large (~16MB) but reusable across all binding files

---

## Manual Usage

### Step 1: Create the Precompiled Header

```bash
clang-21 -std=c++2c -freflection -freflection-latest -fPIC -stdlib=libc++ \
  -I/usr/include/python3.10 \
  -O3 -DNDEBUG \
  -x c++-header mirror_bridge_pch.hpp \
  -o build/mirror_bridge_pch.hpp.gch
```

**Important**: Use the **exact same flags** you'll use for your bindings!
- Same optimization level (`-O3`, `-O2`, `-g`, etc.)
- Same `-fPIC` if building shared libraries
- Same Python include path

### Step 2: Use the PCH in Your Bindings

```bash
clang-21 -std=c++2c -freflection -freflection-latest -shared -fPIC -stdlib=libc++ \
  -I/usr/include/python3.10 \
  -O3 -DNDEBUG \
  -include-pch build/mirror_bridge_pch.hpp.gch \
  my_bindings.cpp -o my_module.so
```

**Note**: Do NOT `#include "mirror_bridge.hpp"` in your source file when using `-include-pch`. The PCH already includes it.

---

## CMake Integration

For CMake users, we provide an easy helper:

```cmake
# In your CMakeLists.txt
include(cmake/MirrorBridgePCH.cmake)

add_library(my_bindings MODULE my_bindings.cpp)

# Enable PCH for this target
mirror_bridge_enable_pch(my_bindings)
```

This automatically:
1. Creates the PCH with correct flags
2. Configures your target to use it
3. Handles Debug vs Release configurations
4. Rebuilds PCH when `mirror_bridge.hpp` changes

---

## Troubleshooting

### Error: "predefined macro was disabled in precompiled file"

**Cause**: PCH was built with different optimization flags than your source file.

**Solution**: Rebuild PCH with matching flags:
```bash
# If compiling with -O0 (debug), PCH needs -O0 too
clang-21 ... -O0 -g -x c++-header mirror_bridge_pch.hpp -o pch_debug.gch

# If compiling with -O3 (release), PCH needs -O3
clang-21 ... -O3 -DNDEBUG -x c++-header mirror_bridge_pch.hpp -o pch_release.gch
```

### Error: "is pie differs in precompiled file"

**Cause**: PCH was built without `-fPIC` but you're building a shared library with `-fPIC`.

**Solution**: Add `-fPIC` to both PCH and source compilation.

### Error: "PCH file not found"

**Cause**: Wrong path to PCH file.

**Solution**: Use absolute or relative path to the `.gch` file:
```bash
-include-pch ./build/mirror_bridge_pch.hpp.gch  # Relative
-include-pch /absolute/path/to/mirror_bridge_pch.hpp.gch  # Absolute
```

---

## Best Practices

### 1. **One PCH per build configuration**

```bash
# Debug build
clang-21 ... -O0 -g -x c++-header mirror_bridge_pch.hpp -o pch_debug.gch

# Release build
clang-21 ... -O3 -DNDEBUG -x c++-header mirror_bridge_pch.hpp -o pch_release.gch
```

### 2. **Rebuild PCH when updating Mirror Bridge**

If you update `mirror_bridge.hpp`, rebuild the PCH:
```bash
rm build/*.gch
# Rebuild PCH with step 1 above
```

### 3. **Share PCH across multiple binding files**

```bash
# Build PCH once
clang-21 ... -x c++-header mirror_bridge_pch.hpp -o pch.gch

# Use in multiple files
clang-21 ... -include-pch pch.gch bindings1.cpp -o module1.so
clang-21 ... -include-pch pch.gch bindings2.cpp -o module2.so
clang-21 ... -include-pch pch.gch bindings3.cpp -o module3.so
```

This amortizes the PCH cost across all your binding files!

### 4. **PCH is optional**

You can always compile without PCH:
```cpp
#include "mirror_bridge.hpp"  // Works fine without PCH
```

PCH is purely an optional performance optimization.

---

## How It Works (Technical Details)

When you compile `mirror_bridge.hpp` normally:
1. Compiler parses `<meta>` (~100ms)
2. Compiler parses `<Python.h>` (~200ms)
3. Compiler parses `mirror_bridge.hpp` (~150ms)
4. **Total**: ~450ms of header parsing before your code

With PCH:
1. Compiler loads pre-parsed state (~20ms)
2. **Total**: ~20ms

**Savings**: 430ms per compilation unit!

The PCH contains:
- Parsed AST (Abstract Syntax Tree)
- Template definitions
- Macro expansions
- Symbol tables
- Reflection metadata

It's essentially a serialized snapshot of the compiler's internal state after processing the headers.

---

## Comparison to nanobind

**nanobind**: Uses a precompiled *library* (libnanobind.so)
- Compiles binding infrastructure once
- Links against it
- Cannot work with reflection (needs actual types at compile-time)

**Mirror Bridge**: Uses a precompiled *header* (PCH)
- Pre-parses headers once
- Includes them instantly
- Works perfectly with reflection!

We get similar benefits through a different mechanism that's compatible with compile-time introspection.

---

## Summary

**Should I use PCH?**

| Scenario | Recommendation |
|----------|---------------|
| Single small binding file | Optional (minimal benefit) |
| Multiple binding files | **Yes!** (share PCH across all) |
| Large project (10+ classes) | **Absolutely!** (6x speedup) |
| Rapid iteration during development | **Yes!** (faster rebuilds) |
| CI/CD builds | **Yes!** (faster pipelines) |

**PCH is free performance** - it's just a different way of compiling the exact same code.
