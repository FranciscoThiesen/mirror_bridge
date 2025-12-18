#!/usr/bin/env python3
"""Test callbacks example - Python functions as C++ callbacks."""

import sys
sys.path.insert(0, 'build')

import callbacks

def test_button_callback():
    """Test simple void callback on button click."""
    clicked = [False]  # Use list to allow mutation in closure

    def on_click():
        clicked[0] = True

    button = callbacks.Button()
    button.label = "Submit"
    button.set_on_click(on_click)

    assert button.get_click_count() == 0
    assert not clicked[0]

    button.click()

    assert button.get_click_count() == 1
    assert clicked[0]

    print("✓ Button callback works")


def test_timer_callback():
    """Test callback with integer parameter."""
    ticks = []

    def on_tick(count):
        ticks.append(count)

    timer = callbacks.Timer()
    timer.set_callback(on_tick)

    timer.tick(3)

    assert ticks == [1, 2, 3]
    assert timer.get_tick_count() == 3

    print("✓ Timer callback with parameter works")


def test_transform_callback():
    """Test callback that transforms and returns value."""
    processor = callbacks.TextProcessor()

    # Python lambda as C++ callback
    processor.set_transform(lambda s: s.upper())

    result = processor.process("hello world")
    assert result == "HELLO WORLD"

    # Change the transform
    processor.set_transform(lambda s: f"[{s}]")
    result = processor.process("wrapped")
    assert result == "[wrapped]"

    print("✓ Transform callback with return value works")


def test_event_dispatcher():
    """Test event dispatcher with multiple handlers."""
    events_received = []

    def handler1(event):
        events_received.append(f"h1:{event.type}:{event.value}")

    def handler2(event):
        events_received.append(f"h2:{event.type}:{event.value}")

    dispatcher = callbacks.EventDispatcher()
    dispatcher.add_handler(handler1)
    dispatcher.add_handler(handler2)

    assert dispatcher.handler_count() == 2

    dispatcher.dispatch("click", 42)

    assert len(events_received) == 2
    assert "h1:click:42" in events_received
    assert "h2:click:42" in events_received

    dispatcher.clear_handlers()
    assert dispatcher.handler_count() == 0

    print("✓ Event dispatcher with multiple handlers works")


def test_closure_state():
    """Test that Python closures maintain state correctly."""
    counter = [0]

    def incrementer():
        counter[0] += 1

    button = callbacks.Button()
    button.set_on_click(incrementer)

    for _ in range(5):
        button.click()

    assert counter[0] == 5
    assert button.get_click_count() == 5

    print("✓ Closures maintain state correctly")


if __name__ == "__main__":
    test_button_callback()
    test_timer_callback()
    test_transform_callback()
    test_event_dispatcher()
    test_closure_state()

    print("\n✓ All callback tests passed!")
