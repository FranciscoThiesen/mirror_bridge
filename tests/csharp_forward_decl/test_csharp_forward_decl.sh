#!/bin/bash
# A type that is declared and never defined must be reported, not crash the
# generator.
#
# std::is_class_v is TRUE for a forward-declared class, and core::Bindable only
# adds a reflection query an incomplete type can still satisfy, so such a type
# reached typeid -- which does not compile for an incomplete type. The failure
# named no method of the user's library: it pointed into mirror_bridge's own
# source. yaml-cpp's YAML::detail::iterator_value is the real one.
#
# This lives apart from tests/csharp_bindings because that fixture is SHARED:
# tests/constexpr_budget builds it with --lang python, and the Python backend
# still has this gap. Do not move these shapes back into the shared header.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL="$SCRIPT_DIR/../../tools/mirror_bridge"
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

pass=0
fail=0

assert_contains() {
    [[ "$1" == *"$2"* ]]
}

check() {
    local name="$1"
    shift
    if "$@"; then
        echo "  PASS: $name"
        ((pass++))
    else
        echo "  FAIL: $name"
        ((fail++))
    fi
}

echo "Testing a forward-declared type in a bound header:"

out=$("$TOOL" generate "$SCRIPT_DIR" --module fwd --lang csharp -o "$TMPDIR/out" 2>&1)
rc=$?

check "the module builds rather than crashing the generator" test $rc -eq 0
check "a method returning an incomplete type is reported" \
    assert_contains "$out" "Forward.make(): returns"
check "and says it is declared but not defined" \
    assert_contains "$out" "declared but not defined"
check "a method taking one is reported too" \
    assert_contains "$out" "Forward.take(): takes"
check "the rest of the class still binds" \
    grep -q "public int doubled()" "$TMPDIR/out/fwd.cs"

# The property that distinguishes a reported skip from a generator crash: the
# tool must not be naming its own source file.
check "the generator never mentions its own source" \
    bash -c '! grep -q "mirror_bridge_csharp.hpp" <<< "$0"' "$out"

echo ""
if [ "$fail" -eq 0 ]; then
    echo "All $pass forward-declaration tests passed!"
else
    echo "$fail of $((pass + fail)) tests FAILED"
    exit 1
fi
