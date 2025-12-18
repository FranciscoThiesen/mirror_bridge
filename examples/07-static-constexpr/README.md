# Example 07: Static Methods and Constexpr

**Difficulty**: Intermediate

Demonstrates static class members, constexpr constants, and factory methods.

## What You'll Learn

- Static constexpr member constants
- Static factory methods
- Static utility methods
- Class-level state (static member variables)
- Mixing static and instance members

## Files

```
src/
└── math_utils.hpp    # Constants, Point2D, Config, Counter
test_static.py        # Python tests
```

## Key Concepts

### Static Constexpr Constants
```cpp
class Constants {
public:
    static constexpr double PI = 3.14159265358979;
    static constexpr int MAX_ITERATIONS = 1000;
    static constexpr bool DEBUG_MODE = false;
};
```

```python
# Access via class
print(Constants.PI)           # 3.14159...
print(Constants.MAX_ITERATIONS)  # 1000

# Also works via instance
c = Constants()
print(c.PI)
```

### Static Factory Methods
```cpp
class Point2D {
public:
    double x, y;

    static Point2D origin() { return {0, 0}; }
    static Point2D from_polar(double r, double theta);
    static Point2D unit_x() { return {1, 0}; }
};
```

```python
origin = Point2D.origin()
unit = Point2D.unit_x()
polar = Point2D.from_polar(1.0, math.pi/4)
```

### Static Utility Methods
```cpp
class Point2D {
    static double distance(const Point2D& a, const Point2D& b);
    static Point2D midpoint(const Point2D& a, const Point2D& b);
};
```

```python
dist = Point2D.distance(p1, p2)
mid = Point2D.midpoint(p1, p2)
```

### Class-Level State
```cpp
class Counter {
public:
    static int total_count;  // Shared across all instances
    int value = 0;           // Per-instance

    void increment() { value++; total_count++; }
    static int get_total() { return total_count; }
    static void reset_total() { total_count = 0; }
};
```

```python
Counter.reset_total()
c1, c2 = Counter(), Counter()
c1.increment()
c2.increment()
print(Counter.get_total())  # 2 (shared)
print(c1.value)             # 1 (instance)
```

## Running

```bash
cd examples/07-static-constexpr
../../mirror_bridge_auto src/ --module static_math
python3 test_static.py
```

## Common Patterns

### Version Information
```cpp
class Config {
    static constexpr int VERSION_MAJOR = 2;
    static constexpr int VERSION_MINOR = 1;
    static std::string version_string();
};
```

### Singleton-like Factories
```cpp
static Point2D origin();    // Always returns (0,0)
static Point2D unit_x();    // Always returns (1,0)
```

## Notes

- `static constexpr` members are read-only from Python
- Static methods don't require an instance to call
- Static member variables are shared across all instances
- Factory methods are a clean way to create objects with specific configurations
