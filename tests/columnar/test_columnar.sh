#!/bin/bash
# columnar_layout_check.cpp is a standalone program rather than a binding, so it
# needs its own compile. See the comment at the top of it for why it is not a binding. Needs numpy,
# which the clang dev image has and the GCC one does not.
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

if ! python3 -c "import numpy" 2>/dev/null; then
    echo "SKIP: numpy is not installed, and this test asks numpy what it made"
    echo "      of the descriptor rather than asserting on the text of it."
    exit 0
fi

echo "Compiling columnar_layout_check.cpp with $CXX..."
"$CXX" "${FLAGS[@]}" -O1 -I"$ROOT" "${PYINC[@]}" \
    "$HERE/columnar_layout_check.cpp" -o "$OUT/columnar_layout_check" \
    "${PYLD[@]}" "-lpython$PYVER" || exit 1

echo "Running..."
"$OUT/columnar_layout_check"
