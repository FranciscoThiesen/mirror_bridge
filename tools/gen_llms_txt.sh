#!/bin/bash
# Generates llms.txt (curated index) and llms-full.txt (full docs corpus)
# at the repo root, following the llmstxt.org convention.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

BASE="https://github.com/FranciscoThiesen/mirror_bridge/blob/main"
INDEX="$ROOT/llms.txt"
FULL="$ROOT/llms-full.txt"

# Emit a curated "- [title](url): desc" line, but only if the file exists.
# Skipping (and warning) on missing files keeps llms.txt free of dead links.
link() {
    local path="$1" title="$2" desc="$3"
    if [ -f "$ROOT/$path" ]; then
        echo "- [$title]($BASE/$path): $desc" >> "$INDEX"
    else
        echo "WARN: skipping missing file: $path" >&2
    fi
}

# ---------------------------------------------------------------------------
# llms.txt — curated index
# ---------------------------------------------------------------------------
cat > "$INDEX" << 'EOF'
# mirror_bridge

> mirror_bridge is a C++26 reflection-based binding generator: write your C++ once and get Python, Lua, and JavaScript bindings auto-generated with zero boilerplate. Classes and functions are discovered automatically via reflection, so there is no per-binding glue code to maintain. It requires the Bloomberg clang-p2996 reflection compiler, most easily used through the provided Docker image.

## Docs
EOF
link "README.md"                          "README"               "Project overview, value proposition, and quick example"
link "docs/getting-started/installation.md" "Installation"       "Set up the C++26 reflection toolchain via Docker"
link "docs/getting-started/quickstart.md"   "Quick Start"        "Get bindings working in five minutes"
link "docs/getting-started/first-binding.md" "Your First Binding" "Step-by-step first C++ to Python binding"
link "docs/guides/workflow.md"            "Development Workflow"  "Recommended day-to-day workflow"
link "docs/guides/multi-language.md"      "Multi-Language Support" "Generate Python, Lua, and JavaScript from one source"
link "docs/guides/packaging.md"           "Packaging"            "Distribute bindings as installable packages"
link "docs/guides/single-header.md"       "Single-Header"        "Integrate via amalgamated single-header builds"
link "docs/guides/pch-optimization.md"    "PCH Optimization"     "Cut compile times with precompiled headers"

echo "" >> "$INDEX"
echo "## Reference" >> "$INDEX"
link "docs/reference/api.md"              "API Reference"        "C++ binding functions, concepts, and macros"
link "docs/reference/cli.md"              "CLI Reference"        "The unified mirror_bridge command-line interface"
link "docs/reference/configuration.md"    "Configuration"       "Config files for explicit control over bindings"
link "docs/reference/type-conversion.md"  "Type Conversion"     "Supported C++ to target-language type mappings"
link "docs/reference/errors.md"           "Errors"              "Error messages and troubleshooting reference"
link "docs/internals/architecture.md"     "Architecture"        "How the reflection-driven generator works internally"
link "docs/internals/features.md"         "Feature Matrix"      "Verified features across all supported languages"

echo "" >> "$INDEX"
echo "## Examples" >> "$INDEX"
link "examples/README.md"                 "Examples"             "Progressive examples from beginner to production"
link "docs/MIGRATION_FROM_PYBIND11.md"    "Migrating from pybind11" "Transition guide for pybind11 users"
link "docs/ZERO_COPY_BUFFERS.md"          "Zero-Copy Buffers"   "Zero-copy buffer protocol support for Python"

echo "" >> "$INDEX"
echo "## Optional" >> "$INDEX"
link "docs/internals/benchmarks.md"       "Benchmarks"           "Performance comparison vs traditional binding libraries"
link "docs/guides/contributing.md"        "Contributing"         "Develop, test, and contribute to mirror_bridge"
link "docs/playground.md"                 "Playground"           "Try mirror_bridge online with no local install"
link "CHANGELOG.md"                       "Changelog"            "Notable changes per release"

# ---------------------------------------------------------------------------
# llms-full.txt — full docs corpus in reading order
# ---------------------------------------------------------------------------
# Globs are used for the docs dirs so files added later are picked up
# automatically while preserving the section reading order.
: > "$FULL"
emit() {
    local path="$1"
    [ -f "$ROOT/$path" ] || return 0
    printf -- '---\n# FILE: %s\n---\n\n' "$path" >> "$FULL"
    cat "$ROOT/$path" >> "$FULL"
    printf '\n\n' >> "$FULL"
}

emit "README.md"
for f in docs/getting-started/*.md; do emit "$f"; done
for f in docs/guides/*.md; do emit "$f"; done
for f in docs/reference/*.md; do emit "$f"; done
emit "docs/internals/architecture.md"
emit "docs/internals/features.md"
emit "docs/MIGRATION_FROM_PYBIND11.md"
emit "examples/README.md"
emit "CHANGELOG.md"

echo "Wrote $INDEX ($(wc -c < "$INDEX" | tr -d ' ') bytes)"
echo "Wrote $FULL ($(wc -c < "$FULL" | tr -d ' ') bytes)"
