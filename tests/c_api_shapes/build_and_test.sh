#!/bin/bash
# The whole pipeline over a header full of C-style shapes, because two of the
# three only bite through the CLI: the member-template planner instantiates
# over every type the headers name, which is how a forward-declared type
# reaches lookup_type_in_python<T> and `typeid` on an incomplete type.
#
# Each shape here failed the ENTIRE module before, on a real library:
#   bit-field          ImGui's ImFontGlyph    "non-const reference cannot bind"
#   anonymous union    ImGuiStoragePair       member cache stopped being constant
#   forward decl       Open3D's camera types  "'typeid' of incomplete type"
#
# And what a C API hands back: a `const char*` return, which 1,282 functions
# in the compatibility corpus have and none of which could be bound.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
CLI="$ROOT/tools/mirror_bridge"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

fail() { echo "FAIL: $*"; exit 1; }

echo "1. the module builds through the CLI, planner included"
json=$("$CLI" generate "$HERE" --module capi_shapes --lang python \
       --output "$OUT/m" --force --json 2>/dev/null)
echo "$json" | grep -q '"status":"ok"' \
  || fail "generate did not succeed: $json"
[ -f "$OUT/m/capi_shapes.so" ] || fail "no module was produced"

echo "2. the bit-field and the union's named neighbours work at runtime"
PYTHONPATH="$OUT/m" python3 -c "
import capi_shapes as cs
g = cs.Glyph(); g.codepoint = 0x1F600; g.visible = 1
assert g.codepoint == 0x1F600, g.codepoint
assert g.visible == 1
p = cs.StoragePair(); p.key = 11; p.tag = 12
assert (p.key, p.tag) == (11, 12)
for aliased in ('val_i', 'val_f', 'val_p'):
    assert not hasattr(p, aliased), aliased
assert not hasattr(cs, 'HoldsOpaque')
lab = cs.Label()
assert lab.text is None, repr(lab.text)
lab.text = 'hello'
assert lab.text == 'hello'
at = cs.Attributes()
assert at.c_str() == 'text', repr(at.c_str())
assert at.attribute('tag') == 'node', repr(at.attribute('tag'))
assert at.attribute('missing') is None, repr(at.attribute('missing'))
assert not hasattr(at, 'owned'), 'a char* return belongs to the caller'
" 2>/dev/null || fail "the bound shapes do not behave"

echo "PASS: bit-fields, anonymous unions and forward declarations each cost only themselves"
