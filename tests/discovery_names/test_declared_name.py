#!/usr/bin/env python3
"""What the header scan reads as a class name, with no compiler involved.

`mirror_bridge generate` discovers classes through reflection, which reads the
headers after the preprocessor and gets this right by construction. The scan of
the header text is the fallback for headers reflection cannot read -- which is
exactly the set of libraries where getting it wrong is expensive, because there
is nothing behind it. It was wrong: it read the identifier straight after the
keyword, so `class CV_EXPORTS Mat` was a class called CV_EXPORTS. The
compatibility corpus recorded the consequence as one line per library:

    OpenCV      'CV_EXPORTS' is the binding name of 70 classes
    googletest  'GTEST_API_' is the binding name of 38 classes
    doctest     'DOCTEST_INTERFACE' is the binding name of 48 classes
    range-v3    'RANGES_STRUCT_WITH_ADL_BARRIER' is the binding name of 4
    abseil      'ABSL_LOCKABLE' is the binding name of 3 classes

Every shape below is taken from a real library in that corpus. The scanner is
read out of the CLI rather than copied, so this cannot pass against a copy that
has drifted from what ships.
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
CLI = os.path.join(ROOT, "tools", "mirror_bridge")


def embedded_scanner():
    """The class scan as it ships, lifted out of the CLI's heredoc."""
    with open(CLI, encoding="utf-8") as f:
        src = f.read()
    m = re.search(r"parser_script=\$\(cat <<'PYSCRIPT'\n(.*?)\nPYSCRIPT", src, re.S)
    assert m, "the class scan is no longer a PYSCRIPT heredoc in tools/mirror_bridge"
    return m.group(1)


SCANNER = embedded_scanner()


def scan(header_text):
    """{name: kind} for one header's worth of text."""
    with tempfile.TemporaryDirectory() as d:
        script = os.path.join(d, "scan.py")
        with open(script, "w") as f:
            f.write(SCANNER)
        header = os.path.join(d, "probe.hpp")
        with open(header, "w") as f:
            f.write(header_text)
        r = subprocess.run([sys.executable, script, header],
                           capture_output=True, text=True)
        assert r.returncode == 0, r.stderr
    out = {}
    for line in r.stdout.splitlines():
        if not line.strip():
            continue
        parts = line.split("\t")
        out[parts[0]] = parts[2] if len(parts) > 2 else "class"
    return out


