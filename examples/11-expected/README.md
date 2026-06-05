# Example 11: Expected

A CSV parser using `std::expected<T, E>` for error handling that maps cleanly
across the C++/Python boundary.

## What You'll Learn

- `std::expected<T, E>` return values
- Success values pass through directly to Python
- `std::unexpected(...)` errors become Python `ValueError`
- `std::vector<double>` results converting to Python `list`
- Chaining fallible operations in C++ without exceptions

## The C++ Code

```cpp
struct FileParser {
    std::string delimiter = ",";
    bool strict_mode = false;

    std::expected<std::vector<double>, std::string> parse_numbers(const std::string& input) const {
        std::vector<double> result;
        // ...
        if (result.empty()) {
            return std::unexpected("no numbers found in input");
        }
        return result;
    }

    std::expected<double, std::string> compute_mean(const std::string& input) const;
    std::expected<double, std::string> compute_stddev(const std::string& input) const;
};
```

## Build & Run

```bash
# Generate Python bindings
../../tools/mirror_bridge generate src/ --module file_parser --lang python

# Run the test
python3 test_file_parser.py
```

## Usage

```python
import file_parser

parser = file_parser.FileParser()

# Success: the expected value passes through directly
numbers = parser.parse_numbers("1.5, 2.7, 3.14, 42.0")  # [1.5, 2.7, 3.14, 42.0]
mean = parser.compute_mean("10, 20, 30, 40")            # 25.0

# Error: std::unexpected(...) becomes a Python ValueError
try:
    parser.parse_numbers("hello, world")
except ValueError as e:
    print(f"Parse error: {e}")  # invalid number: 'hello'

# Properties still work
parser.delimiter = ";"
parser.parse_numbers("1.0; 2.0; 3.0")  # [1.0, 2.0, 3.0]
```

## Expected Mapping

| C++ Return | Python Behavior |
|------------|-----------------|
| `expected<T, E>` holding a value | returns `T` |
| `std::unexpected(msg)` | raises `ValueError(msg)` |
| `expected<std::vector<double>, ...>` | returns a `list` on success |

The binding layer needs no special error-handling code: the unexpected branch
is converted to an exception automatically.

## Next Steps

- [packaging](../packaging/) - Building a distributable pip package
