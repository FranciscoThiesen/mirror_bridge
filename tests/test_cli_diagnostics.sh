#!/bin/bash
# What the CLI says when it cannot bind something.
#
# Each case here used to be silent, or to surface only as a C++ error inside a
# generated file the CLI then deletes. The assertions are on the words the
# user reads, because that is the whole feature.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL="$SCRIPT_DIR/../tools/mirror_bridge"
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

pass=0
fail=0

check() {
    local name="$1" haystack="$2" needle="$3"
    if [[ "$haystack" == *"$needle"* ]]; then
        echo "  ✓ $name"
        pass=$((pass + 1))
    else
        echo "  ✗ $name"
        echo "      expected to find: $needle"
        echo "      in:"
        printf '%s\n' "$haystack" | sed 's/^/        /' | head -25
        fail=$((fail + 1))
    fi
}

check_absent() {
    local name="$1" haystack="$2" needle="$3"
    if [[ "$haystack" != *"$needle"* ]]; then
        echo "  ✓ $name"
        pass=$((pass + 1))
    else
        echo "  ✗ $name (unexpectedly found: $needle)"
        fail=$((fail + 1))
    fi
}

# --------------------------------------------------------------------------
echo "Enums are not classes, and are reported rather than dropped"
# --------------------------------------------------------------------------
mkdir -p "$TMPDIR/enums/src"
cat > "$TMPDIR/enums/src/trading.hpp" <<'EOF'
#pragma once
namespace trading {
enum class Side { Buy, Sell };
enum Venue { NYSE, NASDAQ };
struct Order {
    double px = 0.0;
    Side side = Side::Buy;
    double price() const { return px; }
};
}
EOF
out=$("$TOOL" generate "$TMPDIR/enums/src" --module tdiag --lang python \
      --output "$TMPDIR/enums/build" --stubs --force 2>&1)

# `enum class Side` contains the token `class`; the discovery scan used to
# report it as a class to bind, and a plain `enum Venue` was invisible.
check "scoped enum is not discovered as a class" "$out" "Discovered 1 classes"
check_absent "enum is not listed as a found class" "$out" "Found: Side"
# The Python backend binds them now, so they are not names missing from the
# module and the "not bound" report must not claim them.
check_absent "enums are not reported as unbound for Python" "$out" "enum type(s)"

# The stub used to annotate enum members with the C++ enum's own name, which
# the module never defines, making the whole file unusable to a type checker.
# Now the module does define it, so naming it is both true and more useful.
pyi="$TMPDIR/enums/build/tdiag.pyi"
if [ -f "$pyi" ]; then
    stub=$(cat "$pyi")
    check "stub declares the enum" "$stub" "class Side(_IntEnum):"
    check "stub declares the plain enum too" "$stub" "class Venue(_IntEnum):"
    check "stub annotates an enum member with the enum" "$stub" "side: Side"
    check "and imports IntEnum under an alias" "$stub" "from enum import IntEnum as _IntEnum"
else
    echo "  ✗ no stub generated at $pyi"
    fail=$((fail + 1))
fi

# The same enums through the Lua backend, which has no enum object to map
# onto and so still reports them.
lua_out=$("$TOOL" generate "$TMPDIR/enums/src" --module tdiag_lua --lang lua \
      --output "$TMPDIR/enums/build_lua" --force 2>&1)
check "Lua still reports enums as unbound" "$lua_out" "enum type(s)"
check "and names them" "$lua_out" "Side"
check "and says which backend does bind them" "$lua_out" "enum.IntEnum"

# --------------------------------------------------------------------------
echo "Free functions that cannot be bound say so after the banner"
# --------------------------------------------------------------------------
mkdir -p "$TMPDIR/unbound/src"
cat > "$TMPDIR/unbound/src/book.hpp" <<'EOF'
#pragma once
namespace lib {
struct Curve { double r = 0.05; };
inline double price(double s) { return s; }
inline double price(double s, const Curve& c) { return s * (1 - c.r); }
template <typename T> T vwap(T a) { return a; }
inline double vwap(double a, double b) { return (a + b) / 2; }
inline double solo(double x) { return x + 1; }
}
EOF
out=$("$TOOL" generate "$TMPDIR/unbound/src" --module bdiag --lang python \
      --output "$TMPDIR/unbound/build" --force 2>&1)

