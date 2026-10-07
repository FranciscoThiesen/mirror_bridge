#!/bin/bash
# What the reflection walk is allowed to see, and what the CLI does when it
# sees nothing.
#
# Discovery reads the headers after the preprocessor has run, which means it
# reads the whole translation unit: every system header, and every third-party
# header the project includes. Only the project's own classes may reach the
# module. The three cases here are the ways that went wrong:
#
#   src/             a vendored header of the same name, in the same namespace,
#                    must not contribute classes to the module
#   unreachable/     reflection compiles and reports nothing, so the text
#                    scan's answer has to stand rather than the module coming
#                    out empty
#   templates_only/  an empty class list is ordinary and must not break the CLI
set -e
cd "$(dirname "$0")"

HERE="$(pwd)"
MB_TOOL="$(cd ../.. && pwd)/tools/mirror_bridge"

rm -rf build

fail=0
expect() {
    if [[ "$out" == *"$2"* ]]; then echo "  ✓ $1"; else echo "  ✗ $1 (expected: $2)"; fail=1; fi
}
reject() {
    if [[ "$out" != *"$2"* ]]; then echo "  ✓ $1"; else echo "  ✗ $1 (unexpected: $2)"; fail=1; fi
}

# Every generate below is allowed to fail: a failure is a result to assert on,
# not a reason to stop reporting the rest.
#
# --output is nested on purpose. A path whose parent does not exist yet cannot
# be resolved by plain realpath, and a relative output directory puts the
# planner's work where the compiler cannot reach it, which turned the whole
# reflection pass off and blamed the headers for it.

# --------------------------------------------------------------------------
echo "A vendored header of the same name does not join the module"
# --------------------------------------------------------------------------
out=$("$MB_TOOL" generate src --module discscope_py --lang python --no-templates \
      --output build/scope --force -I "$HERE/vendor" 2>&1) || true
echo "$out" | sed -e 's/\x1b\[[0-9;]*m//g'

echo ""
expect "the project's own class is found"     "Found: lib::MyConfig in config.h"
expect "the list came from the compiler"      "(reflection)"
reject "the vendored class is not discovered" "VendorInternal"
reject "nor the one beside it"                "VendorOther"
expect "the module builds"                    "Successfully built bindings"

python3 - <<'PY' || fail=1
import sys

sys.path.insert(0, "build/scope")
import discscope_py

c = discscope_py.MyConfig()
assert c.n == 1, c.n
assert c.twice() == 2, c.twice()
for absent in ("VendorInternal", "VendorOther"):
    assert not hasattr(discscope_py, absent), f"{absent} must not be in the module"
print("  ✓ the module exposes the project's class and neither vendored one")
PY

# --------------------------------------------------------------------------
echo ""
echo "Reflection reporting nothing falls back instead of emptying the module"
# --------------------------------------------------------------------------
out=$("$MB_TOOL" generate unreachable --module discunreach_py --lang python --no-templates \
      --output build/unreachable --force 2>&1) || true
echo "$out" | sed -e 's/\x1b\[[0-9;]*m//g'

echo ""
expect "the empty walk is announced"          "found no class in them"
expect "the text scan's answer is used"       "(text scan)"
expect "the class is still bound"             "Found: Thing"
expect "the module still builds"              "Successfully built bindings"
reject "it is not blamed on a failed compile" "could not read these headers"

# --------------------------------------------------------------------------
echo ""
echo "A header set with no classes at all still generates"
# --------------------------------------------------------------------------
rc=0
out=$("$MB_TOOL" generate templates_only --module disctmpl_py --lang python \
      --output build/templates --force 2>&1) || rc=$?
echo "$out" | sed -e 's/\x1b\[[0-9;]*m//g'

echo ""
if [ "$rc" -eq 0 ]; then
    echo "  ✓ exits 0"
else
    echo "  ✗ expected exit 0, got $rc"
    fail=1
fi
reject "the CLI does not trip over the empty class list" "bad array subscript"
expect "the template instantiation is planned"           "across 1 templates"
expect "the module builds"                               "Successfully built bindings"

echo ""
[ "$fail" -eq 0 ]
