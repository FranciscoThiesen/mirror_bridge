#!/usr/bin/env python3
"""Test the stub generator script."""

import os
import sys
import tempfile
import unittest
from pathlib import Path

# Add scripts directory to path
SCRIPT_DIR = Path(__file__).parent.parent / 'scripts'
sys.path.insert(0, str(SCRIPT_DIR))

from generate_stubs import (
    parse_cpp_type,
    split_template_args,
    infer_type_from_value,
    clean_signature_types,
    generate_module_stub,
)


class TestCppTypeMapping(unittest.TestCase):
    """Test C++ to Python type mappings."""

    def test_primitives(self):
        """Test primitive type mappings."""
        self.assertEqual(parse_cpp_type('int'), 'int')
        self.assertEqual(parse_cpp_type('double'), 'float')
        self.assertEqual(parse_cpp_type('float'), 'float')
        self.assertEqual(parse_cpp_type('bool'), 'bool')
        self.assertEqual(parse_cpp_type('void'), 'None')

    def test_string_types(self):
        """Test string type mappings."""
        self.assertEqual(parse_cpp_type('std::string'), 'str')
        self.assertEqual(parse_cpp_type('string'), 'str')
        self.assertEqual(parse_cpp_type('const char*'), 'str')

    def test_const_and_ref(self):
        """Test const and reference qualifiers are stripped."""
        self.assertEqual(parse_cpp_type('const int'), 'int')
        self.assertEqual(parse_cpp_type('int&'), 'int')
        self.assertEqual(parse_cpp_type('const std::string&'), 'str')

    def test_vector(self):
        """Test vector type mappings."""
        self.assertEqual(parse_cpp_type('std::vector<int>'), 'list[int]')
        self.assertEqual(parse_cpp_type('vector<double>'), 'list[float]')
        self.assertEqual(parse_cpp_type('std::vector<std::string>'), 'list[str]')

    def test_map(self):
        """Test map type mappings."""
        self.assertEqual(parse_cpp_type('std::map<std::string, int>'), 'dict[str, int]')
        self.assertEqual(parse_cpp_type('std::unordered_map<int, double>'), 'dict[int, float]')

    def test_set(self):
        """Test set type mappings."""
        self.assertEqual(parse_cpp_type('std::set<int>'), 'set[int]')
        self.assertEqual(parse_cpp_type('std::unordered_set<std::string>'), 'set[str]')

    def test_optional(self):
        """Test optional type mappings."""
        self.assertEqual(parse_cpp_type('std::optional<int>'), 'int | None')

    def test_smart_pointers(self):
        """Test smart pointer types (transparent unwrapping)."""
        self.assertEqual(parse_cpp_type('std::shared_ptr<MyClass>'), 'MyClass')
        self.assertEqual(parse_cpp_type('std::unique_ptr<Widget>'), 'Widget')

    def test_namespaced_class(self):
        """Test namespaced class types."""
        self.assertEqual(parse_cpp_type('my::ns::MyClass'), 'MyClass')
        self.assertEqual(parse_cpp_type('foo::bar::baz::Type'), 'Type')

    def test_unknown_types(self):
        """Test unknown types are passed through."""
        self.assertEqual(parse_cpp_type('CustomType'), 'CustomType')


class TestTemplateArgSplitting(unittest.TestCase):
    """Test template argument splitting."""

    def test_simple_split(self):
        """Test splitting simple arguments."""
        self.assertEqual(split_template_args('int, double'), ['int', 'double'])
        self.assertEqual(split_template_args('std::string, int'), ['std::string', 'int'])

    def test_nested_templates(self):
        """Test splitting with nested templates."""
        self.assertEqual(
            split_template_args('std::vector<int>, std::string'),
            ['std::vector<int>', 'std::string']
        )
        self.assertEqual(
            split_template_args('std::map<std::string, int>, bool'),
            ['std::map<std::string, int>', 'bool']
        )


class TestTypeInference(unittest.TestCase):
    """Test runtime type inference."""

    def test_primitives(self):
        """Test inferring primitive types."""
        self.assertEqual(infer_type_from_value(42), 'int')
        self.assertEqual(infer_type_from_value(3.14), 'float')
        self.assertEqual(infer_type_from_value(True), 'bool')
        self.assertEqual(infer_type_from_value('hello'), 'str')
        self.assertEqual(infer_type_from_value(None), 'None')

    def test_collections(self):
        """Test inferring collection types."""
        self.assertEqual(infer_type_from_value([1, 2, 3]), 'list[int]')
        self.assertEqual(infer_type_from_value(['a', 'b']), 'list[str]')
        self.assertEqual(infer_type_from_value([]), 'list')
        self.assertEqual(infer_type_from_value({}), 'dict')
        self.assertEqual(infer_type_from_value(set()), 'set')


class TestSignatureCleaning(unittest.TestCase):
    """Test signature cleaning."""

    def test_std_namespace_removal(self):
        """Test std:: prefix removal and type mapping."""
        result = clean_signature_types('(arg: std::string)')
        # std::string is mapped to str
        self.assertIn('str', result)
        self.assertNotIn('std::', result)

    def test_vector_cleaning(self):
        """Test vector type cleaning."""
        result = clean_signature_types('(items: std::vector<int>)')
        self.assertNotIn('std::', result)


class TestTheModuleAnswersForItself(unittest.TestCase):
    """A mirror_bridge module carries its own stubs, and those win.

    This script reconstructs a stub by importing the finished .so and
    reading docstrings off it. The module built by mirror_bridge already
    holds one written from the reflection the binding was written from,
    which knows the real C++ parameter names, the exact member types, and
    that a numeric vector comes back as an array.array. Two generators, two
    answers, and the reconstruction was the wrong one every time they
    differed. It is now the fallback.
    """

    class Carrier:
        __name__ = "carrier"

        @staticmethod
        def __mirror_bridge_stubs__():
            return "class Shapes:\n    def doubles(self) -> _array[float]: ...\n"

    class Bare:
        __name__ = "bare"

    def test_a_module_with_stubs_is_quoted_verbatim(self):
        out = generate_module_stub(self.Carrier, "carrier")
        self.assertEqual(out, self.Carrier.__mirror_bridge_stubs__())
        self.assertIn("_array[float]", out)

    def test_a_module_without_them_still_gets_the_reconstruction(self):
        out = generate_module_stub(self.Bare, "bare")
        self.assertIn("Type stubs for bare", out)

    def test_a_carrier_that_raises_falls_back_rather_than_failing(self):
        class Broken:
            @staticmethod
            def __mirror_bridge_stubs__():
                raise RuntimeError("no")

        out = generate_module_stub(Broken, "broken")
        self.assertIn("Type stubs for broken", out)

    def test_an_empty_answer_is_not_taken_for_a_stub(self):
        class Empty:
            @staticmethod
            def __mirror_bridge_stubs__():
                return "   "

        self.assertIn("Type stubs for empty", generate_module_stub(Empty, "empty"))


if __name__ == '__main__':
    unittest.main()
