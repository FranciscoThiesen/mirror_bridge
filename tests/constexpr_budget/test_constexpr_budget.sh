#!/usr/bin/env bash
# The reflection pass escalates its constexpr budget instead of giving up.
#
# Discovery walks every class in the codebase at compile time, so it is the
# pass most likely to run out of constexpr budget -- and the cost of running
# out is not a slow build, it is a wrong answer: the CLI falls back to scanning
# header text, which cannot see a class declared through an export macro
# (`class CV_EXPORTS Mat`) and reports the macro's name instead. OpenCV's core
# module, 86k lines of headers across 139 files, exceeded the first budget and
# was being discovered that way for no other reason.
#
# A codebase large enough to exhaust a real budget is too large to put in a
# test, so MB_CONSTEXPR_BUDGET forces the first tier low and the escalation
# path runs on an ordinary fixture.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
FIXTURE="$ROOT/tests/csharp_bindings"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# How small a budget has to be to exhaust this fixture differs by compiler,
# because clang counts steps and GCC counts operations. The values are measured
# rather than guessed, and they have to exhaust *both* passes -- discovery and
# the module compile -- or half the escalation path goes untested:
#
#   GCC   1000   internal compiler error in coerce_template_parms, which is a
#                GCC 16.2 bug this test should not be exercising
#         10000  both passes run out and report cleanly        <- used
#         100000 discovery runs out, the module compile does not
#         1e6+   neither runs out
#   clang 1000   both passes run out and report cleanly        <- used
if [ -n "${MB_CXX:-}" ]; then
    CXX_NAME="$MB_CXX"
elif command -v clang++ >/dev/null 2>&1 && clang++ --version 2>&1 | grep -qi "reflection\|p2996\|bloomberg"; then
    CXX_NAME="clang++"
else
    CXX_NAME="g++"
fi
case "$(basename "$CXX_NAME")" in
    *clang*) TIGHT=1000 ;;
    *)       TIGHT=10000 ;;
esac
echo "compiler: $(basename "$CXX_NAME"), forcing the first budget tier to $TIGHT"

PASS=0; FAIL=0
ok()  { PASS=$((PASS+1)); echo "  PASS  $1"; }
bad() { FAIL=$((FAIL+1)); echo "  FAIL  $1"; }

run_discover() {
    local out="$1"; shift
    "$ROOT/tools/mirror_bridge" generate "$FIXTURE" --module "$(basename "$out")" \
        -o "$out" --lang python --no-stubs --discover-only --json \
        >"$out.json" 2>"$out.err"
}

source_of() {
    python3 -c "
import json, sys
try:
    print(json.load(open(sys.argv[1]))['source'])
except Exception:
    print('<no json>')
" "$1"
}

classes_of() {
    python3 -c "
import json, sys
try:
    d = json.load(open(sys.argv[1]))
    print(','.join(sorted(d['classes'])))
except Exception:
    print('<no json>')
" "$1"
}

echo "=== an ordinary budget needs no escalation ==="
mkdir -p "$WORK/plain"
run_discover "$WORK/plain"
check_src="$(source_of "$WORK/plain.json")"
if [ "$check_src" = "reflection" ]; then
    ok "discovery uses reflection"
else
    bad "expected reflection, got '$check_src'"
    tail -5 "$WORK/plain.err"
fi
if grep -q "ran out of constexpr budget" "$WORK/plain.err"; then
    bad "escalated when it did not need to"
else
    ok "says nothing about the budget, because nothing happened"
fi
BASELINE="$(classes_of "$WORK/plain.json")"

echo "=== a budget too small to finish escalates rather than falling back ==="
mkdir -p "$WORK/tight"
MB_CONSTEXPR_BUDGET=$TIGHT run_discover "$WORK/tight"
tight_src="$(source_of "$WORK/tight.json")"
if [ "$tight_src" = "reflection" ]; then
    ok "still reflection, not a text-scan fallback"
else
    bad "a tight budget degraded discovery to '$tight_src'"
    tail -8 "$WORK/tight.err"
fi
if [ "$(classes_of "$WORK/tight.json")" = "$BASELINE" ]; then
    ok "and the class list is identical to the unconstrained run"
else
    bad "the class list changed after escalating"
    echo "    baseline:  $BASELINE"
    echo "    escalated: $(classes_of "$WORK/tight.json")"
fi

echo "=== and it says so, because the retry can take minutes ==="
# Only in the log, and an unexplained multi-minute wait is indistinguishable
# from a hang, so this has to reach the terminal.
if grep -q "ran out of constexpr budget" "$WORK/tight.err"; then
    ok "the escalation is reported on stderr"
else
    bad "the escalation was silent"
fi
if grep -q "retrying at -fconstexpr" "$WORK/tight.err"; then
    ok "and names the budget it retried at"
else
    bad "did not name the new budget"
fi
# "export macro" alone is the wrong needle: the CLI's own text-scan fallback
# notice says that too, so this assertion passed on a GCC run where nothing had
# escalated at all. Match a phrase only the escalation message has.
if grep -q "This is slower" "$WORK/tight.err"; then
    ok "and explains why the wait is worth it"
else
    bad "does not explain why it is worth the wait"
fi

echo "=== the module compile escalates too, or the fix is half-applied ==="
# Discovery escalating on its own would be worse than not escalating: it would
# report a clean class list and the module build would then die on the identical
# cause, which reads as the two steps disagreeing about the same headers.
MB_CONSTEXPR_BUDGET=$TIGHT "$ROOT/tools/mirror_bridge" generate "$FIXTURE" \
    --module budget_mod -o "$WORK/mod" --lang python --no-stubs \
    >"$WORK/mod.out" 2>&1
mod_rc=$?
if [ "$mod_rc" -eq 0 ] && [ -f "$WORK/mod/budget_mod.so" ]; then
    ok "a module still builds with the budget forced low"
else
    bad "the module did not build (rc=$mod_rc)"
    tail -12 "$WORK/mod.out"
fi
if grep -q "the module compile ran out of constexpr budget" "$WORK/mod.out"; then
    ok "and the module compile reports its own escalation"
else
    bad "the module compile did not report an escalation"
fi
if python3 -c "
import sys
sys.path.insert(0, '$WORK/mod')
import budget_mod
d = budget_mod.Device()
d.id = 9
assert d.doubled() == 18, d.doubled()
assert d.describe() == 'probe/9', d.describe()
" 2>"$WORK/mod.imp"; then
    ok "and the module it built behaves correctly"
else
    bad "the escalated module misbehaved"
    cat "$WORK/mod.imp"
fi

# The first attempt's diagnostics are not the reason anything failed, and
# printing them before a successful retry reads as the tool contradicting
# itself.
if grep -qE "^/.*error: constexpr" "$WORK/mod.out"; then
    bad "printed the pre-retry error wall"
    grep -m2 -E "error: constexpr" "$WORK/mod.out"
else
    ok "does not print the errors it is about to retry past"
fi

echo ""
echo "$PASS passed, $FAIL failed"
if [ "$FAIL" -ne 0 ]; then
    echo "FAIL: constexpr budget escalation"
    exit 1
fi
echo "PASS: constexpr budget escalation"
