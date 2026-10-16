#!/usr/bin/env python3
"""The ratio gate fails when it has nothing to compare against.

benchmarks/check_ratios.py turns the benchmark numbers into a check: a ratio
that was 2.8x against pybind11 and is now 2.2x fails the build. It compares
against benchmarks/ratio_floors.json, which is not in the repository and only
arrives through the monthly refresh PR.

Everything about that is fine except what it reports while unarmed. Measuring
the floors and returning 0 is indistinguishable from comparing against them
and finding nothing wrong, and there are three separate ways to reach it:

  * no floors file at all
  * a file whose "floors" value is null, which parses, so no exception fires
  * floors keyed by operations this run does not measure, so the comparison
    runs over an empty intersection and reports "no ratio regressed"

The third is the one that will happen: operation keys have been renamed in
this repository before. A rename leaves a file that parses, loads and compares
nothing.

--require-floors is what CI passes. These are the cases it has to catch.
No compiler and no benchmark run needed.
"""
import json
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
GATE = os.path.join(ROOT, "benchmarks", "check_ratios.py")

OPERATIONS = ["null_call", "arithmetic_int", "vector_get", "attr_get", "construction"]


def results(mb_ns=50.0, competitor_ns=140.0):
    return {
        "mirror_bridge": {op: mb_ns for op in OPERATIONS},
        "pybind11": {op: competitor_ns for op in OPERATIONS},
        "nanobind": {op: mb_ns * 1.1 for op in OPERATIONS},
    }


class RatioGate(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.results = os.path.join(self.dir, "results.json")
        self.floors = os.path.join(self.dir, "floors.json")
        with open(self.results, "w") as f:
            json.dump(results(), f)

    def run_gate(self, *flags, results_path=None):
        return subprocess.run(
            [sys.executable, GATE, results_path or self.results,
             "--floors", self.floors, *flags],
            capture_output=True, text=True,
        )

    def write_floors(self, floors):
        with open(self.floors, "w") as f:
            json.dump(floors, f)

    def test_a_missing_baseline_is_a_failure_under_require_floors(self):
        got = self.run_gate("--require-floors")
        self.assertEqual(got.returncode, 1, got.stdout)
        self.assertIn("no ratio was gated", got.stdout)
        self.assertTrue(os.path.exists(self.floors),
                        "the floors must still be written: the run that "
                        "establishes them is the run that fails")

    def test_a_missing_baseline_without_the_flag_still_bootstraps_quietly(self):
        got = self.run_gate()
        self.assertEqual(got.returncode, 0, got.stdout)
        self.assertIn("no ratio was gated", got.stdout)

    def test_a_null_floors_value_does_not_pass(self):
        """It parses, so no exception fires and the file looks present."""
        self.write_floors({"floors": None})
        got = self.run_gate("--require-floors")
        self.assertEqual(got.returncode, 1, got.stdout)

    def test_empty_floors_do_not_pass(self):
        self.write_floors({"floors": {}})
        got = self.run_gate("--require-floors")
        self.assertEqual(got.returncode, 1, got.stdout)
        self.assertIn("no ratio was gated", got.stdout)

    def test_floors_naming_only_operations_this_run_does_not_measure(self):
        """A renamed operation key leaves a file that compares nothing."""
        self.write_floors({"floors": {"attribute_get": {"pybind11": 2.5}}})
        got = self.run_gate("--require-floors")
        self.assertEqual(got.returncode, 1, got.stdout)
        self.assertIn("compared 0", got.stdout)

    def test_a_corrupt_floors_file_says_so_rather_than_tracebacking(self):
        with open(self.floors, "w") as f:
            f.write("this is not json")
        got = self.run_gate("--require-floors")
        self.assertEqual(got.returncode, 1)
        self.assertNotIn("Traceback", got.stderr)
        self.assertIn("no usable", got.stdout)

    def test_floors_without_the_key_say_so_rather_than_tracebacking(self):
        self.write_floors({"comment": "someone edited this by hand"})
        got = self.run_gate("--require-floors")
        self.assertEqual(got.returncode, 1)
        self.assertNotIn("Traceback", got.stderr)

    def test_real_floors_gate_and_pass(self):
        self.run_gate()  # bootstrap from this run
        got = self.run_gate("--require-floors")
        self.assertEqual(got.returncode, 0, got.stdout)
        self.assertIn("compared %d" % (len(OPERATIONS) * 2), got.stdout)
        self.assertIn("No ratio regressed", got.stdout)

    def test_a_regression_fails(self):
        self.run_gate()
        slower = os.path.join(self.dir, "slower.json")
        with open(slower, "w") as f:
            json.dump(results(mb_ns=50.0 * 1.5), f)  # 2.8x becomes 1.87x
        got = self.run_gate("--require-floors", results_path=slower)
        self.assertEqual(got.returncode, 1, got.stdout)
        self.assertIn("REGRESSED", got.stdout)

    def test_a_drop_inside_the_tolerance_does_not_fail(self):
        self.run_gate()
        noise = os.path.join(self.dir, "noise.json")
        with open(noise, "w") as f:
            json.dump(results(mb_ns=50.0 * 1.05), f)  # 5%, under the 12% band
        got = self.run_gate("--require-floors", results_path=noise)
        self.assertEqual(got.returncode, 0, got.stdout)

    def test_being_slower_than_a_competitor_is_reported_not_failed(self):
        faster_competitor = os.path.join(self.dir, "behind.json")
        with open(faster_competitor, "w") as f:
            json.dump(results(mb_ns=200.0, competitor_ns=140.0), f)
        self.run_gate(results_path=faster_competitor)
        got = self.run_gate("--require-floors", results_path=faster_competitor)
        self.assertEqual(got.returncode, 0, got.stdout)
        self.assertIn("Slower than a competitor", got.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
