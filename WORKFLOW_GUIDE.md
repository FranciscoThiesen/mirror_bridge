# Mirror Bridge - Complete Workflow Guide

> **Note**: This document has moved to [docs/guides/workflow.md](docs/guides/workflow.md).
> This file is kept for backward compatibility.

## The Ideal Setup: Auto-Discovery + PCH

This guide shows the **recommended workflow** for maximum productivity with Mirror Bridge:
- ✅ **Zero boilerplate code** (auto-discovery)
- ✅ **Fast compilation** (precompiled headers)
- ✅ **Simple workflow** (2-3 commands total)

---

## Quick Comparison: Mirror Bridge vs nanobind

### nanobind Workflow (Traditional Approach)

```cpp
// File: bindings.cpp (you must write this manually)
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include "my_classes.hpp"

namespace nb = nanobind;

NB_MODULE(my_module, m) {
    // Manual binding for SimpleClass
    nb::class_<SimpleClass>(m, "SimpleClass")
        .def(nb::init<>())
        .def(nb::init<int, double, std::string>())
        .def_rw("id", &SimpleClass::id)
        .def_rw("value", &SimpleClass::value)
        .def_rw("name", &SimpleClass::name)
        .def("set_id", &SimpleClass::set_id)
        .def("get_id", &SimpleClass::get_id)
        .def("set_value", &SimpleClass::set_value)
        .def("get_value", &SimpleClass::get_value)
        .def("compute", &SimpleClass::compute);

    // Manual binding for Vector3D (repeat for each class...)
    nb::class_<Vector3D>(m, "Vector3D")
        .def(nb::init<>())
        .def_rw("x", &Vector3D::x)
        .def_rw("y", &Vector3D::y)
        .def_rw("z", &Vector3D::z)
        .def("length", &Vector3D::length);

    // ...and so on for every class
}
```

**Lines of code**: ~18 per class × N classes

**Compilation**:
```bash
# One-time: Build nanobind stub library
clang++ -c nb_combined.cpp -o nb_stub.o  # ~800ms

# Per project: Compile bindings + link stub
clang++ bindings.cpp nb_stub.o -o my_module.so  # ~165-218ms
```

---

### Mirror Bridge Workflow (Auto-Discovery + PCH)

```cpp
// NO CODE TO WRITE!
// Just have your header files with your classes
```

**Lines of code**: **0** (literally zero)

**Compilation**:
```bash
# One-time setup (per build configuration): Create PCH
clang-21 -std=c++2c -freflection -freflection-latest -fPIC -stdlib=libc++ \
  -I/usr/include/python3.10 -O3 -DNDEBUG \
  -x c++-header mirror_bridge_pch.hpp -o mirror_bridge_pch.hpp.gch  # ~600ms

# Per project: Auto-discover and compile
mirror_bridge_auto src/ --module my_module --use-pch  # ~194-252ms
```

---

## Step-by-Step: Setting Up Auto-Discovery + PCH

### Step 1: One-Time PCH Creation

Create the precompiled header **once per build configuration** (Debug/Release):

```bash
# For Release builds (-O3)
clang-21 -std=c++2c -freflection -freflection-latest \
  -fPIC -stdlib=libc++ \
  -I/usr/include/python3.10 \
  -O3 -DNDEBUG \
  -x c++-header mirror_bridge_pch.hpp \
  -o build/mirror_bridge_pch_release.hpp.gch

# For Debug builds (-g)
clang-21 -std=c++2c -freflection -freflection-latest \
  -fPIC -stdlib=libc++ \
  -I/usr/include/python3.10 \
  -g -O0 \
  -x c++-header mirror_bridge_pch.hpp \
  -o build/mirror_bridge_pch_debug.hpp.gch
```

**Time cost**: ~600ms (done once, reused forever)

### Step 2: Auto-Discover and Build

Now build your module with a single command:

```bash
mirror_bridge_auto src/ --module my_module --use-pch
```

**What it does**:
1. Scans all `.hpp` and `.h` files in `src/`
2. Finds all `struct` and `class` definitions
3. Auto-generates binding code
4. Compiles with PCH (fast!)
5. Produces `build/my_module.so`

**Time cost**: ~194-252ms (3-6x faster than without PCH!)

### Step 3: Use in Python

```python
import my_module

# All classes automatically bound!
obj = my_module.SimpleClass()
obj.id = 42
obj.name = "Hello"
print(obj.get_id())  # 42

vec = my_module.Vector3D()
vec.x = 1.0
# ...all members and methods work automatically
```

---

## Advanced: Multi-Project Setup

If you have multiple binding modules, you can **share the PCH** across all of them:

```bash
# Create PCH once
clang-21 ... -x c++-header mirror_bridge_pch.hpp -o build/pch.gch

# Build multiple modules, all using the same PCH
mirror_bridge_auto project1/src/ --module project1 --use-pch build/pch.gch
mirror_bridge_auto project2/src/ --module project2 --use-pch build/pch.gch
mirror_bridge_auto project3/src/ --module project3 --use-pch build/pch.gch
```

The PCH cost (600ms) is **amortized across all projects**!

---

## CMake Integration (Automatic PCH)

For CMake users, PCH is handled automatically:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_project)

