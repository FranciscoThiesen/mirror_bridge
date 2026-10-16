#!/bin/bash
# buffer_shape_check.cpp is a standalone program rather than a binding, so it
# needs its own compile. See the comment at the top of it for why the thing it
# covers cannot be reached from Python.
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
# python3-config --ldflags omits -lpython on some builds, so name it from the
# interpreter rather than hoping.
PYVER="$(python3 -c 'import sys; print(f"{sys.version_info[0]}.{sys.version_info[1]}")')"
# shellcheck disable=SC2207
PYLD=($(python3-config --ldflags))

echo "Compiling buffer_shape_check.cpp with $CXX..."
"$CXX" "${FLAGS[@]}" -O1 -I"$ROOT" "${PYINC[@]}" \
    "$HERE/buffer_shape_check.cpp" -o "$OUT/buffer_shape_check" \
    "${PYLD[@]}" "-lpython$PYVER" || exit 1

echo "Running..."
"$OUT/buffer_shape_check"
