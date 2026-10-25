#!/usr/bin/env python3
"""A step that can fail must not run before the refresh PR is opened.

The benchmarks workflow regenerates two tables and opens a PR with them. Two
of its steps are checks that are meant to fail the build, and both of them
deliberately run *after* the PR step, because a month of numbers is worth
more than the promptness of a red build.

The trading splice was not written that way. format_trading_results.py
refuses to render when numpy was missing -- correctly, since the page would
otherwise claim a vectorised comparison it never made -- and that refusal
ran before the PR step and took the whole job with it. The runtime table had
already regenerated cleanly and was thrown away with it. That is why the
published page still carried seven of the twelve measured operations long
after the formatter was fixed to emit twelve, and why ratio_floors.json never
landed, leaving the ratio gate unarmed.

The comment in the workflow stated the rule. Nothing checked it. So:

  * every step that can fail the job runs after the PR step
  * a pre-PR step that swallows a failure records it as an output, and some
    post-PR step actually reads that output -- a recorded rc nobody reads is
    a gate that gates nothing, which this repository has shipped before
  * the swallowing shell really does swallow: tested by running it

No compiler, no benchmark run and no network needed.
"""
import os
import re
import subprocess
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
WORKFLOW = os.path.join(ROOT, ".github", "workflows", "benchmarks.yml")

PR_STEP = "Open PR with the regenerated page"

STEP_INDENT = " " * 6
KEY_INDENT = " " * 8


def _chunks():
    """Split benchmarks.yml into one text block per step, in order."""
    with open(WORKFLOW) as f:
        body = f.read().split("\n")
    marker = STEP_INDENT + "- "
    starts = [i for i, line in enumerate(body) if line.startswith(marker)]
    if not starts:
        raise AssertionError(f"no steps found in {WORKFLOW}; the reader needs updating")
    bounds = starts + [len(body)]
    return [body[a:b] for a, b in zip(starts, bounds[1:])]


def _parse(chunk):
    """One step's name, id and run body.

    Hand-read rather than parsed with PyYAML, which none of the three images
    installs: a test that skips itself in CI is a test that does not exist.
    This understands only the shape this one file is written in, and every
    caller asserts on what it found, so a file that outgrows the reader fails
    the tests rather than quietly matching nothing.
    """
    step = {}
    lines = list(chunk)
    lines[0] = KEY_INDENT + lines[0][len(STEP_INDENT) + 2:]

    i = 0
    while i < len(lines):
        line = lines[i]
        i += 1
        if not line.startswith(KEY_INDENT) or line.startswith(KEY_INDENT + " "):
            continue
        key, _, value = line[len(KEY_INDENT):].partition(":")
        value = value.strip()
        if key not in ("name", "id", "uses", "run"):
            continue
        if key != "run" or value not in ("|", "|-"):
            step[key] = value
            continue
        block = []
        while i < len(lines):
            nxt = lines[i]
            if nxt.strip() and not nxt.startswith(KEY_INDENT + " "):
                break
            block.append(nxt[len(KEY_INDENT) + 2:] if nxt.strip() else "")
            i += 1
        step["run"] = "\n".join(block)
    return step


def steps():
    return [_parse(c) for c in _chunks()]


def index_of(name):
    for i, step in enumerate(steps()):
        if step.get("name") == name:
            return i
    raise AssertionError(f"no step named {name!r} in {WORKFLOW}")


class RefreshContract(unittest.TestCase):
    def test_the_pr_step_is_still_there(self):
        index_of(PR_STEP)

    def test_every_failing_step_runs_after_the_pr(self):
        pr = index_of(PR_STEP)
        failing = [(i, s["name"]) for i, s in enumerate(steps())
                   if s.get("name", "").startswith("Fail if")]
        self.assertTrue(failing, "the workflow has stopped checking anything")
        for i, name in failing:
            self.assertGreater(i, pr, f"{name!r} runs before the PR is opened")

    def test_a_recorded_rc_is_read_by_someone(self):
        # The failure mode this catches is a step that carefully writes
        # rc=$? and exits 0 while nothing downstream looks at it.
        pr = index_of(PR_STEP)
        all_steps = steps()
        recorded = {s["id"] for i, s in enumerate(all_steps)
                    if i < pr and "GITHUB_OUTPUT" in s.get("run", "") and "id" in s}
        self.assertTrue(recorded, "no pre-PR step records a result any more")

        after = "\n".join(s.get("run", "") for s in all_steps[pr + 1:])
        for step_id in recorded:
            self.assertIn(f"steps.{step_id}.outputs.rc", after,
                          f"step {step_id!r} records an rc that nothing reads")

    def test_every_post_pr_check_reads_a_recorded_rc_or_runs_a_command(self):
        pr = index_of(PR_STEP)
        all_steps = steps()
        ids = {s["id"] for s in all_steps[:pr] if "id" in s}
        for s in all_steps[pr + 1:]:
            body = s.get("run", "")
            refs = set(re.findall(r"steps\.([A-Za-z0-9_-]+)\.outputs", body))
            for ref in refs:
                self.assertIn(ref, ids,
                              f"{s['name']!r} reads steps.{ref}, which no earlier step defines")


