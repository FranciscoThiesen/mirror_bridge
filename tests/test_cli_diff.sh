#!/bin/bash
# Tests for 'mirror_bridge diff' command

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL="$SCRIPT_DIR/../tools/mirror_bridge"
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

echo "Testing 'mirror_bridge diff' command:"

# Setup: create test headers
mkdir -p "$TMPDIR/src"
cat > "$TMPDIR/src/point.hpp" << 'EOF'
struct Point {
    double x = 0.0;
    double y = 0.0;
    double length() const;
};
EOF

# ---- Test 1: First run creates snapshot ----
output=$("$TOOL" diff "$TMPDIR/src" --output "$TMPDIR/build" 2>&1)
check "first run creates snapshot" \
    assert_contains "$output" "No previous snapshot"
check "snapshot file exists" \
    test -f "$TMPDIR/build/.mirror_bridge/binding_snapshot.txt"

# ---- Test 2: No changes detected on second run ----
output=$("$TOOL" diff "$TMPDIR/src" --output "$TMPDIR/build" 2>&1)
check "no changes detected on re-run" \
    assert_contains "$output" "No binding surface changes"

# ---- Test 3: Adding a member is detected ----
cat > "$TMPDIR/src/point.hpp" << 'EOF'
struct Point {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double length() const;
};
EOF

output=$("$TOOL" diff "$TMPDIR/src" --output "$TMPDIR/build" 2>&1)
check "detects added member" \
    assert_contains "$output" "added"

# ---- Test 4: --update flag saves new snapshot without prompt ----
"$TOOL" diff "$TMPDIR/src" --output "$TMPDIR/build" --update > /dev/null 2>&1
output=$("$TOOL" diff "$TMPDIR/src" --output "$TMPDIR/build" 2>&1)
check "--update saves snapshot (no diff after)" \
    assert_contains "$output" "No binding surface changes"

# ---- Test 5: Removing a method is detected ----
cat > "$TMPDIR/src/point.hpp" << 'EOF'
struct Point {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};
EOF

output=$("$TOOL" diff "$TMPDIR/src" --output "$TMPDIR/build" 2>&1)
check "detects removed method" \
    assert_contains "$output" "removed"

# ---- Test 6: Adding a new header file is detected ----
"$TOOL" diff "$TMPDIR/src" --output "$TMPDIR/build" --update > /dev/null 2>&1
cat > "$TMPDIR/src/color.hpp" << 'EOF'
struct Color {
    int r = 0;
    int g = 0;
    int b = 0;
};
EOF

output=$("$TOOL" diff "$TMPDIR/src" --output "$TMPDIR/build" 2>&1)
check "detects new header file" \
    assert_contains "$output" "added"

# ---- Test 7: MIRROR_BRIDGE_SKIP is respected ----
"$TOOL" diff "$TMPDIR/src" --output "$TMPDIR/build" --update > /dev/null 2>&1
cat > "$TMPDIR/src/internal.hpp" << 'EOF'
// MIRROR_BRIDGE_SKIP
struct InternalOnly {
    int secret = 42;
};
EOF

output=$("$TOOL" diff "$TMPDIR/src" --output "$TMPDIR/build" 2>&1)
check "MIRROR_BRIDGE_SKIP file is ignored" \
    assert_contains "$output" "No binding surface changes"

# ---- Test 8: Missing source dir produces error ----
output=$("$TOOL" diff 2>&1 || true)
check "missing src_dir produces error" \
    assert_contains "$output" "Error"

# ---- Test 9: Deterministic output (run twice, same result) ----
out1=$("$TOOL" diff "$TMPDIR/src" --output "$TMPDIR/build" 2>&1)
out2=$("$TOOL" diff "$TMPDIR/src" --output "$TMPDIR/build" 2>&1)
check "output is deterministic across runs" \
    test "$out1" = "$out2"

echo ""
if [ "$fail" -eq 0 ]; then
    echo "All $pass diff tests passed!"
else
    echo "$fail of $((pass + fail)) tests FAILED"
    exit 1
fi