class TestNameBetweenKeywordAndBrace(unittest.TestCase):
    def assertNames(self, text, expected):
        found = sorted(n for n, kind in scan(text).items() if kind == "class")
        self.assertEqual(found, sorted(expected))

    def test_export_macro_is_not_the_class_name(self):
        self.assertNames("class CV_EXPORTS Mat { public: int rows; };", ["Mat"])
        self.assertNames("class GTEST_API_ Test { public: int id; };", ["Test"])
        self.assertNames("struct DOCTEST_INTERFACE Options { int flags; };", ["Options"])
        self.assertNames("class ABSL_LOCKABLE Mutex { public: int held; };", ["Mutex"])
        self.assertNames("class RANGES_STRUCT_WITH_ADL_BARRIER B { public: int v; };", ["B"])

    def test_two_macros_before_the_name(self):
        # OpenCV spells some classes `class CV_EXPORTS_W CV_FINAL Foo`.
        self.assertNames("class CV_EXPORTS_W CV_WRAP Foo { public: int v; };", ["Foo"])

    def test_macro_after_the_name_is_not_the_class_name(self):
        # `#define CV_FINAL final` sits after the name, not before it, so the
        # last identifier in the head is the wrong answer here and the right
        # one in every case above. ALL_CAPS is what tells the two apart.
        self.assertNames("class Named CV_FINAL { public: int v; };", ["Named"])
        self.assertNames("class CV_EXPORTS Both CV_FINAL { public: int v; };", ["Both"])

    def test_function_like_specifiers(self):
        self.assertNames("class __declspec(dllexport) Exported { public: int v; };",
                         ["Exported"])
        self.assertNames('struct __attribute__((visibility("default"))) Vis { int v; };',
                         ["Vis"])
        self.assertNames("struct alignas(64) Wide { double a; };", ["Wide"])
        self.assertNames("class CV_EXPORTS_AS(fn) Renamed { public: int v; };",
                         ["Renamed"])

    def test_standard_attribute_does_not_lose_the_class(self):
        # This one was not merely misnamed: the old pattern required an
        # identifier straight after the keyword, so an attributed class matched
        # nothing at all and vanished from discovery without a word.
        self.assertNames("struct [[nodiscard]] Tagged { int tag; };", ["Tagged"])

    def test_final_is_not_a_class_name(self):
        self.assertNames("class EXPORT_API Sealed final { public: int s; };", ["Sealed"])
        self.assertNames("struct Plain final { int v; };", ["Plain"])

    def test_an_upper_case_class_name_survives(self):
        # The ALL_CAPS rule must not cost a library a real name. Nothing else
        # is in the head, so there is nothing to prefer over it.
        self.assertNames("struct SHA256 { int state; };", ["SHA256"])
        self.assertNames("class CV_EXPORTS SHA256 { public: int state; };", ["SHA256"])

    def test_plain_declarations_are_unchanged(self):
        self.assertNames("struct Plain { int v; };", ["Plain"])
        self.assertNames("struct Base { int v; };\nclass D : public Base { public: int w; };",
                         ["Base", "D"])

    def test_nested_class_behind_a_macro(self):
        text = ("class CV_EXPORTS Outer {\n"
                "public:\n"
                "  class CV_EXPORTS Inner { public: int v; };\n"
                "};\n")
        self.assertNames(text, ["Outer", "Outer::Inner"])

    def test_private_nested_class_is_still_withheld(self):
        # Binding an inaccessible type is ill-formed, and resolving the name
        # through the macro must not reach past the access check.
        text = ("class CV_EXPORTS Outer {\n"
                "  class CV_EXPORTS Secret { public: int v; };\n"
                "};\n")
        self.assertNames(text, ["Outer"])

    def test_forward_declaration_is_not_a_class(self):
        self.assertNames("class CV_EXPORTS Later;\nstruct Here { int v; };", ["Here"])

    def test_elaborated_type_specifier_is_not_a_declaration(self):
        self.assertNames("struct Real { int v; };\nvoid f(class Real* p);", ["Real"])

    def test_anonymous_struct_names_nothing(self):
        self.assertNames("struct { int v; } instance;\nstruct Named { int v; };", ["Named"])

    def test_enum_is_reported_as_an_enum_not_a_class(self):
        found = scan("enum class Status { Idle, Busy };\nstruct S { int v; };")
        self.assertEqual(found.get("Status"), "enum")
        self.assertEqual(found.get("S"), "class")

    def test_template_class_is_not_reported_directly(self):
        # A template needs instantiating before it can be bound, so the scan
        # reports nothing for it -- including when a macro follows the head.
        self.assertNames("template <class T> class CV_EXPORTS Mat_ { public: T v; };\n"
                         "struct Plain { int v; };", ["Plain"])

    def test_macro_name_inside_a_comment_or_string_is_ignored(self):
        text = ('// class CV_EXPORTS Commented { };\n'
                'const char* s = "class CV_EXPORTS Quoted { }";\n'
                'struct Real { int v; };\n')
        self.assertNames(text, ["Real"])


class TestTemplateHintKeys(unittest.TestCase):
    """The planner keys template parameter shapes by name, and the same macro
    sat between the keyword and that name. A hint filed under CV_EXPORTS is a
    hint the planner never finds."""

    @classmethod
    def setUpClass(cls):
        sys.path.insert(0, os.path.join(ROOT, "tools"))
        import mirror_bridge_templates
        cls.scan_hints = staticmethod(mirror_bridge_templates.scan_hints)

    def test_hint_is_keyed_by_the_class_name(self):
        self.assertEqual(self.scan_hints("template <class T> class CV_EXPORTS Mat_ { };"),
                         {"Mat_": "T"})

    def test_non_type_parameter_shape_survives_the_macro(self):
        self.assertEqual(self.scan_hints("template <typename T, int N> struct GTEST_API_ Fixed { };"),
                         {"Fixed": "TV"})

    def test_specifiers_and_attributes(self):
        self.assertEqual(self.scan_hints("template <class T> class __declspec(dllexport) E { };"),
                         {"E": "T"})
        self.assertEqual(self.scan_hints("template <class T> struct alignas(32) A { };"),
                         {"A": "T"})
        self.assertEqual(self.scan_hints("template <class T> struct [[nodiscard]] N { };"),
                         {"N": "T"})

    def test_upper_case_template_name_survives(self):
        self.assertEqual(self.scan_hints("template <class T> struct SHA256 { };"),
                         {"SHA256": "T"})

    def test_pack_and_defaulted_shapes_are_unchanged(self):
        self.assertEqual(self.scan_hints("template <class... Ts> struct DOCTEST_INTERFACE P { };"),
                         {"P": "P"})
        self.assertEqual(self.scan_hints("template <class T, class U = int> class ABSL_LOCKABLE Two { };"),
                         {"Two": "TD"})

    def test_function_template_still_scans(self):
        self.assertEqual(self.scan_hints("template <class T> T addone(T v);"),
                         {"addone": "T"})


if __name__ == "__main__":
    unittest.main(verbosity=2)
