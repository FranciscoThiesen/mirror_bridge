"""Example test for the packaged mathlib bindings."""

from my_mathlib import Statistics

def test_statistics():
    stats = Statistics()
    stats.add_samples([1.0, 2.0, 3.0, 4.0, 5.0])

    assert stats.count == 5
    assert abs(stats.mean - 3.0) < 1e-10
    assert abs(stats.get_stddev() - 1.5811388300841898) < 1e-10

    print("All packaging example tests passed!")

if __name__ == "__main__":
    test_statistics()
