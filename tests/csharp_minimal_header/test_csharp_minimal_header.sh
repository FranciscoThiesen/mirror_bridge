#!/bin/bash
# The generated shim has to name every header it uses, not inherit them from
# the bound header.
#
# A std::array member deliberately registers a VectorPlan for std::vector<E>,
# so that an array and a vector of one element type share one view class in the
# C#. That makes the shim mention std::vector even when the bound header never
# does -- and when the shim's preamble did not include <vector>, a header whose
# only container was a std::array generated a module that could not compile,
# with the run reporting nothing left out. The 25-class fixture never caught it
# because it includes <vector> for unrelated reasons.
#
# So this test's whole point is a header that includes as little as possible.
# Do not add includes to it.

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

echo "Testing a header with the fewest possible includes:"

mkdir -p "$TMPDIR/arr"
cat > "$TMPDIR/arr/arr.hpp" << 'HDR'
#pragma once
#include <array>

struct OnlyArray {
    std::array<double, 3> xs{1.0, 2.0, 3.0};
    double total() const { return xs[0] + xs[1] + xs[2]; }
};
HDR

out=$("$TOOL" generate "$TMPDIR/arr" --module onlyarr --lang csharp -o "$TMPDIR/arrout" 2>&1)
rc=$?
check "a header whose only container is std::array builds" test $rc -eq 0
check "and the shim was produced" test -f "$TMPDIR/arrout/onlyarr_shim.cpp"
check "and the library with it" test -f "$TMPDIR/arrout/libonlyarr.so"
check "nothing was reported as left out" \
    assert_contains "$out" "0 member(s) or method(s) left out"
check "the shim includes vector, which the header never mentioned" \
    grep -q '#include <vector>' "$TMPDIR/arrout/onlyarr_shim.cpp"
check "and array too" \
    grep -q '#include <array>' "$TMPDIR/arrout/onlyarr_shim.cpp"

# The same property in the other direction: a header with only a std::string
# member must not need <vector> to have been included by accident either.
mkdir -p "$TMPDIR/str"
cat > "$TMPDIR/str/str.hpp" << 'HDR'
#pragma once
#include <string>

struct OnlyString {
    std::string name = "x";
    std::string twice() const { return name + name; }
};
HDR

out2=$("$TOOL" generate "$TMPDIR/str" --module onlystr --lang csharp -o "$TMPDIR/strout" 2>&1)
rc2=$?
check "a header whose only member is a std::string builds" test $rc2 -eq 0

echo ""
if [ "$fail" -eq 0 ]; then
    echo "All $pass minimal-header tests passed!"
else
    echo "$fail of $((pass + fail)) tests FAILED"
    exit 1
fi
