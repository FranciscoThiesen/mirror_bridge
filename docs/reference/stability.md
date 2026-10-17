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
| Field annotations | `[[=exclude{}]]`, `[[=readonly{}]]` (P3394), read by the Python backend |
| Discovery opt-outs | `// MIRROR_BRIDGE_SKIP`, `// MIRROR_BRIDGE_SKIP_FILE` |
| CLI commands & flags | `generate`, `build`, `pch`, `diff`, `watch`, `doctor`, `version` and their documented options |
| `--json` output | Existing fields keep their names and types; changes are additive only |
| Type-conversion contract | The mappings in [type-conversion.md](type-conversion.md), including `std::vector<numeric>` returned from a method → `array.array` |
| Umbrella headers | `mirror_bridge.hpp`, `lua/mirror_bridge_lua.hpp`, `javascript/mirror_bridge_javascript.hpp` |
| CMake target | `mirror_bridge::mirror_bridge` via `find_package`/FetchContent |

### Changed in 0.4.0

Reading a `std::vector` **data member** gives a live view of the C++ vector
rather than a copy. A method returning the same type is unchanged. The view is
a `collections.abc.MutableSequence`, so code that iterates, indexes, slices,
takes `len` or compares against a list keeps working; code that asserted
`isinstance(obj.field, list)` or `isinstance(obj.field, array.array)`, or that
passed the member straight to `json.dumps`, needs `list(obj.field)`.

This was a bug fix with a visible type change: the copy meant
`obj.field.append(x)` was accepted and silently discarded.

## Experimental

These may change or be removed in any release; pin a tag if you depend on them:

- The Rust backend (`generate_bindings<T>()` codegen)
- V8 direct-API bindings (`mirror_bridge_v8.hpp`)
- Auto-trampoline internals (the vtable-swap mechanism; the *behavior* —
  Python subclasses overriding C++ virtuals — is intended to stay)
- `--batch` and the `<name>_batch` methods it generates: which signatures
  qualify is expected to widen
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
