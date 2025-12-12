# Precompiled Header (PCH) Guide

Want **66-84% faster compilation**? Use precompiled headers!

## TL;DR - Quick Start

```bash
# Step 1: Build the PCH once (one-time setup)
mirror_bridge pch --output build/ --type release

# Step 2: Use the PCH in your bindings
mirror_bridge generate src/ --module my_module --lang python --pch build/
```

## Performance Impact

Measured on Bloomberg clang-p2996 with `-O3 -DNDEBUG`:

| Project Size | Without PCH | With PCH | Speedup |
|--------------|-------------|----------|---------|
| Simple (1 class) | 567ms | 194ms | **2.9x faster** |
| Medium (10 classes) | 1580ms | 252ms | **6.3x faster** |

**Key insight**: The larger your project, the bigger the PCH benefit!

## What is a Precompiled Header?

A precompiled header (PCH) is a pre-parsed version of header files that the compiler can reuse across multiple source files. Instead of reparsing `mirror_bridge.hpp`, `<Python.h>`, and `<meta>` every time, the compiler loads the pre-parsed version instantly.

**Benefits:**
- Dramatically faster compilation (2-6x speedup)
- Especially helpful for large projects
- No runtime cost - purely a compilation optimization
- Compatible with C++26 reflection

**Trade-offs:**
- Must rebuild PCH if you change compiler flags
- Need separate PCH for Debug vs Release builds
- PCH files are large (~16MB) but reusable

## Using the CLI

### Build PCH

```bash
# Release build
mirror_bridge pch --output build/ --type release

# Debug build
mirror_bridge pch --output build/ --type debug
```

### Generate with PCH

```bash
# Auto-detect PCH in build/
mirror_bridge generate src/ --module my_module --lang python --pch

# Specify PCH path explicitly
mirror_bridge generate src/ --module my_module --lang python --pch /path/to/pch.gch
```

## Manual Usage

### Step 1: Create the PCH

```bash
clang++ -std=c++2c -freflection -freflection-latest -fPIC -stdlib=libc++ \
  -I/usr/include/python3.10 \
  -O3 -DNDEBUG \
  -x c++-header mirror_bridge_pch.hpp \
  -o build/mirror_bridge_pch.hpp.gch
```

**Important**: Use the **exact same flags** you'll use for your bindings!

### Step 2: Use the PCH

```bash
clang++ -std=c++2c -freflection -freflection-latest -shared -fPIC -stdlib=libc++ \
  -I/usr/include/python3.10 \
  -O3 -DNDEBUG \
  -include-pch build/mirror_bridge_pch.hpp.gch \
  my_bindings.cpp -o my_module.so
```

**Note**: Do NOT `#include "mirror_bridge.hpp"` in your source file when using `-include-pch`. The PCH already includes it.

## Best Practices

### 1. One PCH per build configuration

```bash
# Debug
mirror_bridge pch --output build/ --type debug

# Release
mirror_bridge pch --output build/ --type release
```

### 2. Rebuild PCH when updating Mirror Bridge

If you update `mirror_bridge.hpp`, rebuild the PCH:

```bash
rm build/*.gch
mirror_bridge pch --output build/ --type release
```

### 3. Share PCH across multiple modules

```bash
# Build PCH once
mirror_bridge pch --output build/ --type release

# Use in multiple modules
mirror_bridge generate mod1/src/ --module mod1 --lang python --pch build/
mirror_bridge generate mod2/src/ --module mod2 --lang python --pch build/
mirror_bridge generate mod3/src/ --module mod3 --lang python --pch build/
```

### 4. PCH is optional

You can always compile without PCH - it's purely a performance optimization.

## Troubleshooting

### Error: "predefined macro was disabled in precompiled file"

**Cause**: PCH was built with different optimization flags than your source file.

**Solution**: Rebuild PCH with matching flags:
```bash
# If compiling with -O0 (debug), PCH needs debug mode
mirror_bridge pch --output build/ --type debug

# If compiling with -O3 (release), PCH needs release mode
mirror_bridge pch --output build/ --type release
```

### Error: "is pie differs in precompiled file"

**Cause**: PCH was built without `-fPIC` but you're building a shared library.

**Solution**: The CLI handles this automatically. For manual builds, ensure both use `-fPIC`.

### Error: "PCH file not found"

**Cause**: Wrong path to PCH file.

**Solution**: Check the path or use `--pch` without arguments to auto-detect:
```bash
mirror_bridge generate src/ --module my_module --lang python --pch
```

## How It Works

When you compile normally:
1. Compiler parses `<meta>` (~100ms)
2. Compiler parses `<Python.h>` (~200ms)
3. Compiler parses `mirror_bridge.hpp` (~150ms)
4. **Total**: ~450ms of header parsing

With PCH:
1. Compiler loads pre-parsed state (~20ms)
2. **Total**: ~20ms

**Savings**: 430ms per compilation unit!

## When to Use PCH

| Scenario | Recommendation |
|----------|---------------|
| Single small binding file | Optional |
| Multiple binding files | **Yes** |
| Large project (10+ classes) | **Absolutely** |
| Rapid iteration during development | **Yes** |
| CI/CD builds | **Yes** |

**PCH is free performance** - it's just a different way of compiling the exact same code.
