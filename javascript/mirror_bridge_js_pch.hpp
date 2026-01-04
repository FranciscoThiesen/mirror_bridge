#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge JavaScript - Precompiled Header (OPTIONAL)
// ═══════════════════════════════════════════════════════════════════════════
//
// This file is designed to be precompiled for faster JavaScript/Node.js
// binding compilation.
//
// USAGE (OPTIONAL - NOT REQUIRED):
//
// Step 1: Precompile this header (do once):
//   clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
//     -I/usr/include/node \
//     -x c++-header javascript/mirror_bridge_js_pch.hpp -o build/mirror_bridge_js_pch.hpp.gch
//
// Step 2: Use the precompiled header in your bindings:
//   clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
//     -include-pch build/mirror_bridge_js_pch.hpp.gch your_js_bindings.cpp
//
// PERFORMANCE IMPACT:
//   - Expected improvement: 3-4x faster compilation
//   - First compilation (building PCH): slightly slower
//   - Subsequent compilations: significantly faster
//
// ═══════════════════════════════════════════════════════════════════════════

#include "mirror_bridge_javascript.hpp"
