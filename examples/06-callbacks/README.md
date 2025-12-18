# Example 06: Callbacks

**Difficulty**: Intermediate

Demonstrates passing Python functions to C++ code via `std::function`.

## What You'll Learn

- Using `std::function<>` for callbacks
- Void callbacks (no return value)
- Callbacks with parameters
- Callbacks with return values
- Event-driven programming patterns

## Files

```
src/
└── event_system.hpp    # Button, Timer, TextProcessor, EventDispatcher
test_callbacks.py       # Python tests demonstrating callback usage
```

## Key Concepts

### Simple Void Callback
```cpp
using VoidCallback = std::function<void()>;

class Button {
    void set_on_click(VoidCallback callback);
    void click();  // Calls the callback
};
```

```python
def on_click():
    print("Clicked!")

button.set_on_click(on_click)
button.click()  # Prints "Clicked!"
```

### Callback with Parameter
```cpp
using IntCallback = std::function<void(int)>;

class Timer {
    void set_callback(IntCallback callback);
    void tick();  // Calls callback with tick count
};
```

```python
timer.set_callback(lambda count: print(f"Tick {count}"))
timer.tick(3)  # Prints "Tick 1", "Tick 2", "Tick 3"
```

### Callback with Return Value
```cpp
using StringCallback = std::function<std::string(const std::string&)>;

class TextProcessor {
    void set_transform(StringCallback transform);
    std::string process(const std::string& input);
};
```

```python
processor.set_transform(lambda s: s.upper())
result = processor.process("hello")  # Returns "HELLO"
```

## Running

```bash
cd examples/06-callbacks
../../mirror_bridge_auto src/ --module callbacks
python3 test_callbacks.py
```

## Common Patterns

### Event System
```python
def handle_event(event):
    print(f"Event: {event.type}, Value: {event.value}")

dispatcher = callbacks.EventDispatcher()
dispatcher.add_handler(handle_event)
dispatcher.dispatch("click", 42)
```

### Closures with State
```python
counter = [0]
def increment():
    counter[0] += 1

button.set_on_click(increment)
button.click()  # counter[0] is now 1
```

## Notes

- Python functions are automatically wrapped as `std::function`
- The GIL is properly managed during callback execution
- Callbacks can capture Python state via closures
- Multiple callbacks can be registered (see EventDispatcher)
