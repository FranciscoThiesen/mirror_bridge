#!/bin/bash
# A header whose namespaces come from a macro. Reflection reads it after the
# preprocessor and reports fully qualified names; the text scan cannot, and
# the `using namespace` directives derived from the text are then nonsense.
#
# On nlohmann-json that produced
#
#     using namespace detail;
#     using namespace detail::detail;
#     using namespace detail::detail::detail;
#
# and seventeen errors before the first line of binding code. The directives
# exist only so an unqualified name from the text scan resolves, so they are
# emitted only when the text scan is what produced the class list.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
CLI="$ROOT/tools/mirror_bridge"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

fail() { echo "FAIL: $*"; exit 1; }

echo "1. a module builds from a header whose namespace comes from a macro"
json=$("$CLI" generate "$HERE" --module macrons_mod --lang python \
       --output "$OUT/m" --force --json 2>/dev/null)
echo "$json" | grep -q '"status":"ok"' \
  || fail "generate did not succeed: $json"
[ -f "$OUT/m/macrons_mod.so" ] || fail "no module was produced"

echo "2. reflection reported the names in full, which is why no directive is needed"
echo "$json" | grep -q '"name":"macrons::v1::detail::Helper"' \
  || fail "expected a fully qualified name from reflection: $json"
echo "$json" | grep -q '"name":"macrons::v1::Widget"' \
  || fail "expected a fully qualified name from reflection: $json"

echo "3. the classes work at runtime under their real names"
PYTHONPATH="$OUT/m" python3 -c "
import macrons_mod as m
w = m.Widget()
assert w.area() == 1.5 * 2.5, w.area()
w.width = 4.0
assert w.area() == 10.0, w.area()
h = m.Helper()
assert h.doubled() == 6, h.doubled()
" 2>/dev/null || fail "the bound classes do not behave"

echo "PASS: a macro-opened namespace does not poison the generated binding"
