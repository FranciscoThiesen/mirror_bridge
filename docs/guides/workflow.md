# Development Workflow Guide

This guide shows the **recommended workflow** for maximum productivity with Mirror Bridge.

## The Ideal Setup: Auto-Discovery + PCH

The recommended workflow combines:
- **Zero boilerplate code** (auto-discovery)
- **Fast compilation** (precompiled headers)
- **Simple workflow** (2-3 commands total)

## Quick Comparison: Mirror Bridge vs Traditional Binding Libraries

### Traditional Approach (pybind11/nanobind)

```cpp
// bindings.cpp - you must write this manually (18+ lines per class)
#include <nanobind/nanobind.h>
#include "my_classes.hpp"

namespace nb = nanobind;

NB_MODULE(my_module, m) {
    nb::class_<MyClass>(m, "MyClass")
        .def(nb::init<>())
        .def(nb::init<int, double, std::string>())
        .def_rw("id", &MyClass::id)
        .def_rw("value", &MyClass::value)
        .def_rw("name", &MyClass::name)
        .def("compute", &MyClass::compute);
    // ...repeat for every class
}
```

### Mirror Bridge Approach

```cpp
// NO CODE TO WRITE!
// Just have your header files with your classes
```

**Lines of code**: 0 (literally zero)

## Step-by-Step Workflow

### Step 1: One-Time PCH Creation

Create the precompiled header **once per build configuration**:

```bash
# Build PCH (one-time setup, ~600ms)
mirror_bridge pch --output build/ --type release
```

This creates `build/mirror_bridge_pch.hpp.gch` which caches all Mirror Bridge infrastructure.

### Step 2: Auto-Discover and Build

Build your module with a single command:

```bash
mirror_bridge generate src/ --module my_module --lang python --pch
```

**What it does**:
1. Scans all `.hpp` and `.h` files in `src/`
2. Finds all `struct` and `class` definitions
3. Auto-generates binding code
4. Compiles with PCH (fast!)
5. Produces `build/my_module.so`

**Time cost**: ~194-252ms (3-6x faster than without PCH!)

### Step 3: Use in Your Language

**Python:**
```python
import my_module

obj = my_module.MyClass()
obj.value = 42
print(obj.compute())
```

**Lua:**
```lua
local my_module = require("my_module")

local obj = my_module.MyClass()
obj.value = 42
print(obj:compute())
```

**JavaScript:**
```javascript
const my_module = require('./build/my_module');

const obj = new my_module.MyClass();
obj.value = 42;
console.log(obj.compute());
```

## Multi-Project Setup

If you have multiple binding modules, share the PCH across all of them:

```bash
# Create PCH once
mirror_bridge pch --output build/ --type release

# Build multiple modules, all using the same PCH
mirror_bridge generate project1/src/ --module project1 --lang python --pch build/
mirror_bridge generate project2/src/ --module project2 --lang python --pch build/
mirror_bridge generate project3/src/ --module project3 --lang python --pch build/
```

The PCH cost (~600ms) is **amortized across all projects**!

## Opt-Out: Skipping Classes

If you have internal classes you don't want bound:

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

## Performance Comparison

### Simple Project (1 class, ~10 methods)

| Approach | Code Lines | Compile Time |
|----------|-----------|--------------|
| nanobind | ~18 lines | 165ms |
| Mirror Bridge (no PCH) | 0 lines | 567ms |
| **Mirror Bridge + PCH** | **0 lines** | **194ms** |

### Medium Project (10 classes, ~50 methods)

| Approach | Code Lines | Compile Time |
|----------|-----------|--------------|
| nanobind | ~103 lines | 218ms |
| Mirror Bridge (no PCH) | 0 lines | 1695ms |
| **Mirror Bridge + PCH** | **0 lines** | **252ms** |

## When to Use PCH

| Scenario | Use PCH? |
|----------|----------|
| Single small binding file | Optional |
| Multiple small files | **Yes** |
| Medium-large project | **Absolutely** |
| Active development | **Yes** |
| CI/CD pipeline | **Yes** |
| One-off script | No |

## Troubleshooting

### "Error: __OPTIMIZE__ predefined macro was disabled"

**Cause**: PCH built with different optimization flags

**Solution**: Rebuild PCH with matching flags:
```bash
# If building with -O3 (release), PCH needs release mode
mirror_bridge pch --output build/ --type release

# If building with -O0 (debug), PCH needs debug mode
mirror_bridge pch --output build/ --type debug
```

### "No classes found in src/"

**Causes**:
1. No `.hpp` or `.h` files in the directory
2. All classes marked with `MIRROR_BRIDGE_SKIP`

**Solutions**:
1. Ensure header files exist
2. Remove skip markers if unintentional

## Summary

**Mirror Bridge with auto-discovery + PCH gives you:**

- **Zero boilerplate** - No binding code to write
- **Fast compilation** - Within 15-20% of nanobind
- **Auto-sync** - Classes automatically available
- **No maintenance** - Add classes, run command, done
- **Type safety** - Compile-time checking via reflection