class TradingSpliceSwallowsItsRefusal(unittest.TestCase):
    """Run the step's own shell, with a stub formatter, and watch what it does."""

    PAGE = (
        "intro\n"
        "<!-- TRADING:BEGIN (generated) -->\n"
        "old trading table\n"
        "<!-- TRADING:END -->\n"
        "outro\n"
    )

    def setUp(self):
        step = steps()[index_of("Splice regenerated trading table into the benchmarks page")]
        self.body = step["run"]
        self.assertNotIn("${{", self.body, "the step grew a context expression; this test cannot run it")
        self.dir = tempfile.mkdtemp()
        os.makedirs(os.path.join(self.dir, "benchmarks"))
        os.makedirs(os.path.join(self.dir, "docs", "internals"))
        self.page = os.path.join(self.dir, "docs", "internals", "benchmarks.md")
        with open(self.page, "w") as f:
            f.write(self.PAGE)
        self.output = os.path.join(self.dir, "github_output")
        open(self.output, "w").close()
        # nproc is in the step's --env string and does not exist everywhere.
        self.bin = os.path.join(self.dir, "bin")
        os.makedirs(self.bin)
        nproc = os.path.join(self.bin, "nproc")
        with open(nproc, "w") as f:
            f.write("#!/bin/sh\necho 4\n")
        os.chmod(nproc, 0o755)

    def write_formatter(self, exit_code, stdout="fresh trading table"):
        path = os.path.join(self.dir, "benchmarks", "format_trading_results.py")
        with open(path, "w") as f:
            f.write(f"import sys\nprint({stdout!r})\nsys.exit({exit_code})\n")

    def run_body(self):
        env = dict(os.environ)
        env["GITHUB_OUTPUT"] = self.output
        env["GITHUB_SHA"] = "deadbeef"
        env["PATH"] = self.bin + os.pathsep + env["PATH"]
        # sh -e is the shell GitHub Actions runs a container step with, which
        # is the whole reason the step needs an explicit `set +e`.
        proc = subprocess.run(["sh", "-e"], input=self.body, cwd=self.dir,
                              env=env, capture_output=True, text=True)
        with open(self.output) as f:
            recorded = f.read()
        with open(self.page) as f:
            page = f.read()
        return proc, recorded, page

    def test_a_refusal_does_not_fail_the_step(self):
        self.write_formatter(1, stdout="")
        proc, recorded, page = self.run_body()
        self.assertEqual(proc.returncode, 0,
                         f"the refusal still kills the job:\n{proc.stdout}\n{proc.stderr}")
        self.assertIn("rc=1", recorded, "the refusal was swallowed without being recorded")
        self.assertEqual(page, self.PAGE, "a refused render overwrote the committed table")

    def test_a_successful_render_is_spliced_in(self):
        self.write_formatter(0)
        proc, recorded, page = self.run_body()
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("rc=0", recorded)
        self.assertIn("fresh trading table", page)
        self.assertNotIn("old trading table", page)
        self.assertIn("intro", page)
        self.assertIn("outro", page)

    def test_the_markers_survive_so_the_next_run_can_splice_again(self):
        self.write_formatter(0)
        self.run_body()
        with open(self.page) as f:
            page = f.read()
        self.assertIn("TRADING:BEGIN", page)
        self.assertIn("TRADING:END", page)


if __name__ == "__main__":
    unittest.main(verbosity=2)
