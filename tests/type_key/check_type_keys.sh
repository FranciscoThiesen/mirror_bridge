#!/bin/bash
# Compiles the type_key static_asserts. core::type_key is the cross-module
# identity of a bound class, so two distinct types sharing a key is a type
# confusion at the Lua and JavaScript boundaries; the assertions pin the
# cases that went wrong and the ones that must not move.
#
# Usage: check_type_keys.sh <c++ compiler> [extra flags...]
set -euo pipefail

CXX="${1:?compiler}"
shift
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

# Same flag split as the CLI and CMakeLists.txt: clang-p2996 needs its
# bundled libc++ to find <meta>; stock GCC 16+ uses plain -freflection.
case "$(basename "$CXX")" in
    *clang*) REFLECT_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++" ;;
    *)       REFLECT_FLAGS="-std=c++26 -freflection" ;;
esac

# shellcheck disable=SC2086
"$CXX" $REFLECT_FLAGS -fsyntax-only -I"$ROOT" "$@" "$HERE/type_key_checks.cpp"
echo "type_key distinctness holds with $CXX"
