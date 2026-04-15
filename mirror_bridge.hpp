#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge - Automatic C++ Bindings to Multiple Languages via C++26 Reflection
// ═══════════════════════════════════════════════════════════════════════════
//
// Multi-language support: Python, JavaScript (Node.js & V8), Lua
//
// This header provides backward compatibility by including Python bindings by default.
// For other languages, include the specific binding header:
//   - python/mirror_bridge_python.hpp     - Python C API bindings
//   - javascript/mirror_bridge_javascript.hpp - Node.js N-API bindings
//   - javascript/mirror_bridge_v8.hpp     - Direct V8 API bindings (for embedded V8)
//   - lua/mirror_bridge_lua.hpp           - Lua C API bindings
//
// ═══════════════════════════════════════════════════════════════════════════

// Default: Include Python bindings for backward compatibility
#include "python/mirror_bridge_python.hpp"

// Auto-detect Eigen and include type converters if available.
// This enables zero-effort binding of classes using Eigen::Vector3d, Matrix4d, etc.
#include "python/mirror_bridge_eigen.hpp"
