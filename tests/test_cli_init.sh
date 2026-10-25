#!/bin/bash
# Tests for 'mirror_bridge init' — the scaffolding a new project starts from.
#
# The C# path gets the most attention because it is the one that has to agree
# with the generator about a name: Program.cs says `using <ns>;` before the
# generator has emitted anything, so if init and build_csharp ever computed the
# namespace differently the scaffold would compile for nobody. The check below
# compares the two directly rather than trusting either.

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

file_contains() {
    grep -q -F -e "$2" "$1" 2>/dev/null
}

echo "Testing 'mirror_bridge init' command:"

# ---- every language scaffolds its own smoke test ----
for lang in python lua js csharp; do
    if (cd "$TMPDIR" && "$TOOL" init "proj_$lang" --lang "$lang" --no-ci >/dev/null 2>&1)
    then rc=0; else rc=1; fi
    check "init --lang $lang succeeds" test "$rc" -eq 0
    check "init --lang $lang writes the example header" \
        test -f "$TMPDIR/proj_$lang/src/greeter.hpp"
    check "init --lang $lang writes a README" \
        test -f "$TMPDIR/proj_$lang/README.md"
    check "init --lang $lang honours --no-ci" \
        test ! -d "$TMPDIR/proj_$lang/.github"
done

check "python scaffolds a .py smoke test" \
    test -f "$TMPDIR/proj_python/test_proj_python.py"
check "lua scaffolds a .lua smoke test" \
    test -f "$TMPDIR/proj_lua/test_proj_lua.lua"
check "js scaffolds a .js smoke test" \
    test -f "$TMPDIR/proj_js/test_proj_js.js"
check "csharp scaffolds a Program.cs" \
    test -f "$TMPDIR/proj_csharp/Program.cs"
check "csharp scaffolds a .csproj named after the project" \
    test -f "$TMPDIR/proj_csharp/proj_csharp.csproj"

# ---- the C# project is a project, not just text ----
check "the .csproj is well-formed XML" \
    python3 -c "import sys,xml.etree.ElementTree as E; E.parse(sys.argv[1])" \
        "$TMPDIR/proj_csharp/proj_csharp.csproj"
check "the .csproj copies the native library beside the assembly" \
    file_contains "$TMPDIR/proj_csharp/proj_csharp.csproj" "libproj_csharp.so"
check "the .csproj builds an executable" \
    file_contains "$TMPDIR/proj_csharp/proj_csharp.csproj" "<OutputType>Exe</OutputType>"
check "the C# README gives the .NET SDK route" \
    file_contains "$TMPDIR/proj_csharp/README.md" "dotnet run"
check "the C# README gives the mono route the container can run" \
    file_contains "$TMPDIR/proj_csharp/README.md" "mono proj_csharp.exe"
check "the C# README explains who disposes what" \
    file_contains "$TMPDIR/proj_csharp/README.md" "IDisposable"
check "the C# .gitignore covers build output" \
    file_contains "$TMPDIR/proj_csharp/.gitignore" "obj/"

# ---- argument handling ----
out=$(cd "$TMPDIR" && "$TOOL" init proj_cs_short --lang cs --no-ci 2>&1)
check "--lang cs is accepted as csharp" \
    test -f "$TMPDIR/proj_cs_short/Program.cs"

out=$(cd "$TMPDIR" && "$TOOL" init proj_bad --lang rust --no-ci 2>&1 || true)
check "an unsupported language is refused" \
    assert_contains "$out" "Invalid language"
check "and the refusal lists csharp as an option" \
    assert_contains "$out" "csharp"

out=$(cd "$TMPDIR" && "$TOOL" init proj_csharp --lang csharp --no-ci 2>&1 || true)
check "init refuses to overwrite an existing directory" \
    assert_contains "$out" "already exists"

# ---- the CI workflow it writes runs the thing it scaffolded ----
out=$(cd "$TMPDIR" && "$TOOL" init proj_ci --lang csharp 2>&1)
check "a CI workflow is written by default" \
    test -f "$TMPDIR/proj_ci/.github/workflows/bindings.yml"
check "the workflow generates C# bindings" \
    file_contains "$TMPDIR/proj_ci/.github/workflows/bindings.yml" "--lang csharp"
check "the workflow compiles and runs them" \
    file_contains "$TMPDIR/proj_ci/.github/workflows/bindings.yml" "mono proj_ci.exe"

# ---- the part that needs a C# toolchain ----
MCS=""
for c in mcs csc; do command -v "$c" >/dev/null 2>&1 && MCS="$c" && break; done

if [ -z "$MCS" ] || ! command -v mono >/dev/null 2>&1; then
    echo "  SKIP  no C# toolchain on PATH (mcs/mono); the scaffold was checked"
    echo "        structurally only. The dev container installs mono-mcs."
else
    proj="$TMPDIR/proj_live"
    (cd "$TMPDIR" && "$TOOL" init proj_live --lang csharp --no-ci >/dev/null 2>&1)
    gen=$(cd "$proj" && "$TOOL" generate src/ --module proj_live --lang csharp 2>&1)
    check "generate produces the bindings" test -f "$proj/build/proj_live.cs"
    check "generate produces the native library" test -f "$proj/build/libproj_live.so"
    check "generate produces the C header" test -f "$proj/build/proj_live.h"

    # The anti-drift check: init wrote `using <ns>;` before the generator ran.
    ns=$(sed -n 's/^using \([A-Za-z0-9]*\);$/\1/p' "$proj/Program.cs" \
        | grep -v '^System' | head -1)
    check "Program.cs names a namespace at all" test -n "$ns"
    check "the scaffolded namespace is the one the generator emits" \
        file_contains "$proj/build/proj_live.cs" "namespace $ns {"

    if (cd "$proj" && $MCS -out:proj_live.exe Program.cs build/proj_live.cs >/dev/null 2>&1); then
        check "the scaffolded project compiles" true
        run_out=$(cd "$proj" && LD_LIBRARY_PATH=build mono proj_live.exe 2>&1)
        check "and it runs green against C++" \
            assert_contains "$run_out" "all checks passed"
    else
        check "the scaffolded project compiles" false
    fi
fi

echo ""
if [ "$fail" -eq 0 ]; then
    echo "All $pass init tests passed!"
else
    echo "$fail of $((pass + fail)) tests FAILED"
    exit 1
fi
