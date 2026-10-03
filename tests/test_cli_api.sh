#!/bin/bash
# `mirror_bridge api` — the Python surface gated on a committed stub.
#
# The contract this asserts is the one docs/guides/independent-teams.md
# tells two teams to depend on: a removed or re-signed name fails, an added
# one does not, and the stub is reproducible enough to be committed.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL="$SCRIPT_DIR/../tools/mirror_bridge"
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

pass=0
fail=0

check() {
    local name="$1" haystack="$2" needle="$3"
    if [[ "$haystack" == *"$needle"* ]]; then
        echo "  ✓ $name"; pass=$((pass + 1))
    else
        echo "  ✗ $name"
        echo "      expected: $needle"
        printf '%s\n' "$haystack" | sed 's/^/        /' | head -20
        fail=$((fail + 1))
    fi
}

check_rc() {
    local name="$1" got="$2" want="$3"
    if [ "$got" -eq "$want" ]; then
        echo "  ✓ $name (exit $got)"; pass=$((pass + 1))
    else
        echo "  ✗ $name: expected exit $want, got $got"; fail=$((fail + 1))
    fi
}

SRC="$TMPDIR/src"
mkdir -p "$SRC"
write_header() {
    cat > "$SRC/api.hpp" <<EOF
#pragma once
#include <vector>
namespace svc {
struct Quote { double bid = 0.0; double ask = 0.0; double mid() const { return (bid + ask) / 2; } };
struct Book {
    std::vector<Quote> levels;
    $1
    double best() const { return levels.empty() ? 0.0 : levels.front().mid(); }
};
$2
}
EOF
}

cd "$TMPDIR" || exit 1

# --------------------------------------------------------------------------
echo "A baseline has to exist before anything can be gated"
# --------------------------------------------------------------------------
write_header "void add(const Quote& q) { levels.push_back(q); }" ""
out=$("$TOOL" api "$SRC" --module quotes --check 2>&1); rc=$?
check "missing baseline is reported" "$out" "no baseline at"
check "it says how to create one" "$out" "--update"
check_rc "missing baseline" "$rc" 2

out=$("$TOOL" api "$SRC" --module quotes --update 2>&1); rc=$?
check "the baseline is written" "$out" "Baseline written to"
check "and the user is told to commit it" "$out" "Commit it"
check_rc "update" "$rc" 0
if [ -f api/quotes.pyi ]; then
    echo "  ✓ baseline exists at the default path"; pass=$((pass + 1))
else
    echo "  ✗ no baseline at api/quotes.pyi"; fail=$((fail + 1))
fi

# --------------------------------------------------------------------------
echo "An unchanged surface passes, and is reproducible"
# --------------------------------------------------------------------------
out=$("$TOOL" api "$SRC" --module quotes --check 2>&1); rc=$?
check "unchanged is reported as such" "$out" "is unchanged"
check_rc "unchanged" "$rc" 0

# Byte-reproducibility is what makes the gate usable at all: regenerating
# must not produce a spurious diff.
cp api/quotes.pyi "$TMPDIR/first.pyi"
"$TOOL" api "$SRC" --module quotes --update > /dev/null 2>&1
if cmp -s "$TMPDIR/first.pyi" api/quotes.pyi; then
    echo "  ✓ regenerating produces byte-identical output"; pass=$((pass + 1))
else
    echo "  ✗ the stub is not reproducible"; fail=$((fail + 1))
fi

# --------------------------------------------------------------------------
echo "Adding a name is additive, and does not fail the gate"
# --------------------------------------------------------------------------
write_header "void add(const Quote& q) { levels.push_back(q); }" \
             "inline double spread(const Quote& q) { return q.ask - q.bid; }"
out=$("$TOOL" api "$SRC" --module quotes --check 2>&1); rc=$?
check "the addition is listed" "$out" "added"
check "the new name is given" "$out" "spread"
check "it is called safe" "$out" "safe for existing importers"
check_rc "additive change" "$rc" 0

"$TOOL" api "$SRC" --module quotes --update > /dev/null 2>&1

# --------------------------------------------------------------------------
echo "Removing a name is breaking"
# --------------------------------------------------------------------------
write_header "void add(const Quote& q) { levels.push_back(q); }" ""
out=$("$TOOL" api "$SRC" --module quotes --check 2>&1); rc=$?
check "the removal is listed" "$out" "removed"
check "the lost name is given" "$out" "spread"
check "the consequence is stated" "$out" "AttributeError"
check "it counts the breakage" "$out" "breaking change"
check "it says how to accept" "$out" "Accept with:"
check_rc "removal" "$rc" 1

"$TOOL" api "$SRC" --module quotes --update > /dev/null 2>&1

# --------------------------------------------------------------------------
echo "Changing a signature is breaking, and both forms are shown"
# --------------------------------------------------------------------------
write_header "void add(const Quote& q, int qty) { (void)qty; levels.push_back(q); }" ""
out=$("$TOOL" api "$SRC" --module quotes --check 2>&1); rc=$?
check "the change is listed" "$out" "changed"
check "the method is named" "$out" "Book.add"
check "the old signature is shown" "$out" "was:"
check "the new signature is shown" "$out" "now:"
check "the new parameter appears" "$out" "qty: int"
check_rc "signature change" "$rc" 1

# --------------------------------------------------------------------------
echo "Without --check it reports but does not fail"
# --------------------------------------------------------------------------
out=$("$TOOL" api "$SRC" --module quotes 2>&1); rc=$?
check "it still reports the change" "$out" "changed"
check_rc "no --check" "$rc" 0

# --------------------------------------------------------------------------
echo "JSON is machine-readable"
# --------------------------------------------------------------------------
out=$("$TOOL" api "$SRC" --module quotes --check --json 2>/dev/null)
if printf '%s' "$out" | python3 -c "
import json, sys
d = json.load(sys.stdin)
assert d['breaking'] == 1, d
assert d['changed'][0]['name'] == 'Book.add', d
assert 'was' in d['changed'][0] and 'now' in d['changed'][0], d
" 2>/dev/null; then
    echo "  ✓ one JSON object with the classified change"; pass=$((pass + 1))
else
    echo "  ✗ JSON is not as documented: $out"; fail=$((fail + 1))
fi

# --------------------------------------------------------------------------
echo "Bad invocations say so"
# --------------------------------------------------------------------------
out=$("$TOOL" api "$SRC" 2>&1); rc=$?
check "a missing --module is an error" "$out" "--module is required"
check_rc "missing module" "$rc" 1

out=$("$TOOL" api "$SRC" --module quotes --nonsense 2>&1); rc=$?
check "an unknown flag is rejected" "$out" "unknown option '--nonsense'"
check_rc "unknown flag" "$rc" 1

echo ""
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
