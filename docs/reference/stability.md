# API Stability

mirror_bridge is pre-1.0, but adopters need to know what they can build on.
This page is that contract.

## Stable

These surfaces will not break without a deprecation period spanning at least
one minor release, an entry in the CHANGELOG, and a migration note:

| Surface | Examples |
|---|---|
| Module macros | `MIRROR_BRIDGE_MODULE`, `MIRROR_BRIDGE_LUA_MODULE`, `MIRROR_BRIDGE_JS_MODULE` |
| Binding entry points | `mirror_bridge::bind_class<T>(m, "Name")` and the Lua/JS equivalents |
| Field annotations | `[[=exclude{}]]`, `[[=readonly{}]]` (P3394) |
| Discovery opt-outs | `// MIRROR_BRIDGE_SKIP`, `// MIRROR_BRIDGE_SKIP_FILE` |
| CLI commands & flags | `generate`, `build`, `pch`, `diff`, `watch`, `doctor`, `version` and their documented options |
| `--json` output | Existing fields keep their names and types; changes are additive only |
| Type-conversion contract | The mappings in [type-conversion.md](type-conversion.md), including `std::vector<numeric>` → `array.array` |
| Umbrella headers | `mirror_bridge.hpp`, `lua/mirror_bridge_lua.hpp`, `javascript/mirror_bridge_javascript.hpp` |
| CMake target | `mirror_bridge::mirror_bridge` via `find_package`/FetchContent |

## Experimental

These may change or be removed in any release; pin a tag if you depend on them:

- The Rust backend (`generate_bindings<T>()` codegen)
- V8 direct-API bindings (`mirror_bridge_v8.hpp`)
- Auto-trampoline internals (the vtable-swap mechanism; the *behavior* —
  Python subclasses overriding C++ virtuals — is intended to stay)
- The `.mirror` config-file format
- Any header or function not documented in [api.md](api.md)

## Versioning

[Semantic versioning](https://semver.org) interpreted for pre-1.0:

- **Patch** (0.2.x): bug fixes only; no breaking changes anywhere.
- **Minor** (0.x.0): features; may break **experimental** surfaces (with a
  CHANGELOG entry), never **stable** ones without the deprecation period
  above.
- The compiler requirement (Bloomberg clang-p2996) tracks upstream P2996
  revisions and may bump in minor releases; the Docker image always matches
  the code it ships with.

Tagged releases are the supported way to consume mirror_bridge:
[github.com/FranciscoThiesen/mirror_bridge/releases](https://github.com/FranciscoThiesen/mirror_bridge/releases).