check "an overloaded free function is reported" "$out" "overloaded in C++"
check "the overloaded name is given" "$out" "lib::price"
check "a name shared with a template is reported" "$out" "sharing a name with a function template"
check "that name is given" "$out" "lib::vwap"
check "the report offers a way out" "$out" "Fix:"
# The report must come after the result, not eight lines before it.
banner_line=$(printf '%s\n' "$out" | grep -n "Successfully built" | head -1 | cut -d: -f1)
warn_line=$(printf '%s\n' "$out" | grep -n "were not bound into" | head -1 | cut -d: -f1)
if [ -n "$banner_line" ] && [ -n "$warn_line" ] && [ "$warn_line" -gt "$banner_line" ]; then
    echo "  ✓ the report is printed after the success banner"
    pass=$((pass + 1))
else
    echo "  ✗ the report is not after the success banner (banner=$banner_line warn=$warn_line)"
    fail=$((fail + 1))
fi

# --------------------------------------------------------------------------
echo "Two classes with one binding name are caught before codegen"
# --------------------------------------------------------------------------
mkdir -p "$TMPDIR/dup/src"
cat > "$TMPDIR/dup/src/shapes.hpp" <<'EOF'
#pragma once
namespace lib {
    struct Config { int a = 1; int get() const { return a; } };
    namespace detail {
        struct Config { int b = 2; int get() const { return b; } };
    }
}
EOF
out=$("$TOOL" generate "$TMPDIR/dup/src" --module ddiag --lang python \
      --output "$TMPDIR/dup/build" --force 2>&1)

check "the collision is named" "$out" "'Config' is the binding name of 2 classes"
check "both declaration sites are shown" "$out" "shapes.hpp:3"
check "the second site too" "$out" "shapes.hpp:5"
check "it explains the unqualified-name rule" "$out" "unqualified name"
# It used to say "the last one bound replaces the rest", which was true and
# was the bug: the last one bound is whichever the header declared second, so
# lib::detail::Config took the name and the public class was unreachable.
# The choice is made before codegen now, and the message names it.
check "it names the class that takes the name" "$out" "lib::Config is bound"
check "and says the others are not bound" "$out" "the rest are not bound at all"
check "it offers the opt-out marker" "$out" "MIRROR_BRIDGE_SKIP"
check "the loser is listed in the unbound report" "$out" "lib::detail::Config"

# --------------------------------------------------------------------------
echo "diff refuses to record a baseline that would pass forever"
# --------------------------------------------------------------------------
mkdir -p "$TMPDIR/dg/src/sub"
cat > "$TMPDIR/dg/src/sub/deep.hpp" <<'EOF'
#pragma once
struct Deep { int a = 1; };
EOF
out=$("$TOOL" diff "$TMPDIR/dg/src" --output "$TMPDIR/dg/build" 2>&1)
rc=$?
check "an empty snapshot is an error" "$out" "found no headers to snapshot"
check "it says why" "$out" "subdirectories"
check "it names the honest alternative" "$out" ".pyi"
if [ "$rc" -eq 3 ]; then
    echo "  ✓ exits 3"
    pass=$((pass + 1))
else
    echo "  ✗ expected exit 3, got $rc"
    fail=$((fail + 1))
fi

out=$("$TOOL" diff "$TMPDIR/dg/src" --module demo 2>&1)
rc=$?
check "an unknown flag is rejected" "$out" "unknown option '--module'"
if [ "$rc" -eq 1 ]; then
    echo "  ✓ exits 1"
    pass=$((pass + 1))
else
    echo "  ✗ expected exit 1, got $rc"
    fail=$((fail + 1))
fi

# A directory diff can actually scan must keep working.
cat > "$TMPDIR/dg/src/top.hpp" <<'EOF'
#pragma once
struct Top { int a = 1; };
EOF
out=$("$TOOL" diff "$TMPDIR/dg/src" --output "$TMPDIR/dg/build" 2>&1)
check "a scannable directory still snapshots" "$out" "Snapshot saved"
out=$("$TOOL" diff "$TMPDIR/dg/src" --output "$TMPDIR/dg/build" --check 2>&1)
rc=$?
check "and --check still passes on no drift" "$out" "No binding surface changes"
if [ "$rc" -eq 0 ]; then
    echo "  ✓ exits 0"
    pass=$((pass + 1))
else
    echo "  ✗ expected exit 0, got $rc"
    fail=$((fail + 1))
fi

echo ""
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
