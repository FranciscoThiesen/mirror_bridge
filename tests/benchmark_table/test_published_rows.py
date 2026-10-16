#!/usr/bin/env python3
"""Every operation the benchmark suite measures reaches the published page.

The runtime suite measures twelve operations per framework. The page in
docs/internals/benchmarks.md showed seven of them, and had since the table
was first generated, because the formatter's row list and the runner's result
keys were written independently and three keys never matched:

    runner writes          formatter looked for
    attr_get               attribute_get
    attr_set               attribute_set
    vector_set             vector_sum

The formatter's reaction to a key it cannot find was `continue`, so the row
vanished and the exit code stayed 0. Two of the five missing rows were
attribute read and write -- the most frequent operation on a bound object --
and one was the whole list-in direction, which is the direction the project's
headline buffer claim is about.

Nothing here runs a benchmark or needs a compiler. It reads the two files that
have to agree and compares them, so an operation added to the runner either
appears on the page or fails this test.

That comparison is only as good as its reading of the runners, which is
textual: it recognises `results['k'] =` and `results.k =` and nothing else. A
runner that started writing its results some other way -- results.update({...}),
Object.assign(results, {...}), a computed key -- would be read as measuring
fewer operations than it does, and this would pass while the page lost rows
again. So the shapes it cannot read are rejected outright rather than ignored.
"""
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
BENCH = os.path.join(ROOT, "benchmarks")
FORMATTER = os.path.join(BENCH, "format_results.py")
RUNNERS = {
    "python": os.path.join(BENCH, "runtime", "python", "run_runtime_benchmarks.py"),
    "lua": os.path.join(BENCH, "runtime", "lua", "run_runtime_benchmarks.lua"),
    "javascript": os.path.join(BENCH, "runtime", "javascript", "run_runtime_benchmarks.js"),
}


def measured_keys(path):
    """The operation keys a runner writes into its results table.

    Each language spells the assignment its own way -- `results['k'] =` in
    Python, `results.k =` in Lua and JavaScript -- so match both rather than
    maintaining a copy of the list here.
    """
    with open(path, encoding="utf-8") as f:
        src = f.read()
    quoted = set(re.findall(r"results\[['\"]([A-Za-z_][A-Za-z0-9_]*)['\"]\]\s*=", src))
    dotted = set(re.findall(r"\bresults\.([A-Za-z_][A-Za-z0-9_]*)\s*=", src))
    return quoted | dotted


def published_keys():
    """The formatter's row list, read out of the file that ships."""
    with open(FORMATTER, encoding="utf-8") as f:
        src = f.read()
    block = re.search(r"OPERATIONS = \[(.*?)\n\]", src, re.S)
    assert block, "OPERATIONS is no longer a list literal in format_results.py"
    return re.findall(r"\(\s*['\"]([A-Za-z_][A-Za-z0-9_]*)['\"]", block.group(1))


def framework_names():
    """The framework list, read out of the formatter rather than copied.

    Copying it is the mistake this whole test exists because of.
    """
    with open(FORMATTER, encoding="utf-8") as f:
        src = f.read()
    m = re.search(r"FRAMEWORKS = \[(.*?)\]", src, re.S)
    assert m, "FRAMEWORKS is no longer a list literal in format_results.py"
    return re.findall(r"['\"]([A-Za-z_][A-Za-z0-9_]*)['\"]", m.group(1))


FRAMEWORKS = framework_names()

# An all-zero column is dropped by the formatter as noise, so a framework can
# only appear in a payload with real numbers in it.
assert "mirror_bridge" in FRAMEWORKS and "pybind11" in FRAMEWORKS


def payload(keys, ns=100.0):
    return {fw: {k: ns + i for i, k in enumerate(sorted(keys))} for fw in FRAMEWORKS}


def render(data, *flags):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump(data, f)
        path = f.name
    try:
        return subprocess.run(
            [sys.executable, FORMATTER, path, "--fragment", *flags],
            capture_output=True, text=True,
        )
    finally:
        os.unlink(path)


