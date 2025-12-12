# Quick Start (5 Minutes)

Get C++ performance in Python/JavaScript/Lua with zero binding code.

## Step 1: Get the Environment

```bash
git clone https://github.com/FranciscoThiesen/mirror_bridge
cd mirror_bridge
./start_dev_container.sh
```

**First time?** Choose option 1 to pull the pre-built image (~2 minutes) or option 2 to build from source (~60 minutes).

## Step 2: Verify Installation

```bash
cd /workspace
./tests/run_all_tests.sh
```

Expected output: `ALL TESTS PASSED!`

## Step 3: Try an Example

```bash
cd examples/01-hello-world

# Auto-generate Python bindings
mirror_bridge generate src/ --module greeter --lang python

# Test it
python3 test_greeter.py
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

**Generate binding (one command):**
```bash
mirror_bridge generate . --module my_calc --lang python
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

- **[First Binding Tutorial](first-binding.md)** - Step-by-step walkthrough
- **[CLI Reference](../reference/cli.md)** - All command options
- **[Examples](../../examples/)** - Progressive learning examples
- **[Workflow Guide](../guides/workflow.md)** - PCH optimization for faster builds

## Multi-Language Support

Mirror Bridge supports multiple languages with the same workflow:

```bash
# Python
mirror_bridge generate src/ --module my_module --lang python

# Lua
mirror_bridge generate src/ --module my_module --lang lua

# JavaScript (Node.js)
mirror_bridge generate src/ --module my_module --lang js

# All languages at once
mirror_bridge generate src/ --module my_module --lang all
```

See the [Multi-Language Guide](../guides/multi-language.md) for details.
