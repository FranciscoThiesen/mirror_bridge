#!/usr/bin/env bash
# The generated C ABI, consumed from plain C++17 and plain C99.
#
# This is issue #3, asked in December 2025: "would there be a way to use a
# version of this (within your Docker image with the compiler that supports it)
# to generate the bindings, and then use them from, say, C++17 in a normal
# compiler?" The C# backend's shim is already that -- a flat C ABI with no
# reflection in it -- so the only thing missing was the header, which is now
# emitted beside it.
#
# The point of the test is the compiler flags: the consumer is built with
# -std=c++17 and -std=c99 by a compiler that does not know what reflection is.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

PASS=0; FAIL=0
ok()  { PASS=$((PASS+1)); echo "  PASS  $1"; }
bad() { FAIL=$((FAIL+1)); echo "  FAIL  $1"; }

echo "=== generate ==="
if "$ROOT/tools/mirror_bridge" generate "$HERE" --module cab --lang csharp \
       -o "$WORK" >"$WORK/gen.log" 2>&1; then
    ok "the csharp backend produced the ABI"
else
    bad "generate failed"
    tail -20 "$WORK/gen.log"
    echo "FAIL: C ABI consumer"
    exit 1
fi

for f in cab.h cab_shim.cpp; do
    if [ -f "$WORK/$f" ]; then ok "$f was written"; else bad "$f is missing"; fi
done
LIB="$(ls "$WORK"/libcab.so "$WORK"/libcab.dylib "$WORK"/cab.dll 2>/dev/null | head -1)"
if [ -n "$LIB" ]; then ok "the shared library was built"; else bad "no shared library"; fi

echo "=== the header is self-contained and reflection-free ==="
for bad_token in '<meta>' '^^' '[:' 'MB_EXPORT' 'std::'; do
    if grep -qF "$bad_token" "$WORK/cab.h"; then
        bad "cab.h contains $bad_token, so it is not plain C"
    else
        ok "no $bad_token in cab.h"
    fi
done
if grep -q "mirror_bridge" "$WORK/cab.h" && ! grep -q '#include "' "$WORK/cab.h"; then
    ok "it includes no project header, only <stdint.h>"
else
    bad "cab.h pulls in something other than <stdint.h>"
fi

cat > "$WORK/use.cpp" <<'EOF'
#include "cab.h"
#include <cstdio>

int main() {
    void* r = mb_Reading_new();
    if (!r) { std::puts("construct failed"); return 1; }
    mb_Reading_set_id(r, 21);
    mb_Reading_set_value(r, 2.5);
    std::printf("id=%d scaled2=%g\n", mb_Reading_get_id(r), mb_Reading_scaled(r, 2.0));

    const char* tag = mb_Reading_get_tag(r);
    std::printf("tag=%s\n", tag ? tag : "(null)");
    mb_free_string(tag);

    double xs[3] = {1.0, 2.5, 3.5};
    std::printf("sum_of=%g\n", mb_Reading_sum_of(r, xs, 3));

    void* v = mb_Reading_get_history(r);
    mb_vec_double_add(v, 9.5);
    std::printf("history=%lld,%g\n", (long long)mb_vec_double_size(v),
                mb_vec_double_get(v, 0));

    mb_Reading_boom(r);
    std::printf("kind=%d msg=%s\n", mb_last_error_kind(), mb_last_error());
    mb_clear_error();

    mb_Reading_delete(r);
    std::puts("CPP17 OK");
    return 0;
}
EOF

echo "=== a C++17 consumer, built by a compiler with no reflection ==="
CXX_PLAIN="${CXX:-g++}"
command -v "$CXX_PLAIN" >/dev/null 2>&1 || CXX_PLAIN=c++
if "$CXX_PLAIN" -std=c++17 -Wall -Wextra "$WORK/use.cpp" -I"$WORK" \
       -L"$(dirname "$LIB")" -lcab -o "$WORK/use" 2>"$WORK/cxx.log"; then
    ok "it compiles at -std=c++17 with -Wall -Wextra"
else
    bad "the C++17 consumer did not compile"
    head -12 "$WORK/cxx.log"
fi

if [ -x "$WORK/use" ]; then
    out="$(LD_LIBRARY_PATH="$(dirname "$LIB")" "$WORK/use" 2>&1)"
    echo "$out" | sed 's/^/    /'
    for want in "id=21 scaled2=5" "tag=probe" "sum_of=7" "history=1,9.5" "CPP17 OK"; do
        if echo "$out" | grep -qF "$want"; then ok "output has: $want"; else bad "missing: $want"; fi
    done
    # out_of_range maps to kind 3, the same ladder the C# side reads.
    if echo "$out" | grep -q "kind=3"; then
        ok "the exception kind crosses as well"
    else
        bad "exception kind was not reported"
    fi
fi

echo "=== and from plain C99, since the header claims to be C ==="
if command -v gcc >/dev/null 2>&1 || command -v cc >/dev/null 2>&1; then
    CC_PLAIN="$(command -v gcc || command -v cc)"
    sed -e 's|#include <cstdio>|#include <stdio.h>|' -e 's|std::||g' \
        "$WORK/use.cpp" > "$WORK/use.c"
    if "$CC_PLAIN" -std=c99 -Wall "$WORK/use.c" -I"$WORK" \
           -L"$(dirname "$LIB")" -lcab -o "$WORK/usec" 2>"$WORK/cc.log"; then
        ok "it compiles at -std=c99 too"
        if LD_LIBRARY_PATH="$(dirname "$LIB")" "$WORK/usec" | grep -q "CPP17 OK"; then
            ok "and runs"
        else
            bad "the C99 consumer did not run correctly"
        fi
    else
        bad "the C99 consumer did not compile"
        head -10 "$WORK/cc.log"
    fi
else
    echo "  SKIP  no C compiler on PATH"
fi

echo ""
echo "$PASS passed, $FAIL failed"
if [ "$FAIL" -ne 0 ]; then
    echo "FAIL: C ABI consumer"
    exit 1
fi
echo "PASS: C ABI consumer"
