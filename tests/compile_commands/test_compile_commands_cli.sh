#!/usr/bin/env bash
# --compile-commands, end to end through the CLI.
#
# The fixture header refuses to compile unless WIDGET_ENABLED is defined and an
# extra include directory is on the path. Neither appears on the mirror_bridge
# command line, so a module that imports proves the compile database was read
# and that its flags reached the reflection pass -- which is the whole claim.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
PROJECT="$HERE/project"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

PASS=0; FAIL=0
ok()   { PASS=$((PASS+1)); echo "  PASS  $1"; }
bad()  { FAIL=$((FAIL+1)); echo "  FAIL  $1"; }

# Written here rather than committed: a compile database carries absolute
# paths, which differ between this checkout and anyone else's.
cat > "$WORK/compile_commands.json" <<JSON
[
  {
    "directory": "$PROJECT",
    "file": "$PROJECT/include/widget.hpp",
    "command": "c++ -std=c++17 -O2 -Wall -I extra -DWIDGET_ENABLED=1 -c include/widget.hpp"
  }
]
JSON

echo "=== without the compile database, the fixture must fail ==="
if "$ROOT/tools/mirror_bridge" generate "$PROJECT/include" --module widget_nodb \
       -o "$WORK/nodb" --lang python --no-stubs >"$WORK/nodb.log" 2>&1; then
    bad "a build with no compile database should not have succeeded"
else
    if grep -q "WIDGET_ENABLED is not defined" "$WORK/nodb.log"; then
        ok "fails for the right reason (the #error fires)"
    else
        ok "fails without the compile database"
    fi
fi

echo "=== with the compile database, it must build and import ==="
if "$ROOT/tools/mirror_bridge" generate "$PROJECT/include" --module widget_db \
       -o "$WORK/db" --lang python --no-stubs \
       --compile-commands "$WORK/compile_commands.json" >"$WORK/db.log" 2>&1; then
    ok "generate succeeded using only the compile database for flags"
else
    bad "generate failed with the compile database"
    tail -25 "$WORK/db.log"
fi

if grep -q "compile_commands.json.*flag" "$WORK/db.log"; then
    ok "the run reports what it harvested"
else
    bad "no report of harvested flags"
fi

if python3 -c "
import sys
sys.path.insert(0, '$WORK/db')
import widget_db
w = widget_db.Widget()
assert w.slots == 7, w.slots
assert w.capacity() == 14, w.capacity()
" 2>>"$WORK/import.log"; then
    ok "the module imports and kMaxSlots from the extra -I came through"
else
    bad "module did not import or behaved wrongly"
    cat "$WORK/import.log"
fi

echo "=== --compile-commands-for narrows by source path ==="
if "$ROOT/tools/mirror_bridge" generate "$PROJECT/include" --module widget_miss \
       -o "$WORK/miss" --lang python --no-stubs \
       --compile-commands "$WORK/compile_commands.json" \
       --compile-commands-for "$PROJECT/nothing_here" >"$WORK/miss.log" 2>&1; then
    bad "scoping to a path with no entries should harvest nothing and fail"
else
    ok "scoping to an unrelated path harvests nothing, so the build fails"
fi

echo "=== an MSVC database read on this machine, which is the real case ==="
# MSVC has no reflection, so a Windows project's build runs there and generation
# runs here. The database therefore arrives with /I and /D in it, and with paths
# naming a filesystem this machine does not have.
cat > "$WORK/msvc_compile_commands.json" <<JSON
[
  {
    "directory": "C:/src/widget/build",
    "file": "C:/src/widget/include/widget.hpp",
    "command": "cl.exe /nologo /EHsc /MD /std:c++17 /O2 /I C:/src/widget/extra /DWIDGET_ENABLED=1 /Fo widget.obj /c C:/src/widget/include/widget.hpp"
  }
]
JSON

if "$ROOT/tools/mirror_bridge" generate "$PROJECT/include" --module widget_msvc \
       -o "$WORK/msvc" --lang python --no-stubs \
       --compile-commands "$WORK/msvc_compile_commands.json" \
       --replace-prefix "C:/src/widget=$PROJECT" >"$WORK/msvc.log" 2>&1; then
    ok "MSVC spellings plus --replace-prefix build a working module"
else
    bad "the MSVC-flavoured database did not work"
    tail -20 "$WORK/msvc.log"
fi

