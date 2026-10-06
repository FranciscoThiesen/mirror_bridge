#!/bin/bash
# Class discovery when something stands between the keyword and the name.
#
# `class EXPORTLIB_API Widget` is how a shipped C++ library declares a class.
# A scan of the header text reads EXPORTLIB_API as the class name and the
# module binds copies of the macro; discovery goes through reflection, which
# reads the headers after the preprocessor has run. The same scan takes
# `alignas` for a class name and loses `struct [[nodiscard]] X` entirely,
# neither of which involves the preprocessor at all.
set -e
cd "$(dirname "$0")"

MB_TOOL="$(cd ../.. && pwd)/tools/mirror_bridge"

rm -rf build
out=$("$MB_TOOL" generate src --module exportlib_py --lang python --output build --force 2>&1)
echo "$out" | sed -e 's/\x1b\[[0-9;]*m//g'

fail=0
expect() {
    if [[ "$out" == *"$2"* ]]; then echo "  ✓ $1"; else echo "  ✗ $1 (expected: $2)"; fail=1; fi
}
reject() {
    if [[ "$out" != *"$2"* ]]; then echo "  ✓ $1"; else echo "  ✗ $1 (unexpected: $2)"; fail=1; fi
}

echo ""
expect "the class is found under its own name"      "Found: exportlib::Widget in exportlib.hpp"
reject "the export macro is not taken for a class"  "EXPORTLIB_API"
expect "a public nested class is found"             "Found: exportlib::Widget::Handle"
reject "a private nested class is not"              "Widget::Secret"
expect "mutually aliasing classes both appear"      "Found: exportlib::WidgetCursor"
expect "alignas is not taken for a class name"      "Found: exportlib::Wide in exportlib.hpp"
reject "and no class called alignas is bound"       "alignas"
expect "an attributed class is not lost"            "Found: exportlib::Tagged in exportlib.hpp"
expect "a final class is found"                     "Found: exportlib::Sealed in exportlib.hpp"
reject "MIRROR_BRIDGE_SKIP is honoured"             "exportlib::Internal"
expect "the enum is reported rather than bound"     "Status"
expect "the list came from the compiler"            "(reflection)"

python3 test_export_macro.py
[ "$fail" -eq 0 ]
