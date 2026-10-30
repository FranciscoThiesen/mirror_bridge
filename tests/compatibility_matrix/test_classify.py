#!/usr/bin/env python3
"""The usability bar, the regression ladder and the failure classifier.

These three decide whether a corpus run passes and what the published page
says went wrong, so they are worth pinning on their own: the rest of
run_compatibility.py needs clones and a reflection toolchain, and nothing was
checking the judgement itself.
"""
import os
import sys
import unittest

# The module under test lives in compatibility/. The test lives here because
# tests/run_all_tests.sh is what CI runs and it only looks under tests/, so a
# test next to its module would never have run -- which is how the classifier
# shipped the wrong cause for a week. Same arrangement as
# tests/benchmark_table/, which tests benchmarks/.
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "compatibility"))
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


class TestLinkFailureIsNotADiscoveryFailure(unittest.TestCase):
    """A link failure proves discovery and codegen both worked.

    tinyobjloader was published as "reflection could not read these headers"
    when the truth is that it is not header-only. Two bugs stacked: the
    classifier had no rule for `undefined reference`, so clang's "linker
    command failed with exit code 1" matched the generic \berror\b rule, and
    then the text-scan override replaced that with reflection-tu-failed.
    """

    LINK = ("tinyobj_compat_shim.cpp:(.text+0x8238): undefined reference to "
            "`vtable for tinyobj::MaterialFileReader'\n"
            "clang++: error: linker command failed with exit code 1\n")

    def test_undefined_reference_is_needs_linking(self):
        category, reason, _ = rc.classify_failure(None, self.LINK)
        self.assertEqual(category, "needs-linking")
        self.assertIn("link-args", reason)

    # The rule has to sit ABOVE the generic one, or clang's own summary line
    # wins because it contains the word error.
    def test_it_beats_the_generic_compile_error_rule(self):
        category, _, _ = rc.classify_failure(
            None, "clang++: error: linker command failed with exit code 1\n"
                  "/usr/bin/ld: undefined symbol: foo\n")
        self.assertEqual(category, "needs-linking")

    def test_a_real_compile_error_is_still_a_compile_error(self):
        category, _, _ = rc.classify_failure(
            None, "shim.cpp:12:3: error: no matching member function for call "
                  "to 'evaluate_string'\n")
        self.assertEqual(category, "compile-error")


class TestArtifactName(unittest.TestCase):
    """Which file existing means "a module was built", per backend.

    This had one answer, Python's, so `os.path.isfile(<module>.so)` was False
    for every C# library whether or not the build succeeded -- and "0/32 bind"
    was a statement about that line rather than about the backend. Adding
    --lang threaded the flag through run_one and left this behind.
    """

    def test_python_and_lua_emit_a_plain_so(self):
        self.assertEqual(rc.module_artifact("jsoncpp_compat", "python"),
                         "jsoncpp_compat.so")
        self.assertEqual(rc.module_artifact("jsoncpp_compat", "lua"),
                         "jsoncpp_compat.so")

    def test_node_emits_a_node_addon(self):
        self.assertEqual(rc.module_artifact("jsoncpp_compat", "js"),
                         "jsoncpp_compat.node")

    # The whole point: C# writes lib<module>.so, so the Python spelling can
    # never match and the row can never say built.
    def test_csharp_emits_a_lib_prefixed_so(self):
        self.assertEqual(rc.module_artifact("jsoncpp_compat", "csharp"),
                         "libjsoncpp_compat.so")

    def test_no_backend_shares_csharp_spelling(self):
        for lang in ("python", "lua", "js"):
            self.assertNotEqual(rc.module_artifact("m", lang),
                                rc.module_artifact("m", "csharp"))


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
        with open(os.path.join(ROOT, "compatibility", "libraries.json")) as f:
            corpus = json.load(f)["corpus"]
        self.assertGreater(len(corpus), 0)
        for lib in corpus:
            self.assertTrue(lib.get("must_export"),
                            "%s declares no must_export" % lib["name"])
            self.assertIn(lib.get("expect"), rc.EXPECT_RANK,
                          "%s has an unknown expect value %r" % (lib["name"], lib.get("expect")))


class TestFailureClassifier(unittest.TestCase):
    """What the matrix prints in the "Why not, if not" column.

    A category is a claim that we know what the problem is, so a wrong one
    is worse than `compile-error`: it sends the next reader after the wrong
    bug. jsoncpp was published as a name collision for a week while the real
    failure was a conversion two thousand lines away.
    """

    def test_the_cli_duplicate_name_warning_is_not_a_cause(self):
        # The CLI prints this, picks the class in the shallowest namespace
        # and carries on binding, so the line is advice. Here it sits above
        # an unrelated compile error, which is jsoncpp's exact shape.
        out = ("Discovered 23 classes (reflection)\n"
               "\u26a0 'Factory' is the binding name of 2 classes:\n"
               "      Json::StreamWriter::Factory (writer.h:58)\n"
               "      Json::CharReader::Factory (reader.h:266)\n"
               "Compiling python module...\n"
               "mirror_bridge_python.hpp:2085:33: error: no matching conversion "
               "for static_cast from 'const Value' to 'ValueType'\n")
        category, _, _ = rc.classify_failure(None, out)
        self.assertNotEqual(category, "duplicate-binding-name")
        self.assertEqual(category, "compile-error")

    def test_a_real_redefinition_is_a_cause(self):
        out = "foo.cpp:12:8: error: redefinition of 'Factory'\n"
        self.assertEqual(rc.classify_failure(None, out)[0], "duplicate-binding-name")

    def test_the_export_macro_warning_still_outranks_the_error_it_causes(self):
        # Unlike the duplicate name, this one is the root cause: discovery
        # named a macro, so the binding it wrote could never compile.
        out = ("\u26a0 'TINYXML2_LIB' is the binding name of 3 classes:\n"
               "error: unknown type name 'TINYXML2_LIB'\n")
        self.assertEqual(rc.classify_failure(None, out)[0], "export-macro-discovery")

    def test_a_specific_signature_beats_the_catch_all(self):
        out = "error: no matching function for call to 'from_python'\n"
        self.assertEqual(rc.classify_failure(None, out)[0], "no-converter-for-parameter")

    def test_output_with_no_error_is_unknown_rather_than_guessed(self):
        self.assertEqual(rc.classify_failure(None, "Built fine.\n")[0], "unknown")

    def test_discovery_finding_nothing_is_its_own_answer(self):
        cli = {"status": "error", "stage": "discover", "reason": "No classes found"}
        self.assertEqual(rc.classify_failure(cli, "")[0], "no-classes-found")

    def test_every_signature_has_a_category_a_reader_can_act_on(self):
        for category, pattern, reason, _flags in rc.FAILURE_SIGNATURES:
            self.assertTrue(category and reason, category)
            self.assertNotIn("_", category, "%s should be kebab-case" % category)


if __name__ == "__main__":
    unittest.main(verbosity=2)
