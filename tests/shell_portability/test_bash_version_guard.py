#!/usr/bin/env python3
"""A script that needs bash 4 has to say so on bash 3.

macOS ships bash 3.2 from 2007 and never will ship a newer one. Every
user-facing script here uses associative arrays or name references, both
bash 4, and without a guard the first one fails with

    declare: -A: invalid option

which was the entire output of every subcommand including `doctor`. The
docs list macOS as supported and tell the reader to run
`./tools/mirror_bridge`, and all fifteen CI `runs-on` entries are
`ubuntu-latest`, so nothing was ever going to catch it.

The rule this pins: any shell script in the repo that uses bash 4 syntax
checks BASH_VERSINFO before reaching it. That is checkable without a bash 3
to run, which matters because CI has none.
"""
import os
import re
import subprocess
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

# Syntax that bash 3.2 rejects. Name references and associative arrays are
# the two this project actually uses.
BASH4 = re.compile(r"^\s*(?:local|declare|typeset)\s+-[A-Za-z]*[An]", re.M)

SKIP_DIRS = {".git", ".worktrees", "build", "node_modules", "__pycache__"}

# packaging/pip/src/mirror_bridge_cli/_runtime/ is not tracked: build_wheel.sh
# copies the real scripts into it, so checking the copies would report the
# same script twice and go stale on its own.
SKIP_PATHS = (os.path.join("packaging", "pip", "src", "mirror_bridge_cli", "_runtime"),)


def shell_scripts():
    for base, dirs, files in os.walk(ROOT):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not d.startswith(".")]
        if any(part in base for part in SKIP_PATHS):
            continue
        for name in files:
            path = os.path.join(base, name)
            if name.endswith(".sh"):
                yield path
                continue
            # Extensionless executables with a bash shebang, which is what
            # the three user-facing CLIs are.
            if not os.access(path, os.X_OK) or "." in name:
                continue
            try:
                with open(path, "rb") as f:
                    if f.read(2) != b"#!":
                        continue
                    f.seek(0)
                    first = f.readline().decode("utf-8", "replace")
            except OSError:
                continue
            if "bash" in first:
                yield path


class BashVersionGuard(unittest.TestCase):
    def setUp(self):
        self.scripts = sorted(shell_scripts())
        self.assertTrue(self.scripts, "found no shell scripts to check")

    def test_every_bash4_script_guards_its_version(self):
        unguarded = []
        for path in self.scripts:
            with open(path) as f:
                body = f.read()
            if not BASH4.search(body):
                continue
            # The guard has to come before the first bash 4 construct, or it
            # never runs.
            guard = body.find("BASH_VERSINFO")
            first_use = BASH4.search(body).start()
            if guard == -1 or guard > first_use:
                unguarded.append(os.path.relpath(path, ROOT))
        self.assertEqual(unguarded, [],
                         "these use bash 4 syntax with no version check above it: %s"
                         % ", ".join(unguarded))

    def test_the_user_facing_clis_do_not_hardcode_bin_bash(self):
        # /bin/bash on macOS is the 3.2 that cannot run them. env picks up a
        # newer one when the user has installed it.
        wrong = []
        for name in ("tools/mirror_bridge", "mirror_bridge_doctor", "mirror_bridge_auto"):
            path = os.path.join(ROOT, name)
            if not os.path.exists(path):
                continue
            with open(path) as f:
                shebang = f.readline().strip()
            if shebang != "#!/usr/bin/env bash":
                wrong.append("%s has %r" % (name, shebang))
        self.assertEqual(wrong, [], "; ".join(wrong))

    def test_the_guard_says_what_to_do_about_it(self):
        with open(os.path.join(ROOT, "tools", "mirror_bridge")) as f:
            head = f.read(4000)
        for needle in ("bash 4", "brew install bash", "start_dev_container.sh"):
            self.assertIn(needle, head,
                          "the guard does not mention %r, so a reader is told what is "
                          "wrong and not what to do" % needle)

    def test_every_script_parses(self):
        # bash -n on the running bash: catches a syntax error the guard would
        # otherwise mask by exiting first.
        broken = []
        for path in self.scripts:
            p = subprocess.run(["bash", "-n", path], capture_output=True, text=True)
            if p.returncode != 0:
                broken.append("%s: %s" % (os.path.relpath(path, ROOT),
                                          p.stderr.strip().split("\n")[0]))
        self.assertEqual(broken, [], "; ".join(broken))


if __name__ == "__main__":
    unittest.main(verbosity=2)
