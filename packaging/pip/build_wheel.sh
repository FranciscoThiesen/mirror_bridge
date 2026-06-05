#!/bin/bash
# Build the mirror-bridge pip package.
#
# Stages the runtime (headers + CLI scripts) from the repo root into the
# package's _runtime directory, then builds sdist + wheel with python -m
# build. Run from anywhere; paths are derived from this script's location.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RUNTIME_DIR="$SCRIPT_DIR/src/mirror_bridge_cli/_runtime"

rm -rf "$RUNTIME_DIR" "$SCRIPT_DIR/dist" "$SCRIPT_DIR"/src/*.egg-info
mkdir -p "$RUNTIME_DIR"

# Directories the CLI needs at runtime: PROJECT_ROOT is the runtime root,
# so the layout must mirror the repo (compile_binding passes -I$PROJECT_ROOT).
for dir in core python lua javascript rust tools scripts; do
    cp -R "$REPO_ROOT/$dir" "$RUNTIME_DIR/$dir"
done

cp "$REPO_ROOT"/mirror_bridge.hpp \
   "$REPO_ROOT"/mirror_bridge_pch.hpp \
   "$REPO_ROOT"/mirror_bridge_doctor \
   "$REPO_ROOT"/mirror_bridge_auto \
   "$REPO_ROOT"/mirror_bridge_build \
   "$REPO_ROOT"/mirror_bridge_generate \
   "$REPO_ROOT"/LICENSE \
   "$RUNTIME_DIR/"

find "$RUNTIME_DIR" -name '__pycache__' -type d -exec rm -rf {} + 2>/dev/null || true
find "$RUNTIME_DIR" -name '.DS_Store' -delete 2>/dev/null || true

echo "Staged runtime: $(find "$RUNTIME_DIR" -type f | wc -l | tr -d ' ') files"

cd "$SCRIPT_DIR"
python3 -m build

echo ""
echo "Artifacts in $SCRIPT_DIR/dist/:"
ls -lh dist/
echo ""
echo "To publish: python3 -m twine upload dist/*"
