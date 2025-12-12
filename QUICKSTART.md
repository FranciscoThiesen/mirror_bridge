# Mirror Bridge - Quick Start (5 Minutes)

> **Note**: This document has moved to [docs/getting-started/quickstart.md](docs/getting-started/quickstart.md).
> This file is kept for backward compatibility.

Get C++ performance in Python/JavaScript/Lua with zero binding code.

## Step 1: Get the Environment

```bash
git clone https://github.com/FranciscoThiesen/mirror_bridge
cd mirror_bridge
./start_dev_container.sh
```

**First time?** Choose option 1 to pull the pre-built image (~2 minutes) or option 2 to build from source (~60 minutes).

## Step 2: Verify It Works

```bash
cd /workspace
./tests/run_all_tests.sh
```

Expected output: `✓ ALL TESTS PASSED! (12/12)`

## Step 3: Try an Example

```bash
cd examples/option2

# Look at the C++ code
cat src/calculator.hpp
cat src/vector3.hpp

# Auto-generate Python bindings
../../mirror_bridge_auto src/ --module math_module

# Test it
python3 test_option2.py
```

## Step 4: Create Your Own

**Write C++:**
```cpp
// my_class.hpp
struct Calculator {
    double value = 0.0;
    double add(double x) { return value += x; }
};
```

**Generate binding (one line):**
```bash
mirror_bridge_auto . --module my_calc
```

**Use from Python:**
```python
import sys; sys.path.insert(0, 'build')
import my_calc

calc = my_calc.Calculator()
calc.add(10)
print(calc.value)  # 10.0
```

## What Just Happened?

1. Mirror Bridge scanned your C++ headers using reflection
2. Discovered all classes, methods, and members automatically
3. Generated Python bindings at compile-time
4. No manual binding code needed!

## Next Steps

- **More examples**: See `examples/` directory
- **Multi-language**: Try JavaScript and Lua bindings
- **Advanced features**: Smart pointers, containers, inheritance
- **Performance**: Use `--use-pch` for 3-6x faster compilation

See [README.md](README.md) for full documentation.
