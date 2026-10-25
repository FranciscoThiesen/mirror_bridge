#!/usr/bin/env python3
"""Test container bindings."""

import array
import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'build'))

import containers

def test_vector_double():
    dp = containers.DataProcessor()

    # Add values via method
    dp.add_value(1.0)
    dp.add_value(2.0)
    dp.add_value(3.0)

    assert dp.value_count() == 3, f"Expected 3, got {dp.value_count()}"
    assert dp.sum() == 6.0, f"Expected 6.0, got {dp.sum()}"
    assert dp.average() == 2.0, f"Expected 2.0, got {dp.average()}"

    # A vector of a numeric type comes back as an array.array rather than a
    # list: the whole buffer is copied in one go instead of allocating a
    # Python float per element. It indexes, slices and iterates like any
    # sequence, and list() gives a list where a list is what you want.
    values = dp.get_values()
    assert isinstance(values, array.array), f"Expected array.array, got {type(values)}"
    assert values.typecode == 'd'
    assert list(values) == [1.0, 2.0, 3.0], f"Expected [1.0, 2.0, 3.0], got {values}"

    print("✓ Vector<double> test passed")

def test_vector_string():
    dp = containers.DataProcessor()

    dp.add_label("alpha")
    dp.add_label("beta")
    dp.add_label("gamma")

    assert dp.label_count() == 3, f"Expected 3, got {dp.label_count()}"

    labels = dp.get_labels()
    assert labels == ["alpha", "beta", "gamma"], f"Expected ['alpha', 'beta', 'gamma'], got {labels}"

    print("✓ Vector<string> test passed")

def test_set_vector():
    dp = containers.DataProcessor()

    # Set vector directly from Python list
    dp.values = [10.0, 20.0, 30.0, 40.0, 50.0]
    assert dp.sum() == 150.0, f"Expected 150.0, got {dp.sum()}"

    dp.labels = ["one", "two", "three"]
    assert dp.label_count() == 3

    print("✓ Set vector from list test passed")

def test_array():
    dp = containers.DataProcessor()

    # Set dimensions via method
    dp.set_dimensions(2, 3, 4)
    assert dp.volume() == 24, f"Expected 24, got {dp.volume()}"

    # Set dimensions directly
    dp.dimensions = [5, 5, 5]
    assert dp.volume() == 125, f"Expected 125, got {dp.volume()}"

    print("✓ Array test passed")

def test_clear():
    dp = containers.DataProcessor()
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
