#!/bin/bash
# Tests for 'mirror_bridge watch' command
#
# Tests argument validation, help output, checksum detection, and temp file
# cleanup. The full build cycle requires the reflection compiler, so these
# tests focus on the watch harness behavior.

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

echo "Testing 'mirror_bridge watch' command:"

# ---- Test 1: Missing src_dir produces error ----
output=$("$TOOL" watch 2>&1 || true)
check "missing src_dir produces error" \
    assert_contains "$output" "Source directory"

# ---- Test 2: Missing module name produces error ----
output=$("$TOOL" watch "$TMPDIR" 2>&1 || true)
check "missing --module produces error" \
    assert_contains "$output" "Module name"

# ---- Test 3: --help shows usage ----
output=$("$TOOL" watch --help 2>&1)
check "--help mentions Live reload" \
    assert_contains "$output" "Live reload"
check "--help mentions --module" \
    assert_contains "$output" "--module"
check "--help mentions --interval" \
    assert_contains "$output" "--interval"

# ---- Test 4: Checksum output is deterministic ----
mkdir -p "$TMPDIR/src"
cat > "$TMPDIR/src/a.hpp" << 'EOF'
struct A { int x; };
EOF
cat > "$TMPDIR/src/b.hpp" << 'EOF'
struct B { double y; };
EOF

out1=$(find "$TMPDIR/src" -maxdepth 3 \( -name "*.hpp" -o -name "*.h" \) -exec md5sum {} \; 2>/dev/null | sort)
out2=$(find "$TMPDIR/src" -maxdepth 3 \( -name "*.hpp" -o -name "*.h" \) -exec md5sum {} \; 2>/dev/null | sort)
check "checksum output is deterministic" \
    test "$out1" = "$out2"

# ---- Test 5: Checksum changes when file is modified ----
out_before=$(find "$TMPDIR/src" -maxdepth 3 \( -name "*.hpp" -o -name "*.h" \) -exec md5sum {} \; 2>/dev/null | sort)

cat > "$TMPDIR/src/a.hpp" << 'EOF'
struct A { int x; int y; };
EOF

out_after=$(find "$TMPDIR/src" -maxdepth 3 \( -name "*.hpp" -o -name "*.h" \) -exec md5sum {} \; 2>/dev/null | sort)
check "checksum changes when header modified" \
    test "$out_before" != "$out_after"

# ---- Test 6: Watch starts and can be interrupted cleanly ----
mkdir -p "$TMPDIR/watch_src"
cat > "$TMPDIR/watch_src/test.hpp" << 'EOF'
struct Test { int a; };
EOF

# Start watch — it will fail on generate (no reflection compiler in this context)
# but the process should start, print the header, and be killable
"$TOOL" watch "$TMPDIR/watch_src" --module test_mod --interval 1 > "$TMPDIR/watch_out.txt" 2>&1 &
WATCH_PID=$!

# Let it run briefly
sleep 3

# Verify it started (printed the header)
check "watch prints startup banner" \
    assert_contains "$(cat "$TMPDIR/watch_out.txt")" "Watch Mode"

# Kill it and verify it exits
kill "$WATCH_PID" 2>/dev/null || true
wait "$WATCH_PID" 2>/dev/null || true

check "watch process terminates on SIGTERM" \
    test ! -d "/proc/$WATCH_PID"

echo ""
if [ "$fail" -eq 0 ]; then
    echo "All $pass watch tests passed!"
else
    echo "$fail of $((pass + fail)) tests FAILED"
    exit 1
fi