if python3 -c "
import sys
sys.path.insert(0, '$WORK/msvc')
import widget_msvc
assert widget_msvc.Widget().slots == 7
" 2>/dev/null; then
    ok "and the module behaves"
else
    bad "the module from the MSVC database misbehaved"
fi

# Without the remap the Windows paths name nothing here, and the point is that
# this fails rather than quietly building against the wrong headers.
if "$ROOT/tools/mirror_bridge" generate "$PROJECT/include" --module widget_noremap \
       -o "$WORK/noremap" --lang python --no-stubs \
       --compile-commands "$WORK/msvc_compile_commands.json" >"$WORK/noremap.log" 2>&1; then
    bad "unmapped Windows paths should not have produced a module"
else
    ok "unmapped Windows paths fail instead of silently missing the config"
fi

echo "=== a conflicting define is reported, not resolved quietly ==="
cat > "$WORK/conflict.json" <<JSON
[
  {"directory": "$PROJECT", "file": "$PROJECT/include/widget.hpp",
   "command": "c++ -I extra -DWIDGET_ENABLED=1 -DMODE=fast -c include/widget.hpp"},
  {"directory": "$PROJECT", "file": "$PROJECT/include/other.cpp",
   "command": "c++ -I extra -DWIDGET_ENABLED=1 -DMODE=slow -c include/other.cpp"}
]
JSON
"$ROOT/tools/mirror_bridge" generate "$PROJECT/include" --module widget_conf \
    -o "$WORK/conf" --lang python --no-stubs \
    --compile-commands "$WORK/conflict.json" >"$WORK/conf.log" 2>&1 || true
if grep -q "MODE is defined as both" "$WORK/conf.log"; then
    ok "names the macro and both values"
else
    bad "a macro defined two ways was not reported"
    grep -i "warning" "$WORK/conf.log" | head -3
fi

echo "=== a missing compile database is an argument error, not a crash ==="
"$ROOT/tools/mirror_bridge" generate "$PROJECT/include" --module widget_none \
    -o "$WORK/none" --lang python --no-stubs \
    --compile-commands "$WORK/does_not_exist.json" >"$WORK/none.log" 2>&1
if grep -q "no compile_commands.json at" "$WORK/none.log"; then
    ok "names the missing file"
else
    bad "did not report the missing compile database clearly"
    tail -5 "$WORK/none.log"
fi

echo "=== --discover-only stops before codegen ==="
if "$ROOT/tools/mirror_bridge" generate "$PROJECT/include" --module widget_disc \
       -o "$WORK/disc" --lang python --discover-only \
       --compile-commands "$WORK/compile_commands.json" >"$WORK/disc.log" 2>&1; then
    ok "discover-only exits successfully"
else
    bad "discover-only failed"
    tail -20 "$WORK/disc.log"
fi
if grep -q "Discovery report" "$WORK/disc.log" && grep -q "source:    reflection" "$WORK/disc.log"; then
    ok "reports reflection as the discovery source"
else
    bad "discovery report missing or did not use reflection"
    grep -A8 "Discovery report" "$WORK/disc.log" || true
fi
if [ -f "$WORK/disc/widget_disc.so" ]; then
    bad "discover-only built a module"
else
    ok "no module was built"
fi

echo "=== --discover-only --json emits one object ==="
"$ROOT/tools/mirror_bridge" generate "$PROJECT/include" --module widget_js \
    -o "$WORK/js" --lang python --discover-only --json \
    --compile-commands "$WORK/compile_commands.json" >"$WORK/disc.json" 2>"$WORK/disc.json.err"
if python3 -c "
import json, sys
d = json.load(open('$WORK/disc.json'))
assert d['status'] == 'ok', d
assert d['action'] == 'discover', d
assert d['source'] == 'reflection', d
assert d['counts']['classes'] >= 1, d
assert 'Widget' in ' '.join(d['classes']), d['classes']
assert d['compile_commands']['flags'] >= 2, d['compile_commands']
"; then
    ok "the JSON object carries source, counts, classes and the compile-db report"
else
    bad "discover-only --json was not the expected object"
    head -c 600 "$WORK/disc.json"; echo
fi

echo ""
echo "$PASS passed, $FAIL failed"
if [ "$FAIL" -ne 0 ]; then
    echo "FAIL: --compile-commands and --discover-only"
    exit 1
fi
echo "PASS: --compile-commands and --discover-only"
