#!/usr/bin/env python3
"""The usability bar and the regression ladder, without a compiler.

These two decide whether a corpus run passes, so they are worth pinning on
their own: the rest of run_compatibility.py needs clones and a reflection
toolchain, and nothing was checking the judgement itself.
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run_compatibility as rc


class TestUsabilityBar(unittest.TestCase):
    def test_all_present_is_ok(self):
        v = rc.classify_exports(["Value", "Reader"], ["Reader", "Value", "Writer"])
        self.assertEqual(v["status"], "ok")
        self.assertEqual(v["must_export_missing"], [])

    def test_none_present_is_token(self):
        # entt: three internal tag types and nothing a caller can use.
        v = rc.classify_exports(["registry"], ["group_descriptor", "null_t", "tombstone_t"])
        self.assertEqual(v["status"], "token-binding")
        self.assertEqual(v["must_export_missing"], ["registry"])
        self.assertIn("registry", v["reason"])

    def test_some_present_is_partial(self):
        # box2d: a world can be created and nothing can move in it.
        v = rc.classify_exports(["b2World", "b2Body"], ["b2World", "b2BodyDef", "b2Vec2"])
        self.assertEqual(v["status"], "partial-binding")
        self.assertEqual(v["must_export_present"], ["b2World"])
        self.assertEqual(v["must_export_missing"], ["b2Body"])

    def test_no_bar_declared_stays_ok(self):
        # An entry with no must_export is not silently failed -- but --validate
        # refuses such an entry, so this is only the library-less default.
        v = rc.classify_exports([], ["anything"])
        self.assertEqual(v["status"], "ok")

    def test_empty_module_with_a_bar_is_token(self):
        v = rc.classify_exports(["App"], [])
        self.assertEqual(v["status"], "token-binding")

    def test_a_name_that_merely_contains_the_wanted_one_does_not_count(self):
        # b2BodyDef must not satisfy a requirement for b2Body.
        v = rc.classify_exports(["b2Body"], ["b2BodyDef", "b2BodyUserData"])
        self.assertEqual(v["status"], "token-binding")


class TestRegressionLadder(unittest.TestCase):
    def test_ranks_are_ordered(self):
        self.assertGreater(rc.outcome_rank("ok"), rc.outcome_rank("partial-binding"))
        self.assertGreater(rc.outcome_rank("partial-binding"), rc.outcome_rank("token-binding"))
        self.assertGreater(rc.outcome_rank("token-binding"), rc.outcome_rank("failed"))

    def test_expect_ranks_line_up_with_outcomes(self):
        # Raising the bar must not read as a regression: tinyxml2 still builds
        # the module it always built, so `expect: partial` against
        # partial-binding is not a failure.
        self.assertEqual(rc.EXPECT_RANK["partial"], rc.outcome_rank("partial-binding"))
        self.assertEqual(rc.EXPECT_RANK["bind"], rc.outcome_rank("ok"))
        self.assertEqual(rc.EXPECT_RANK["token"], rc.outcome_rank("token-binding"))

    def test_unknown_expectation_can_never_regress(self):
        for status in ("ok", "partial-binding", "token-binding", "failed", "timeout"):
            self.assertGreaterEqual(rc.outcome_rank(status), rc.EXPECT_RANK["unknown"])


class TestManifest(unittest.TestCase):
    def test_every_library_declares_a_bar_and_an_expectation(self):
        import json
        here = os.path.dirname(os.path.abspath(__file__))
        corpus = json.load(open(os.path.join(here, "libraries.json")))["corpus"]
        self.assertGreater(len(corpus), 0)
        for lib in corpus:
            self.assertTrue(lib.get("must_export"),
                            "%s declares no must_export" % lib["name"])
            self.assertIn(lib.get("expect"), rc.EXPECT_RANK,
                          "%s has an unknown expect value %r" % (lib["name"], lib.get("expect")))


if __name__ == "__main__":
    unittest.main(verbosity=2)
