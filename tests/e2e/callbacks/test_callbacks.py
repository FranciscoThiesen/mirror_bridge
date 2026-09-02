#!/usr/bin/env python3
"""Test std::function callback bindings"""

import sys
import os

# Add build directory to Python path (use absolute path for CI compatibility)
# Build output is at repo_root/build, which is 3 levels up from tests/e2e/callbacks
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..', 'build'))

import callbacks

print("=== Callback Binding Test ===")
print()

# Test 1: Simple void callback
print("Test 1: void(int) callback")
emitter = callbacks.EventEmitter()

received_values = []
def data_handler(value):
    received_values.append(value)

emitter.on_data(data_handler)
assert emitter.has_data_callback(), "Callback should be set"
# A C++ bool must arrive as a real bool, not as 1/0.
assert emitter.has_data_callback() is True, "bool return should be True, not 1"
assert callbacks.EventEmitter().has_data_callback() is False, "bool return should be False, not 0"

emitter.emit_data(42)
emitter.emit_data(100)
emitter.emit_data(-5)

assert received_values == [42, 100, -5], f"Expected [42, 100, -5], got {received_values}"
print(f"  Received values: {received_values}")
print("  PASS")
print()

# Test 2: String callback
print("Test 2: void(string) callback")
messages = []
def message_handler(msg):
    messages.append(msg)

emitter.on_message(message_handler)
assert emitter.has_message_callback(), "Message callback should be set"

emitter.emit_message("Hello")
emitter.emit_message("World")

assert messages == ["Hello", "World"], f"Expected ['Hello', 'World'], got {messages}"
print(f"  Received messages: {messages}")
print("  PASS")
print()

# Test 3: Callback with return value
print("Test 3: int(int, int) callback")
def add_handler(a, b):
    return a + b

emitter.on_compute(add_handler)
assert emitter.has_compute_callback(), "Compute callback should be set"

result = emitter.compute(10, 20)
assert result == 30, f"Expected 30, got {result}"
print(f"  compute(10, 20) = {result}")

# Test with lambda
emitter.on_compute(lambda a, b: a * b)
result = emitter.compute(6, 7)
assert result == 42, f"Expected 42, got {result}"
print(f"  compute(6, 7) with multiply = {result}")
print("  PASS")
print()

# Test 4: Clear callbacks
print("Test 4: Clear callbacks")
emitter.clear_callbacks()
assert not emitter.has_data_callback(), "Data callback should be cleared"
assert not emitter.has_message_callback(), "Message callback should be cleared"
assert not emitter.has_compute_callback(), "Compute callback should be cleared"
print("  All callbacks cleared")
print("  PASS")
print()

# Test 5: None callback
print("Test 5: Setting None as callback")
emitter.on_data(None)
assert not emitter.has_data_callback(), "None should clear callback"
print("  None clears callback")
print("  PASS")
print()

# Test 6: DataProcessor with transform
print("Test 6: DataProcessor with transform callback")
processor = callbacks.DataProcessor()

# Square transform
processor.set_transform(lambda x: x * x)

values = [1.0, 2.0, 3.0, 4.0]
result = processor.process(values)
assert list(result) == [1.0, 4.0, 9.0, 16.0], f"Expected [1.0, 4.0, 9.0, 16.0], got {result}"
print(f"  process([1,2,3,4]) with square = {result}")

# Sum after square transform
total = processor.sum_transformed(values)
assert total == 30.0, f"Expected 30.0, got {total}"
print(f"  sum_transformed([1,2,3,4]) = {total}")
print("  PASS")
print()

# Test 7: Callback exception handling
print("Test 7: Callback exception propagation")
def bad_callback(value):
    raise ValueError("Test exception from Python")

emitter.on_data(bad_callback)

try:
    emitter.emit_data(1)
    assert False, "Should have raised an exception"
except RuntimeError as e:
    assert "Test exception from Python" in str(e), f"Wrong error message: {e}"
    print(f"  Exception properly propagated: {e}")
    print("  PASS")
print()

print("=" * 40)
print("ALL CALLBACK TESTS PASSED!")
print("=" * 40)
