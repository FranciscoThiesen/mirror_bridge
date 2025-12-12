#!/usr/bin/env python3
"""Test container bindings."""

import sys
sys.path.insert(0, 'build')

import data_processor

def test_vector_double():
    dp = data_processor.DataProcessor()

    # Add values via method
    dp.add_value(1.0)
    dp.add_value(2.0)
    dp.add_value(3.0)

    assert dp.value_count() == 3, f"Expected 3, got {dp.value_count()}"
    assert dp.sum() == 6.0, f"Expected 6.0, got {dp.sum()}"
    assert dp.average() == 2.0, f"Expected 2.0, got {dp.average()}"

    # Get values as list
    values = dp.get_values()
    assert values == [1.0, 2.0, 3.0], f"Expected [1.0, 2.0, 3.0], got {values}"

    print("✓ Vector<double> test passed")

def test_vector_string():
    dp = data_processor.DataProcessor()

    dp.add_label("alpha")
    dp.add_label("beta")
    dp.add_label("gamma")

    assert dp.label_count() == 3, f"Expected 3, got {dp.label_count()}"

    labels = dp.get_labels()
    assert labels == ["alpha", "beta", "gamma"], f"Expected ['alpha', 'beta', 'gamma'], got {labels}"

    print("✓ Vector<string> test passed")

def test_set_vector():
    dp = data_processor.DataProcessor()

    # Set vector directly from Python list
    dp.values = [10.0, 20.0, 30.0, 40.0, 50.0]
    assert dp.sum() == 150.0, f"Expected 150.0, got {dp.sum()}"

    dp.labels = ["one", "two", "three"]
    assert dp.label_count() == 3

    print("✓ Set vector from list test passed")

def test_array():
    dp = data_processor.DataProcessor()

    # Set dimensions via method
    dp.set_dimensions(2, 3, 4)
    assert dp.volume() == 24, f"Expected 24, got {dp.volume()}"

    # Set dimensions directly
    dp.dimensions = [5, 5, 5]
    assert dp.volume() == 125, f"Expected 125, got {dp.volume()}"

    print("✓ Array test passed")

def test_clear():
    dp = data_processor.DataProcessor()
    dp.values = [1.0, 2.0, 3.0]
    dp.labels = ["a", "b"]
    dp.dimensions = [1, 2, 3]

    dp.clear()

    assert dp.value_count() == 0
    assert dp.label_count() == 0
    assert dp.volume() == 0

    print("✓ Clear test passed")

if __name__ == "__main__":
    test_vector_double()
    test_vector_string()
    test_set_vector()
    test_array()
    test_clear()
    print("\n✓ All tests passed!")
