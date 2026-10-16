#!/bin/bash
# Batch siblings are generated only under -DMIRROR_BRIDGE_BATCH_METHODS, so the
# module is built both ways and the same checks run against each. The expected
# set of generated names differs; nothing else about the module may.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

CXX="${MB_CXX:-}"
if [ -z "$CXX" ]; then
    if command -v clang++ >/dev/null 2>&1; then CXX=clang++; else CXX=g++; fi
fi

if [ "$(basename "$CXX")" = "g++" ]; then
    FLAGS=(-std=c++26 -freflection)
else
    FLAGS=(-std=c++2c -freflection -freflection-latest -stdlib=libc++)
fi

# shellcheck disable=SC2207
PYINC=($(python3-config --includes))

status=0

for mode in off on; do
    if [ "$mode" = on ]; then
        DEFS=(-DMIRROR_BRIDGE_BATCH_METHODS)
        export MB_BATCH_ENABLED=1
    else
        DEFS=()
        export MB_BATCH_ENABLED=0
    fi

    mkdir -p "$OUT/$mode"
    echo "Building with generation $mode..."
    "$CXX" "${FLAGS[@]}" -O1 -shared -fPIC "${DEFS[@]+"${DEFS[@]}"}" \
        -I"$ROOT" -I"$HERE" "${PYINC[@]}" \
        "$HERE/batch_methods_binding.cpp" -o "$OUT/$mode/batched.so" || { status=1; continue; }

    PYTHONPATH="$OUT/$mode" python3 "$HERE/test_batched.py" || status=1
    echo
done

exit $status
