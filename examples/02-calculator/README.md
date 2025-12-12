# Example 02: Calculator

A calculator class demonstrating methods with various signatures.

## What You'll Learn

- Methods with parameters
- Methods with multiple parameters
- Const methods
- Exception handling
- String return values

## The C++ Code

```cpp
struct Calculator {
    double value = 0.0;

    double add(double x);
    double subtract(double x);
    double multiply(double x);
    double divide(double x);  // Throws on divide by zero

    // Multiple parameters
    double compute(double a, double b, double c);

    // Const method
    double get_value() const;

    void reset();
    std::string to_string() const;
};
```

## Build & Run

```bash
# Generate Python bindings
../../tools/mirror_bridge generate src/ --module calculator --lang python

# Run the test
python3 test_calculator.py
```

## Usage

```python
import calculator

calc = calculator.Calculator()

# Basic operations
calc.add(10.0)      # Returns 10.0
calc.subtract(3.0)  # Returns 7.0
calc.multiply(2.0)  # Returns 14.0
calc.divide(2.0)    # Returns 7.0

# Multiple parameters
result = calc.compute(2.0, 3.0, 4.0)  # 2 + 3*4 = 14

# Const method
value = calc.get_value()

# String representation
print(calc.to_string())  # "Calculator(value=7.0)"

# Exception handling
try:
    calc.divide(0.0)
except RuntimeError as e:
    print(f"Error: {e}")
```

## Exception Handling

C++ exceptions are automatically converted to Python exceptions:

| C++ Exception | Python Exception |
|---------------|------------------|
| `std::runtime_error` | `RuntimeError` |
| `std::invalid_argument` | `ValueError` |
| `std::exception` | `RuntimeError` |

## Next Steps

- [03-containers](../03-containers/) - Working with vectors and arrays
