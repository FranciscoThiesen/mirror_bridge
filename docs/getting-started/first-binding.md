# Your First Binding

This tutorial walks you through creating your first C++ to Python binding with Mirror Bridge.

## Goal

We'll create a simple `Greeter` class in C++ and use it from Python.

## Step 1: Write Your C++ Class

Create a file `greeter.hpp`:

```cpp
#pragma once
#include <string>

struct Greeter {
    std::string name = "World";
    int greeting_count = 0;

    std::string greet() {
        greeting_count++;
        return "Hello, " + name + "!";
    }

    void reset() {
        greeting_count = 0;
    }
};
```

That's it! No special macros, no registration code, just plain C++.

## Step 2: Generate Bindings

Run the Mirror Bridge CLI:

```bash
mirror_bridge generate . --module greeter --lang python
```

This command:
1. Scans all `.hpp` and `.h` files in the current directory
2. Discovers the `Greeter` class using C++26 reflection
3. Generates Python C API binding code
4. Compiles it to `build/greeter.so`

## Step 3: Use from Python

Create `test_greeter.py`:

```python
import sys
sys.path.insert(0, 'build')
import greeter

# Create an instance
g = greeter.Greeter()

# Access properties
print(g.name)           # "World"
print(g.greeting_count) # 0

# Modify properties
g.name = "Mirror Bridge"

# Call methods
message = g.greet()
print(message)          # "Hello, Mirror Bridge!"
print(g.greeting_count) # 1

# Call more methods
g.greet()
g.greet()
print(g.greeting_count) # 3

g.reset()
print(g.greeting_count) # 0
```

Run it:

```bash
python3 test_greeter.py
```

## What Got Bound?

Mirror Bridge automatically discovered and bound:

| C++ Feature | Python Access |
|-------------|---------------|
| `std::string name` | `g.name` (read/write property) |
| `int greeting_count` | `g.greeting_count` (read/write property) |
| `std::string greet()` | `g.greet()` (method returning string) |
| `void reset()` | `g.reset()` (void method) |
| Default constructor | `greeter.Greeter()` |

## Adding More Features

Let's extend our class with more C++ features:

```cpp
#pragma once
#include <string>
#include <vector>

struct Greeter {
    std::string name = "World";
    int greeting_count = 0;
    std::vector<std::string> history;

    std::string greet() {
        greeting_count++;
        std::string msg = "Hello, " + name + "!";
        history.push_back(msg);
        return msg;
    }

    // Multiple parameters
    std::string greet_custom(std::string prefix, std::string suffix) {
        greeting_count++;
        std::string msg = prefix + name + suffix;
        history.push_back(msg);
        return msg;
    }

    // Return container
    std::vector<std::string> get_history() const {
        return history;
    }

    void reset() {
        greeting_count = 0;
        history.clear();
    }
};
```

Regenerate and test:

```bash
mirror_bridge generate . --module greeter --lang python
```

```python
import greeter

g = greeter.Greeter()
g.name = "Developer"

g.greet()
g.greet_custom("Hi there, ", "!")
g.greet_custom("Welcome, ", " to Mirror Bridge!")

# Containers work automatically
history = g.get_history()
for msg in history:
    print(msg)

# Output:
# Hello, Developer!
# Hi there, Developer!
# Welcome, Developer to Mirror Bridge!
```

## Skipping Classes

If you have internal classes you don't want to expose:

```cpp
// This will be bound
struct PublicAPI {
    void do_something();
};

// MIRROR_BRIDGE_SKIP
// This will NOT be bound
struct InternalHelper {
    void internal_stuff();
};
```

## Next Steps

- **[Examples](../../examples/)** - More comprehensive examples
- **[CLI Reference](../reference/cli.md)** - All generation options
- **[Type Conversion](../reference/type-conversion.md)** - How types map between languages
- **[Multi-Language](../guides/multi-language.md)** - Add Lua and JavaScript bindings
