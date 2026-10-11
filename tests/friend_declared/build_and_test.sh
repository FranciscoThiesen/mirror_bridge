#!/bin/bash
# A class whose name is first introduced by a `friend` declaration is missing
# from members_of() of its namespace, so the namespace walk cannot find it.
# pmp-library is the case: pmp::SurfaceMesh, the class the library exists for,
# was absent from the module while all 16 of its satellite property and handle
# types bound. Recovering it from the signatures of the library's own functions
# took pmp from 16 classes to 32.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
CLI="$ROOT/tools/mirror_bridge"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

fail() { echo "FAIL: $*"; exit 1; }

echo "1. the friend-declared class is discovered and bound"
json=$("$CLI" generate "$HERE/src" --module fd_mod --lang python \
       --output "$OUT/m" --force --json 2>/dev/null)
echo "$json" | grep -q '"status":"ok"' || fail "generate failed: $json"
echo "$json" | grep -q 'fd::Mesh'   || fail "Mesh was not discovered; got: $json"
echo "$json" | grep -q 'fd::Report' || fail "Report (return-type only) was not discovered"

echo "2. it works at runtime, and so does the return-type-only class"
PYTHONPATH="$OUT/m" python3 -c "
import fd_mod as m
mesh = m.Mesh()
assert mesh.count() == 3, mesh.count()
mesh.grow()
assert mesh.count() == 4
r = m.Report()
assert r.value() == 7
" 2>/dev/null || fail "the recovered classes do not behave"

echo "3. discovery went through reflection, not the text scan"
# A text-scan fallback would also find Mesh, by regex, and prove nothing about
# the namespace walk. The CLI says which path it took.
log=$("$CLI" generate "$HERE/src" --module fd_mod2 --lang python \
      --output "$OUT/n" --force 2>&1)
case "$log" in
    *"(reflection)"*) : ;;
    *) fail "discovery fell back to the text scan; this test would prove nothing. Log: $log" ;;
esac

echo "PASS: a friend-declared class is recovered from the library's own signatures"
