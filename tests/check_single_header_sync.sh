#!/usr/bin/env bash
#
# single_header/*.hpp are generated from the modular headers by amalgamate.sh
# and committed, so the two can drift apart with nothing to notice: a change
# under python/ or core/ that skips the regeneration leaves anyone using the
# single-header distribution on different code from anyone using the modular
# headers. No workflow referenced amalgamate.sh or single_header/ before this.
#
# Regeneration happens in a copy of only the directories amalgamate.sh reads,
# so running this never touches the working tree.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

mkdir -p "$work/repo"
cp "$ROOT/amalgamate.sh" "$work/repo/"
for d in core python lua javascript single_header; do
    cp -R "$ROOT/$d" "$work/repo/$d"
done

if ! (cd "$work/repo" && ./amalgamate.sh) > "$work/amalgamate.log" 2>&1; then
    echo "✗ amalgamate.sh failed"
    tail -20 "$work/amalgamate.log"
    exit 1
fi

rc=0
for committed in "$ROOT"/single_header/*.hpp; do
    name="$(basename "$committed")"
    fresh="$work/repo/single_header/$name"

    # A truncated regeneration has bitten this repo before and exited 0 while
    # doing it, so the line count is checked rather than only the diff.
    fresh_lines="$(wc -l < "$fresh" 2>/dev/null || echo 0)"
    if [ "$fresh_lines" -lt 100 ]; then
        echo "✗ $name regenerated as only $fresh_lines lines — regeneration is broken, not the committed file"
        rc=1
        continue
    fi

    if diff -q "$committed" "$fresh" > /dev/null 2>&1; then
        echo "✓ $name in sync ($fresh_lines lines)"
    else
        echo "✗ $name is out of date"
        diff "$committed" "$fresh" | head -20
        rc=1
    fi
done

if [ "$rc" -ne 0 ]; then
    echo
    echo "Run ./amalgamate.sh and commit single_header/."
    echo "On a merge conflict inside a generated header, take either side and regenerate."
fi
exit "$rc"
