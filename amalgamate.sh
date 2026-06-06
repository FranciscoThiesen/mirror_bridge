#!/bin/bash
# Mirror Bridge Amalgamation Script
# Creates single-header versions for each language (Python, Lua, JavaScript)
# Inspired by simdjson's amalgamation approach

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/single_header"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}Mirror Bridge Amalgamation${NC}"
echo -e "${BLUE}==========================${NC}"
echo ""

mkdir -p "$OUTPUT_DIR"

# Helper function to extract content between pragma once and end of file
extract_content() {
    local file="$1"
    # Skip the #pragma once line and include guards, extract everything else.
    # Also skip #include statements for mirror_bridge headers.
    # Guard patterns require the _HPP suffix: a bare MIRROR_BRIDGE match
    # would also strip user-facing macro definitions like
    # `#define MIRROR_BRIDGE_VALIDATE(T) \`, leaving its continuation
    # body dangling in the output.
    awk '
        /^#pragma once/ { next }
        /^#ifndef MIRROR_BRIDGE[A-Z_]*_HPP/ { next }
        /^#define MIRROR_BRIDGE[A-Z_]*_HPP/ { next }
        /^#endif.*MIRROR_BRIDGE[A-Z_]*_HPP/ { next }
        /^#include ".*mirror_bridge.*\.hpp"/ { next }
        /^#include "\.\.\/core\/mirror_bridge_core\.hpp"/ { next }
        { print }
    ' "$file"
}

# ============================================================================
# Python Single Header
# ============================================================================
echo -e "${YELLOW}Creating Python single-header...${NC}"

cat > "$OUTPUT_DIR/mirror_bridge_python.hpp" << 'EOF'
#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge - Python Single Header
// ═══════════════════════════════════════════════════════════════════════════
//
// Auto-generated single-header version for Python bindings
// Generated: $(date)
//
// This file contains:
// - Core reflection infrastructure (language-agnostic)
// - Python C API bindings
//
// Usage:
//   #include "mirror_bridge_python.hpp"
//   MIRROR_BRIDGE_MODULE(my_module,
//       mirror_bridge::bind_class<MyClass>(m, "MyClass");
//   )
//
// ═══════════════════════════════════════════════════════════════════════════

EOF

# Add core header content
echo "// ============================================================================" >> "$OUTPUT_DIR/mirror_bridge_python.hpp"
echo "// CORE - Language-Agnostic Reflection Infrastructure" >> "$OUTPUT_DIR/mirror_bridge_python.hpp"
echo "// ============================================================================" >> "$OUTPUT_DIR/mirror_bridge_python.hpp"
echo "" >> "$OUTPUT_DIR/mirror_bridge_python.hpp"

extract_content "$SCRIPT_DIR/core/mirror_bridge_core.hpp" >> "$OUTPUT_DIR/mirror_bridge_python.hpp"

# Add Python bindings content
echo "" >> "$OUTPUT_DIR/mirror_bridge_python.hpp"
echo "// ============================================================================" >> "$OUTPUT_DIR/mirror_bridge_python.hpp"
echo "// PYTHON - Python C API Bindings" >> "$OUTPUT_DIR/mirror_bridge_python.hpp"
echo "// ============================================================================" >> "$OUTPUT_DIR/mirror_bridge_python.hpp"
echo "" >> "$OUTPUT_DIR/mirror_bridge_python.hpp"

# Extract Python content but skip the core include
awk '
    /^#include ".*core\/mirror_bridge_core\.hpp"/ { next }
    /^#pragma once/ { next }
    { print }
' "$SCRIPT_DIR/python/mirror_bridge_python.hpp" >> "$OUTPUT_DIR/mirror_bridge_python.hpp"

echo -e "${GREEN}✓ Created: $OUTPUT_DIR/mirror_bridge_python.hpp${NC}"

# ============================================================================
# Lua Single Header
# ============================================================================
echo -e "${YELLOW}Creating Lua single-header...${NC}"

cat > "$OUTPUT_DIR/mirror_bridge_lua.hpp" << 'EOF'
#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge - Lua Single Header
// ═══════════════════════════════════════════════════════════════════════════
//
// Auto-generated single-header version for Lua bindings
// Generated: $(date)
//
// This file contains:
// - Core reflection infrastructure (language-agnostic)
// - Lua C API bindings
//
// Usage:
//   #include "mirror_bridge_lua.hpp"
//   MIRROR_BRIDGE_LUA_MODULE(my_module,
//       mirror_bridge::lua::bind_class<MyClass>(L, "MyClass");
//   )
//
// ═══════════════════════════════════════════════════════════════════════════

EOF

# Add core header content
echo "// ============================================================================" >> "$OUTPUT_DIR/mirror_bridge_lua.hpp"
echo "// CORE - Language-Agnostic Reflection Infrastructure" >> "$OUTPUT_DIR/mirror_bridge_lua.hpp"
echo "// ============================================================================" >> "$OUTPUT_DIR/mirror_bridge_lua.hpp"
echo "" >> "$OUTPUT_DIR/mirror_bridge_lua.hpp"

extract_content "$SCRIPT_DIR/core/mirror_bridge_core.hpp" >> "$OUTPUT_DIR/mirror_bridge_lua.hpp"

