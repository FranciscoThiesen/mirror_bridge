#!/bin/bash
# `generate --exclude` must keep a header subtree out of the module.
#
# MIRROR_BRIDGE_SKIP_FILE is a marker inside the header, which is no help for a
# library you did not write -- and binding a library you did not write is what
# auto-discovery is for. pmp-library is the case that drove this: pointing at
# src/pmp pulled in its OpenGL viewer, whose symbols are not in the objects you
# would link and which nobody wants callable from Python.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
CLI="$ROOT/tools/mirror_bridge"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

fail() { echo "FAIL: $*"; exit 1; }

echo "1. without --exclude, the viewer is discovered"
plain=$("$CLI" generate "$HERE/src" --module xh_plain --lang python \
        --output "$OUT/plain" --force --json 2>/dev/null)
echo "$plain" | grep -q 'xh::Viewer' \
  || fail "expected Viewer in the plain run; got: $plain"

echo "2. --exclude keeps it out, and keeps everything else in"
excl=$("$CLI" generate "$HERE/src" --module xh_excl --lang python \
       --output "$OUT/excl" --force --json --exclude 'viz/*' 2>/dev/null)
if echo "$excl" | grep -q 'xh::Viewer'; then
    fail "Viewer survived --exclude 'viz/*'"
fi
echo "$excl" | grep -q 'xh::Point'  || fail "Point was excluded too"
echo "$excl" | grep -q 'xh::Circle' || fail "Circle was excluded too"

echo "3. the pattern matches at any depth, the way .gitignore does"
# Here the include spelling is 'viz/viewer.hpp'; pointed one level up it is
# 'src/viz/viewer.hpp'. A pattern that only matched the full spelling
# silently excluded nothing, which is the kind of rule that is correct and
# still wrong every time someone uses it.
deep=$("$CLI" generate "$HERE" --module xh_deep --lang python \
       --output "$OUT/deep" --force --json --exclude 'viz/*' 2>/dev/null)
if echo "$deep" | grep -q 'xh::Viewer'; then
    fail "pattern did not match at depth (src/viz/viewer.hpp)"
fi
echo "$deep" | grep -q 'xh::Point' || fail "Point lost in the deep run"

echo "4. the excluded module imports and has the right surface"
PYTHONPATH="$OUT/excl" python3 -c "
import xh_excl
assert xh_excl.Point().sum() == 3.0, 'Point.sum() wrong'
assert hasattr(xh_excl, 'Circle'), 'Circle missing'
assert not hasattr(xh_excl, 'Viewer'), 'Viewer present after exclusion'
print('   imported, Point works, Viewer absent')
" || fail "the excluded module did not import"

echo "PASS: exclude_headers"
