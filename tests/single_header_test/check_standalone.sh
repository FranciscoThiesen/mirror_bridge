#!/bin/bash
# Proves the Python single header compiles with NO access to the repo tree.
# The regular robot_py test builds with the repo root on the include path,
# which is how a single header that #included "python/..." shipped in 0.3.0
# (issue #12). Here the header is copied into an empty directory and the
# binding is compiled against that directory alone.
#
# Usage: check_standalone.sh <c++ compiler> <python include dir> [extra flags...]
set -euo pipefail

CXX="${1:?compiler}"
PY_INC="${2:?python include dir}"
shift 2
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

cp "$ROOT/single_header/mirror_bridge_python.hpp" "$WORK/"
cp "$HERE/robot.hpp" "$WORK/"
# Same binding as the regular test, but including the header by bare name,
# the way a user who copied one file into their project would.
sed 's|#include "../../single_header/mirror_bridge_python.hpp"|#include "mirror_bridge_python.hpp"|' \
    "$HERE/robot_py_binding.cpp" > "$WORK/robot_py_binding.cpp"

# Same flag split as the CLI and CMakeLists.txt: clang-p2996 needs its
# bundled libc++ to find <meta>; stock GCC 16+ uses plain -freflection.
case "$(basename "$CXX")" in
    *clang*) REFLECT_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++" ;;
    *)       REFLECT_FLAGS="-std=c++26 -freflection" ;;
esac

cd "$WORK"
# shellcheck disable=SC2086
"$CXX" $REFLECT_FLAGS -fsyntax-only -I"$WORK" -I"$PY_INC" "$@" robot_py_binding.cpp
echo "single header compiles standalone with $CXX"