class PublishedRows(unittest.TestCase):
    def test_every_measured_operation_has_a_row(self):
        measured = measured_keys(RUNNERS["python"])
        self.assertTrue(measured, "found no result keys in the Python runner")
        missing = sorted(measured - set(published_keys()))
        self.assertEqual(missing, [], "measured on every run but absent from the page")

    def test_every_row_is_actually_measured(self):
        measured = measured_keys(RUNNERS["python"])
        phantom = sorted(set(published_keys()) - measured)
        self.assertEqual(phantom, [], "the page asks for operations nobody measures")

    def test_the_runners_only_write_results_in_shapes_this_test_can_read(self):
        """Reject a write shape rather than silently reading past it.

        measured_keys matches two assignment forms. Anything else is invisible
        to it, which would make every comparison here weaker without saying so.
        """
        unreadable = [
            (r"results\.update\s*\(", "results.update(...)"),
            (r"Object\.assign\s*\(\s*results", "Object.assign(results, ...)"),
            (r"setattr\s*\(\s*results", "setattr(results, ...)"),
            (r"results\[\s*[A-Za-z_]", "results[variable]"),
            (r"results\[\s*f['\"]", "results[f-string]"),
        ]
        for lang, path in RUNNERS.items():
            with open(path, encoding="utf-8") as f:
                src = f.read()
            for pattern, shape in unreadable:
                self.assertIsNone(
                    re.search(pattern, src),
                    f"the {lang} runner writes results as {shape}, which this "
                    f"test cannot read; teach measured_keys that shape before "
                    f"using it",
                )

    def test_the_row_list_has_no_duplicates(self):
        keys = published_keys()
        self.assertEqual(len(keys), len(set(keys)))

    def test_the_other_language_runners_use_the_same_keys(self):
        """Lua and JavaScript measure the same operations under the same names.

        They have no published table of their own, but a key that drifts here
        is the same mistake one step from mattering.
        """
        base = measured_keys(RUNNERS["python"])
        for lang in ("lua", "javascript"):
            self.assertEqual(
                measured_keys(RUNNERS[lang]), base,
                f"the {lang} runner measures a different set of operations",
            )

    def test_a_full_payload_renders_every_row(self):
        keys = published_keys()
        got = render(payload(keys))
        self.assertEqual(got.returncode, 0, got.stderr)
        rows = [ln for ln in got.stdout.splitlines() if ln.startswith("| ") and "---" not in ln]
        self.assertEqual(len(rows), len(keys) + 1, got.stdout)  # + the header
        self.assertEqual(got.stderr, "", "a complete payload should warn about nothing")

    def test_a_missing_key_is_reported_and_fails_strict(self):
        keys = published_keys()
        thin = payload(keys)
        del thin["nanobind"]["attr_get"]
        got = render(thin, "--strict")
        self.assertEqual(got.returncode, 1, "a dropped row must not exit 0 under --strict")
        self.assertIn("attr_get", got.stderr)
        self.assertIn("nanobind", got.stderr)
        self.assertNotIn("Attribute read", got.stdout)

    def test_an_unpublished_measurement_is_reported(self):
        keys = published_keys()
        extra = payload(keys)
        for fw in extra:
            extra[fw]["brand_new_operation"] = 5.0
        got = render(extra, "--strict")
        self.assertEqual(got.returncode, 1)
        self.assertIn("brand_new_operation", got.stderr)

    def test_without_strict_a_drop_still_renders_and_exits_zero(self):
        """The monthly refresh must not start failing retroactively.

        --strict is for the caller that wants the gate; the default stays
        render-and-warn so an old results file still produces a page.
        """
        keys = published_keys()
        thin = payload(keys)
        del thin["swig"]["vector_set"]
        got = render(thin)
        self.assertEqual(got.returncode, 0, got.stderr)
        self.assertIn("vector_set", got.stderr)
        self.assertIn("Null call", got.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