# Add Lua bindings content
echo "" >> "$OUTPUT_DIR/mirror_bridge_lua.hpp"
echo "// ============================================================================" >> "$OUTPUT_DIR/mirror_bridge_lua.hpp"
echo "// LUA - Lua C API Bindings" >> "$OUTPUT_DIR/mirror_bridge_lua.hpp"
echo "// ============================================================================" >> "$OUTPUT_DIR/mirror_bridge_lua.hpp"
echo "" >> "$OUTPUT_DIR/mirror_bridge_lua.hpp"

# Extract Lua content but skip the core include
awk '
    /^#include ".*core\/mirror_bridge_core\.hpp"/ { next }
    /^#pragma once/ { next }
    { print }
' "$SCRIPT_DIR/lua/mirror_bridge_lua.hpp" >> "$OUTPUT_DIR/mirror_bridge_lua.hpp"

echo -e "${GREEN}✓ Created: $OUTPUT_DIR/mirror_bridge_lua.hpp${NC}"

# ============================================================================
# JavaScript Single Header
# ============================================================================
echo -e "${YELLOW}Creating JavaScript single-header...${NC}"

cat > "$OUTPUT_DIR/mirror_bridge_javascript.hpp" << 'EOF'
#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge - JavaScript Single Header
// ═══════════════════════════════════════════════════════════════════════════
//
// Auto-generated single-header version for JavaScript (Node.js N-API) bindings
// Generated: $(date)
//
// This file contains:
// - Core reflection infrastructure (language-agnostic)
// - JavaScript N-API bindings
//
// Usage:
//   #include "mirror_bridge_javascript.hpp"
//   MIRROR_BRIDGE_JS_MODULE(my_module,
//       mirror_bridge::javascript::bind_class<MyClass>(env, m, "MyClass");
//   )
//
// ═══════════════════════════════════════════════════════════════════════════

EOF

# Add core header content
echo "// ============================================================================" >> "$OUTPUT_DIR/mirror_bridge_javascript.hpp"
echo "// CORE - Language-Agnostic Reflection Infrastructure" >> "$OUTPUT_DIR/mirror_bridge_javascript.hpp"
echo "// ============================================================================" >> "$OUTPUT_DIR/mirror_bridge_javascript.hpp"
echo "" >> "$OUTPUT_DIR/mirror_bridge_javascript.hpp"

extract_content "$SCRIPT_DIR/core/mirror_bridge_core.hpp" >> "$OUTPUT_DIR/mirror_bridge_javascript.hpp"

# Add JavaScript bindings content
echo "" >> "$OUTPUT_DIR/mirror_bridge_javascript.hpp"
echo "// ============================================================================" >> "$OUTPUT_DIR/mirror_bridge_javascript.hpp"
echo "// JAVASCRIPT - Node.js N-API Bindings" >> "$OUTPUT_DIR/mirror_bridge_javascript.hpp"
echo "// ============================================================================" >> "$OUTPUT_DIR/mirror_bridge_javascript.hpp"
echo "" >> "$OUTPUT_DIR/mirror_bridge_javascript.hpp"

# Extract JavaScript content but skip the core include
awk '
    /^#include ".*core\/mirror_bridge_core\.hpp"/ { next }
    /^#pragma once/ { next }
    { print }
' "$SCRIPT_DIR/javascript/mirror_bridge_javascript.hpp" >> "$OUTPUT_DIR/mirror_bridge_javascript.hpp"

echo -e "${GREEN}✓ Created: $OUTPUT_DIR/mirror_bridge_javascript.hpp${NC}"

# ============================================================================
# Summary
# ============================================================================
echo ""
echo -e "${BLUE}Amalgamation Complete!${NC}"
echo ""
echo -e "${GREEN}Single-header files created in: $OUTPUT_DIR/${NC}"
echo ""
echo "  📄 mirror_bridge_python.hpp     (Python C API)"
echo "  📄 mirror_bridge_lua.hpp        (Lua C API)"
echo "  📄 mirror_bridge_javascript.hpp (Node.js N-API)"
echo ""
echo -e "${BLUE}Usage:${NC}"
echo ""
echo -e "${YELLOW}Python:${NC}"
echo "  cp single_header/mirror_bridge_python.hpp /path/to/your/project/"
echo "  #include \"mirror_bridge_python.hpp\""
echo ""
echo -e "${YELLOW}Lua:${NC}"
echo "  cp single_header/mirror_bridge_lua.hpp /path/to/your/project/"
echo "  #include \"mirror_bridge_lua.hpp\""
echo ""
echo -e "${YELLOW}JavaScript:${NC}"
echo "  cp single_header/mirror_bridge_javascript.hpp /path/to/your/project/"
echo "  #include \"mirror_bridge_javascript.hpp\""
echo ""

# Generate line counts
if command -v wc &> /dev/null; then
    echo -e "${BLUE}File Sizes:${NC}"
    echo ""
    for file in "$OUTPUT_DIR"/*.hpp; do
        lines=$(wc -l < "$file")
        basename=$(basename "$file")
        printf "  %-40s %6d lines\n" "$basename" "$lines"
    done
    echo ""
fi