# Include Mirror Bridge PCH support
include(cmake/MirrorBridgePCH.cmake)

# Define your module
add_library(my_module MODULE)

# Auto-discover source files
file(GLOB_RECURSE HEADERS src/*.hpp)
target_sources(my_module PRIVATE ${HEADERS})

# Enable auto-discovery (CMake helper to be created)
# This will:
# 1. Scan headers
# 2. Generate binding file
# 3. Enable PCH automatically
# 4. Build the module
mirror_bridge_auto_module(my_module
    SOURCE_DIR src/
    MODULE_NAME my_module
)

# Or manually with PCH:
# mirror_bridge_enable_pch(my_module)
```

---

## Opt-Out: Skipping Classes

If you have internal classes you don't want bound, use comments:

```cpp
// This class will be auto-discovered and bound
struct PublicAPI {
    int value;
    void process();
};

// MIRROR_BRIDGE_SKIP
// This class will be ignored by auto-discovery
struct InternalHelper {
    void internal_stuff();
};
```

Or skip entire files:

```cpp
// MIRROR_BRIDGE_SKIP_FILE
// At the top of a header file to skip the entire file
```

---

## Performance Comparison

### Simple Project (1 class, ~10 methods)

| Approach | Code Lines | Compile Time | Total Effort |
|----------|-----------|--------------|--------------|
| **nanobind** | ~18 lines | 165ms | Manual coding + fast compile |
| **Mirror Bridge (no PCH)** | 1 line | 567ms | Minimal code + slow compile |
| **Mirror Bridge + PCH** | **0 lines** | **194ms** | **Zero code + fast compile** ✅ |

### Medium Project (10 classes, ~50 methods)

| Approach | Code Lines | Compile Time | Total Effort |
|----------|-----------|--------------|--------------|
| **nanobind** | ~103 lines | 218ms | Lots of manual coding |
| **Mirror Bridge (no PCH)** | 15 lines | 1695ms | Some code + very slow |
| **Mirror Bridge + PCH** | **0 lines** | **252ms** | **Zero code + fast compile** ✅ |

---

## When to Use PCH

| Scenario | Recommendation |
|----------|----------------|
| **Single small binding file** | Optional (minimal benefit) |
| **Multiple small files** | **Yes!** (share PCH across all) |
| **Medium-large project** | **Absolutely!** (6x speedup) |
| **Active development** | **Yes!** (faster iteration) |
| **CI/CD pipeline** | **Yes!** (faster builds) |
| **One-off script** | No (overhead not worth it) |

---

## Troubleshooting

### "Error: __OPTIMIZE__ predefined macro was disabled"

**Cause**: PCH built with different optimization flags

**Solution**: Rebuild PCH with matching flags:
```bash
# If building with -O3, PCH needs -O3
clang-21 ... -O3 -DNDEBUG -x c++-header mirror_bridge_pch.hpp -o pch.gch

# If building with -O0 (debug), PCH needs -O0
clang-21 ... -O0 -g -x c++-header mirror_bridge_pch.hpp -o pch_debug.gch
```

### "No classes found in src/"

**Causes**:
1. No `.hpp` or `.h` files in the directory
2. Classes defined inside namespaces (currently not auto-detected)
3. All classes marked with `MIRROR_BRIDGE_SKIP`

**Solutions**:
1. Ensure header files exist
2. Use manual mode for namespaced classes
3. Remove skip markers if unintentional

---

## Developer Experience Summary

### What You Write (nanobind):
```cpp
// bindings.cpp - 18 lines per class
nb::class_<MyClass>(m, "MyClass")
    .def(nb::init<>())
    .def_rw("field1", &MyClass::field1)
    .def_rw("field2", &MyClass::field2)
    // ...16 more lines
```

### What You Write (Mirror Bridge + auto-discovery):
```bash
# Literally nothing - just run the command
mirror_bridge_auto src/ --module my_module --use-pch
```

**Productivity gain**: ∞ (infinite - from N lines to 0 lines!)

**Compilation speed**: Within 15-20% of nanobind with PCH

**Developer happiness**: 📈📈📈

---

## Recommended Workflow for New Projects

```bash
# 1. Create your C++ classes normally
# src/my_class.hpp
struct MyClass {
    int id;
    std::string name;
    void process();
};

# 2. Build PCH once (when you first clone the project)
clang-21 ... -x c++-header mirror_bridge_pch.hpp -o build/pch.gch

# 3. Whenever you add/change classes, just rebuild:
mirror_bridge_auto src/ --module my_module --use-pch

# 4. Import in Python
python3 -c "import my_module; print(my_module.MyClass())"
```

**That's it!** No binding code to write, no binding code to maintain.

---

## The Bottom Line

**Mirror Bridge with auto-discovery + PCH gives you:**

✅ **Zero boilerplate** - No binding code to write
✅ **Fast compilation** - Within 15-20% of nanobind
✅ **Auto-sync** - Classes automatically available in Python
✅ **No maintenance** - Add classes, run command, done
✅ **Type safety** - Compile-time checking via reflection

**Trade-off:**
⚠️ Requires experimental C++26 compiler (Bloomberg clang-p2996)

For projects that can adopt C++26 reflection, this is the **most productive** Python binding workflow available.
