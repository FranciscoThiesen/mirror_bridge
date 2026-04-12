"""Example: Using std::expected for clean error handling across the C++/Python boundary.

C++ functions returning std::expected<T, E> automatically:
- Return the T value on success
- Raise ValueError with the E message on error

No special error handling code needed in the binding layer.
"""

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))
import file_parser

def main():
    parser = file_parser.FileParser()

    # Parse valid CSV numbers
    print("=== Parsing Numbers ===")
    numbers = parser.parse_numbers("1.5, 2.7, 3.14, 42.0")
    print(f"Parsed: {numbers}")

    # Compute statistics
    print(f"\nMean: {parser.compute_mean('10, 20, 30, 40')}")
    print(f"Std dev: {parser.compute_stddev('10, 20, 30, 40'):.4f}")

    # Error handling — errors become Python exceptions naturally
    print("\n=== Error Handling ===")
    try:
        parser.parse_numbers("hello, world")
    except ValueError as e:
        print(f"Parse error (caught): {e}")

    try:
        parser.compute_mean("")
    except ValueError as e:
        print(f"Empty input (caught): {e}")

    try:
        parser.compute_stddev("42")
    except ValueError as e:
        print(f"Too few numbers (caught): {e}")

    # Change delimiter
    parser.delimiter = ";"
    result = parser.parse_numbers("1.0; 2.0; 3.0")
    print(f"\nSemicolon-delimited: {result}")

    print("\nAll examples completed successfully!")

if __name__ == "__main__":
    main()
