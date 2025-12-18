# Example 09: Enum Types

**Difficulty**: Beginner

Demonstrates binding scoped enums (enum class) to Python.

## What You'll Learn

- Scoped enum (enum class) binding
- Enums with explicit values
- Flag-style enums for bitwise operations
- Functions that accept and return enums
- Classes with enum members

## Files

```
src/
└── enums.hpp        # Color, Priority, Status, Direction, Task, Player
test_enums.py        # Python tests
```

## Key Concepts

### Basic Scoped Enum
```cpp
enum class Color {
    Red,
    Green,
    Blue
};
```

```python
color = game_enums.Color.Red
if color == game_enums.Color.Blue:
    print("It's blue!")
```

### Enum with Explicit Values
```cpp
enum class Priority {
    Low = 1,
    Normal = 5,
    High = 10,
    Critical = 100
};
```

```python
task.priority = game_enums.Priority.High
```

### Flag-Style Enums
```cpp
enum class Permission {
    None = 0,
    Read = 1,
    Write = 2,
    Execute = 4,
    All = 7  // Read | Write | Execute
};

bool has_permission(Permission flags, Permission check);
```

```python
# Note: 'None' becomes 'None_' in Python (reserved keyword)
has_read = game_enums.has_permission(
    game_enums.Permission.All,
    game_enums.Permission.Read
)  # True
```

### Enum Helper Functions
```cpp
std::string color_name(Color c);
Color color_from_name(const std::string& name);
```

```python
name = game_enums.color_name(game_enums.Color.Red)  # "red"
color = game_enums.color_from_name("blue")  # Color.Blue
```

### Direction Enum with Rotation
```cpp
enum class Direction {
    North = 0,
    East = 90,
    South = 180,
    West = 270
};

Direction rotate_clockwise(Direction d);
Direction rotate_counter_clockwise(Direction d);
```

```python
d = game_enums.Direction.North
d = game_enums.rotate_clockwise(d)  # East
print(game_enums.direction_degrees(d))  # 90
```

### Classes Using Enums
```cpp
class Task {
public:
    std::string name;
    Priority priority = Priority::Normal;
    Status status = Status::Idle;

    void start();
    void complete();
    bool is_done() const;
};
```

```python
task = game_enums.Task()
task.name = "Feature"
task.priority = game_enums.Priority.High
task.start()
print(task.status == game_enums.Status.Running)  # True
```

## Running

```bash
cd examples/09-enums
../../mirror_bridge_auto src/ --module game_enums
python3 test_enums.py
```

## Python Keyword Handling

Some C++ enum names may conflict with Python reserved words:
- `None` → `None_`
- `True` → `True_`
- `False` → `False_`

## Notes

- Scoped enums (`enum class`) are preferred over unscoped enums
- Enum values are accessed via the enum type (e.g., `Color.Red`)
- Enums can be compared with `==` and `!=`
- Functions can accept and return enum types directly
- Enum members on classes work like regular member variables
