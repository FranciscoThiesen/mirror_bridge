# Example 08: Free Functions

**Difficulty**: Beginner

Demonstrates binding standalone (non-member) functions.

## What You'll Learn

- Binding free functions from namespaces
- Functions with different parameter types
- Functions returning various types (primitives, strings, vectors)
- Exception handling from free functions
- Void functions

## Files

```
src/
└── utilities.hpp    # Arithmetic, string, vector utilities
test_utils.py        # Python tests
```

## Key Concepts

### Basic Free Functions
```cpp
namespace utils {
    int add(int a, int b) {
        return a + b;
    }

    double multiply(double a, double b) {
        return a * b;
    }
}
```

```python
result = utilities.add(2, 3)       # 5
product = utilities.multiply(2.5, 4.0)  # 10.0
```

### String Functions
```cpp
std::string to_upper(const std::string& s);
std::string to_lower(const std::string& s);
std::string repeat(const std::string& s, int n);
bool starts_with(const std::string& s, const std::string& prefix);
```

```python
utilities.to_upper("hello")        # "HELLO"
utilities.repeat("ab", 3)          # "ababab"
utilities.starts_with("hello", "he")  # True
```

### Vector Functions
```cpp
double sum(const std::vector<double>& values);
double mean(const std::vector<double>& values);
std::vector<int> range(int start, int end);
std::vector<double> linspace(double start, double end, int num);
```

```python
utilities.sum([1.0, 2.0, 3.0])     # 6.0
utilities.range(0, 5)              # [0, 1, 2, 3, 4]
utilities.linspace(0.0, 1.0, 5)    # [0.0, 0.25, 0.5, 0.75, 1.0]
```

### Exception Handling
```cpp
double divide(double a, double b) {
    if (b == 0.0) {
        throw std::runtime_error("Division by zero");
    }
    return a / b;
}

void validate_positive(double value) {
    if (value <= 0) {
        throw std::runtime_error("Value must be positive");
    }
}
```

```python
try:
    utilities.divide(1.0, 0.0)
except RuntimeError as e:
    print("Caught:", e)  # "Division by zero"
```

## Running

```bash
cd examples/08-free-functions
../../mirror_bridge_auto src/ --module utilities
python3 test_utils.py
```

## Common Patterns

### Math Utilities
```cpp
double clamp(double value, double min_val, double max_val);
double lerp(double a, double b, double t);
double degrees_to_radians(double degrees);
```

### Validation Functions
```cpp
void validate_positive(double value);  // Throws if <= 0
void validate_range(double value, double min, double max);
```

## Notes

- Free functions are bound at module level (not as class methods)
- Functions from the same namespace can be grouped in one module
- Exception handling works the same as with class methods
- Return type conversion (vectors, strings) is automatic
