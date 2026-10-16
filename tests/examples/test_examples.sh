#!/bin/bash
# Build and run every numbered example the way examples/README.md tells a
# reader to, because nothing did.
#
# This is how examples/open3d-comprehensive shipped with o3d.PointCloud()
# raising TypeError, and how six of the eleven numbered examples came to fail
# without anyone noticing: the harness globs tests/, and examples/ is not
# under it. The shop window is the first thing a new user runs.
#
# An example is expected to pass unless its own README says otherwise. The
# banner a reader sees is the expectation this checks, so an example that
# starts failing has to either be fixed or have its README admit it -- there
# is no third list to drift out of step.
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CLI="$ROOT/tools/mirror_bridge"
EX="$ROOT/examples"

[ -x "$CLI" ] || { echo "no CLI at $CLI"; exit 1; }

pass=0; fail=0; expected_fail=0
declare -a failures=()

for dir in "$EX"/[0-9][0-9]-*/; do
    name=$(basename "$dir")
    test_py=$(ls "$dir"/test_*.py 2>/dev/null | head -1)
    [ -n "$test_py" ] || continue

    # The module name is whatever the test imports that is not stdlib.
    module=$(python3 - "$test_py" <<'PY'
import ast, sys
tree = ast.parse(open(sys.argv[1]).read())
stdlib = set(sys.stdlib_module_names) if hasattr(sys, "stdlib_module_names") else set()
for node in ast.walk(tree):
    if isinstance(node, ast.Import):
        for alias in node.names:
            top = alias.name.split(".")[0]
            if top not in stdlib:
                print(top)
                raise SystemExit(0)
PY
)
    [ -n "$module" ] || { echo "  SKIP $name (no module to import)"; continue; }

    # "Not implemented yet" / "Partly implemented" in the README means the
    # test is a specification rather than a description.
    want_fail=0
    if [ -f "$dir/README.md" ] && \
       grep -qE '^> \*\*(Not implemented yet|Partly implemented)\.\*\*' "$dir/README.md"; then
        want_fail=1
    fi

    ( cd "$dir" && rm -rf build && "$CLI" generate src/ --module "$module" --lang python ) \
        > "/tmp/ex_${name}_gen.log" 2>&1
    gen_rc=$?
    if [ $gen_rc -ne 0 ]; then
        echo "  ✗ $name: generate failed (exit $gen_rc)"
        tail -5 "/tmp/ex_${name}_gen.log" | sed 's/^/      /'
        fail=$((fail + 1)); failures+=("$name (generate)")
        continue
    fi

    ( cd "$dir" && python3 "$(basename "$test_py")" ) > "/tmp/ex_${name}_run.log" 2>&1
    run_rc=$?

    if [ $run_rc -eq 0 ] && [ $want_fail -eq 0 ]; then
        echo "  ✓ $name"
        pass=$((pass + 1))
    elif [ $run_rc -ne 0 ] && [ $want_fail -eq 1 ]; then
        echo "  ✓ $name (fails, and its README says so)"
        expected_fail=$((expected_fail + 1))
    elif [ $run_rc -ne 0 ]; then
        echo "  ✗ $name: test failed (exit $run_rc) and its README promises it works"
        tail -6 "/tmp/ex_${name}_run.log" | sed 's/^/      /'
        fail=$((fail + 1)); failures+=("$name")
    else
        echo "  ✗ $name: passes, but its README still says it does not"
        echo "      Remove the banner from $name/README.md"
        fail=$((fail + 1)); failures+=("$name (stale banner)")
    fi
done

echo ""
echo "  passed: $pass   known-failing: $expected_fail   failed: $fail"
if [ $fail -gt 0 ]; then
    echo "  failing: ${failures[*]}"
    exit 1
fi
exit 0
