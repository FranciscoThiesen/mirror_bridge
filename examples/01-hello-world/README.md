# Example 01: Hello World

The simplest possible Mirror Bridge example - a greeter class.

## What You'll Learn

- Basic class binding
- Properties (read/write)
- Simple methods
- Running your first binding

## The C++ Code

```cpp
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

## Build & Run

```bash
# Generate Python bindings
../../tools/mirror_bridge generate src/ --module greeter --lang python

# Run the test
python3 test_greeter.py
```

## Usage

```python
import greeter

g = greeter.Greeter()
print(g.name)           # "World"

g.name = "Developer"
print(g.greet())        # "Hello, Developer!"
print(g.greeting_count) # 1

g.reset()
print(g.greeting_count) # 0
```

## What Got Bound Automatically

| C++ Feature | Python Access |
|-------------|---------------|
| `std::string name` | `g.name` (property) |
| `int greeting_count` | `g.greeting_count` (property) |
| `std::string greet()` | `g.greet()` (method) |
| `void reset()` | `g.reset()` (method) |

## Next Steps

- [02-calculator](../02-calculator/) - More methods and parameters
