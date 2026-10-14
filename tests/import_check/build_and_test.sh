#!/bin/bash
# "Built" and "works" are different claims, and generate used to make the
# first while printing the second.
#
# Open3D is the case this comes from: 39 classes discovered, a .so produced, a
# green banner, and then ImportError on an undefined symbol the first time
# anyone imported it. The definitions were in the library's compiled sources,
# which nothing had linked. Nothing in the build said so.
#
# The same import is also the only place generate can hear which names the
# module declined to bind: that is decided at compile time and announced at
# module init, which is to say in the caller's first import, on stderr, after
# the build had already claimed success.
set -e
cd "$(dirname "$0")"

MB_TOOL="$(cd ../.. && pwd)/tools/mirror_bridge"
CXX="${MB_CXX:-clang++}"

rm -rf build
mkdir -p build

fail=0
check() {   # name, haystack, needle
    if [[ "$2" == *"$3"* ]]; then echo "  ✓ $1"; else echo "  ✗ $1 (expected: $3)"; fail=1; fi
}
reject() {
    if [[ "$2" != *"$3"* ]]; then echo "  ✓ $1"; else echo "  ✗ $1 (unexpected: $3)"; fail=1; fi
}

echo "Test 1: a module that cannot be imported is not reported as success..."
set +e
out=$("$MB_TOOL" generate src --module engine_py --lang python --output build --force 2>&1)
rc=$?
set -e
out=$(echo "$out" | sed -e 's/\x1b\[[0-9;]*m//g')
[ "$rc" -ne 0 ] && echo "  ✓ exit status is non-zero" || { echo "  ✗ exit status was 0"; fail=1; }
check "the .so was still built"            "$out" "✓ Built:"
check "and the import failure is reported" "$out" "does not import"
check "naming the undefined symbol"        "$out" "undefined symbol"
check "with the fix that applies"          "$out" "--link-args"
reject "no success banner"                 "$out" "✓ Successfully built"

echo ""
echo "Test 2: --no-verify leaves the old behaviour reachable..."
out=$("$MB_TOOL" generate src --module engine_py --lang python --output build --force --no-verify 2>&1 | sed -e 's/\x1b\[[0-9;]*m//g')
check "builds and claims success" "$out" "✓ Successfully built"
reject "and says nothing about importing" "$out" "does not import"

echo ""
echo "Test 3: with the library linked in, the module builds and works..."
# engine.cpp holds no reflection, but it has to agree with the module about
# the standard library, and the CLI builds Python modules with libc++ under
# clang.
STDLIB=""
case "$(basename "$CXX")" in *clang*) STDLIB="-stdlib=libc++" ;; esac
"$CXX" -std=c++17 $STDLIB -fPIC -O1 -w -c engine.cpp -o build/engine.o
out=$("$MB_TOOL" generate src --module engine_py --lang python --output build --force \
        --link-args "$(pwd)/build/engine.o" 2>&1 | sed -e 's/\x1b\[[0-9;]*m//g')
check "success banner returns" "$out" "✓ Successfully built"

echo ""
echo "Test 4: the names the module declined are reported by the build..."
# accumulate() takes a non-const double&, whose writes cannot reach a Python
# caller. The module decides that at compile time and says so at init; the
# build is where the person who can act on it is looking.
check "the declined function is named"  "$out" "accumulate"
check "with the reason, not a guess"    "$out" "non-const reference parameter"
check "under a heading that counts it"  "$out" "free function(s) the module declined"

echo ""
echo "Test 5: the JSON carries the same answer..."
"$MB_TOOL" generate src --module engine_py --lang python --output build --force \
    --link-args "$(pwd)/build/engine.o" --json 2>/dev/null > build/result.json
python3 - <<'PY'
import json, sys
d = json.load(open("build/result.json"))
assert d["status"] == "ok", d["status"]
assert d["imported"]["ok"] is True, d["imported"]
names = [x["name"] for x in d["declined"]]
assert "accumulate" in names, names
reason = next(x["reason"] for x in d["declined"] if x["name"] == "accumulate")
assert "non-const reference" in reason, reason
print("  ✓ status ok, imported.ok true, declined[] names accumulate with its reason")
PY

echo ""
echo "Test 6: and the failing build says so in JSON too..."
rm -f build/engine.o
set +e
"$MB_TOOL" generate src --module engine_py --lang python --output build --force \
    --json 2>/dev/null > build/bad.json
set -e
python3 - <<'PY'
import json
d = json.load(open("build/bad.json"))
assert d["status"] == "error", d["status"]
assert d["imported"]["ok"] is False, d["imported"]
assert "undefined symbol" in d["imported"]["reason"], d["imported"]["reason"]
assert "--link-args" in d["imported"]["suggestion"], d["imported"]["suggestion"]
print("  ✓ status error, imported.ok false, reason and suggestion both set")
PY

echo ""
[ "$fail" -eq 0 ] && echo "✓ import-check test passed"
[ "$fail" -eq 0 ]
