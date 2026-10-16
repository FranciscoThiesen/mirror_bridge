#!/usr/bin/env python3
"""llms.txt and llms-full.txt are generated, so they have to match the generator.

Both are written by tools/gen_llms_txt.sh: llms.txt from a hand-written list
of link lines, llms-full.txt from a glob over docs/. The hand-written list is
the half that drifts. docs/guides/accelerating-python.md was added to
llms.txt directly and never to the generator, so the next person to run it
would have silently deleted the entry, and nothing would have said so.

Same contract as tests/check_single_header_sync.sh: the generated artifact is
committed, so a run that changes it is a diff someone has to look at.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
GEN = os.path.join(ROOT, "tools", "gen_llms_txt.sh")


class LlmsSync(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.mkdtemp()
        env = dict(os.environ, MB_LLMS_OUT=cls.dir)
        cls.proc = subprocess.run(["bash", GEN], env=env, cwd=ROOT,
                                  capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.dir, ignore_errors=True)

    def test_the_generator_runs(self):
        self.assertEqual(self.proc.returncode, 0, self.proc.stderr)

    def test_it_warns_about_nothing_missing(self):
        # A link to a file that no longer exists is skipped with a warning
        # rather than written as a dead link, so a warning here means the
        # curated list points at something that has moved.
        self.assertNotIn("WARN:", self.proc.stderr, self.proc.stderr)

    def test_the_committed_files_are_what_it_writes(self):
        for name in ("llms.txt", "llms-full.txt"):
            with open(os.path.join(self.dir, name)) as f:
                fresh = f.read()
            with open(os.path.join(ROOT, name)) as f:
                committed = f.read()
            self.assertEqual(
                committed, fresh,
                "%s is out of date. Run ./tools/gen_llms_txt.sh and commit the result." % name)

    def test_every_guide_and_reference_page_reaches_the_full_corpus(self):
        # llms-full.txt is a glob, so this is really a check that the glob
        # still covers the directories docs are being added to.
        with open(os.path.join(self.dir, "llms-full.txt")) as f:
            full = f.read()
        for sub in ("guides", "reference", "getting-started"):
            d = os.path.join(ROOT, "docs", sub)
            if not os.path.isdir(d):
                continue
            pages = [p for p in sorted(os.listdir(d)) if p.endswith(".md")]
            self.assertTrue(pages, sub)
            for page in pages:
                self.assertIn("docs/%s/%s" % (sub, page), full,
                              "docs/%s/%s is in the repo and not in llms-full.txt" % (sub, page))


if __name__ == "__main__":
    unittest.main(verbosity=2)
