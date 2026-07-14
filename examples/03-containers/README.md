# Example 03: Containers

Working with C++ containers (vector, array) in Python.

## What You'll Learn

- `std::vector` conversion to/from Python `list`
- `std::array` conversion to/from Python `list`
- Methods returning containers
- Setting containers from Python

## The C++ Code

```cpp
struct DataProcessor {
    std::vector<double> values;
    std::vector<std::string> labels;
    std::array<int, 3> dimensions = {0, 0, 0};

    void add_value(double v);
    double sum() const;
    double average() const;

    std::vector<double> get_values() const;
    std::vector<std::string> get_labels() const;

    void set_dimensions(int x, int y, int z);
    int volume() const;
};
```

## Build & Run

```bash
# Generate Python bindings
../../tools/mirror_bridge generate src/ --module data_processor --lang python

# Run the test
python3 test_containers.py
```

## Usage

```python
import data_processor

dp = data_processor.DataProcessor()

# Add values one at a time
dp.add_value(1.0)
dp.add_value(2.0)
dp.add_value(3.0)
print(dp.sum())      # 6.0
print(dp.average())  # 2.0

# Or set the entire vector from a Python list
dp.values = [10.0, 20.0, 30.0, 40.0, 50.0]
print(dp.sum())  # 150.0

# Get vector as Python list
values = dp.get_values()  # [10.0, 20.0, 30.0, 40.0, 50.0]

# String vectors work too
dp.labels = ["alpha", "beta", "gamma"]
labels = dp.get_labels()  # ["alpha", "beta", "gamma"]

# Fixed-size arrays
dp.dimensions = [2, 3, 4]
print(dp.volume())  # 24
```

## Container Type Mapping

| C++ Type | Python Type |
|----------|-------------|
| `std::vector<T>` | `list` |
| `std::array<T, N>` | `list` |
| `std::deque<T>` | `list` |
| `std::list<T>` | `list` |

## Next Steps

- [04-image-processing](../04-image-processing/) - A realistic image processing library
