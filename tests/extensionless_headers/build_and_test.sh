#!/bin/bash
# A library's public headers are not always *.h or *.hpp.
#
# Eigen's entire public interface is 30 extensionless files (Eigen/Core,
# Eigen/Dense, Eigen/LU) and Qt's is the same shape (<QString>, <QWidget>).
# Matching only the two extensions made those libraries invisible, and the
# walk reached their INTERNAL headers instead -- which are not self-contained,
# so the module failed on errors from inside the library. That reads as the
# library's fault and was ours.
#
# The other half of the contract: an extensionless file is only a header if it
# looks like one. COPYING and Makefile must stay out.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
CLI="$ROOT/tools/mirror_bridge"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

fail() { echo "FAIL: $*"; exit 1; }

echo "1. the extensionless public header is seen, and its class is bound"
# Nothing in src/ is named *.h or *.hpp, so a walk matching only those two
# extensions reports "No header files found" here.
out=$("$CLI" generate "$HERE/src" --module ext_shapes --lang python \
      --output "$OUT/a" --force --json 2>/dev/null)
echo "$out" | grep -q 'ext::Square' \
  || fail "Square was not discovered through the extensionless header; got: $out"
[ -f "$OUT/a/ext_shapes.so" ] || fail "no module was produced"

echo "2. the module imports and the class works"
PYTHONPATH="$OUT/a" python3 -c "
import ext_shapes
s = ext_shapes.Square()
s.side = 3.0
assert s.area() == 9.0, s.area()
" 2>/dev/null || fail "the bound class does not work"

echo "3. COPYING and Makefile are not treated as headers"
# They are extensionless too. A walk that took every extensionless file would
# hand the compiler a licence notice. The headers list the planner was given is
# the authoritative record of what the walk picked up.
hdrs="$OUT/a/.mirror_bridge/plan/ext_shapes/headers.txt"
[ -f "$hdrs" ] || fail "no header list was written; cannot check what was swept in"
grep -q 'Shapes' "$hdrs" || fail "the extensionless header is not in the header list"
for junk in COPYING Makefile; do
    if grep -q "$junk" "$hdrs"; then
        fail "$junk was listed as a header"
    fi
done

echo "4. the sniff is on content, not on the name"
# A file called LICENSE that really is a header would be included; the point
# is that the decision is made by looking inside.
printf '#pragma once\nnamespace ext { struct FromOddName { int v = 7; }; }\n' > "$HERE/src/Oddly_Named"
out2=$("$CLI" generate "$HERE/src" --module ext_odd --lang python \
       --output "$OUT/b" --force --json 2>/dev/null)
rm -f "$HERE/src/Oddly_Named"
echo "$out2" | grep -q 'ext::FromOddName' \
  || fail "an extensionless file whose contents are a header was not picked up"

echo "PASS: extensionless public headers are discovered, junk is not"
