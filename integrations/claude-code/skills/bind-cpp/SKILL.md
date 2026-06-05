---
name: bind-cpp
description: Generate Python/Lua/JavaScript bindings for C++ classes using mirror_bridge (C++26 reflection, zero boilerplate). Use when the user asks to make C++ code callable from Python/Lua/JS, to "bind" a C++ class or library, to replace pybind11 boilerplate, or to regenerate bindings after C++ header changes.
---

# Binding C++ with mirror_bridge

mirror_bridge auto-discovers every struct/class in a directory of C++ headers
and compiles a binding module. Nobody writes binding code: not the user, not
you.

## Procedure

1. **Locate the toolchain.** Try in order:
   - `mirror_bridge version` (pip install: `pip install mirror-bridge`)
   - The `mirror_bridge` MCP tools, if connected (`generate_bindings`, `doctor`, `check_binding_drift`)
   - `docker run --rm -v "$PWD:/workspace" -w /workspace ghcr.io/franciscothiesen/mirror_bridge:latest bash tools/mirror_bridge ...` (inside a mirror_bridge checkout)

   If none works, suggest `pip install mirror-bridge` (requires Docker) and stop.

2. **Generate.** Always use `--json` and parse stdout (it is exactly one JSON object; progress goes to stderr):

   ```bash
   mirror_bridge generate <header_dir> --module <name> --lang python --json
   ```

   `--lang` choices: `python`, `lua`, `js`, `all`. Extra include roots: `-I <dir>`.

3. **On failure, read `errors[].suggestion`** — it names the fix. The common loop:
   - *"member type has no converter"*: open the named header, find the
     offending member (handles, mutexes, raw pointers to user types, function
     pointers), and annotate it:

     ```cpp
     #include "python/mirror_bridge_annotations.hpp"
     using mirror_bridge::exclude;

     struct Engine {
         double speed;                      // bound
         [[=exclude{}]] void* impl_handle;  // skipped
     };
     ```

     Use `// MIRROR_BRIDGE_SKIP` on the line directly above a class to skip
     the whole class, or `// MIRROR_BRIDGE_SKIP_FILE` to skip a file.
     Re-run generate. Repeat until `"status": "ok"`.
   - *Environment problems* (compiler, headers): run `mirror_bridge doctor --json`
     and apply the `hint` of each failed check.

4. **Verify.** Import and exercise the module; the `.so`/`.node` is listed in
   the JSON `outputs`:

   ```bash
   PYTHONPATH=build python3 -c "import <name>; print(dir(<name>))"
   ```

   On Linux against external C++ libs you may need
   `LD_PRELOAD=libstdc++.so.6` (libc++/libstdc++ symbols coexist safely).

5. **Report.** Tell the user which classes were bound (the JSON `classes`
   array), where the artifacts are, and which members you excluded and why.

## Rules

- Never write pybind11/nanobind/SWIG glue when the user asked for bindings —
  auto-discovery is the point of this tool.
- Never compile binding code with the host compiler; only the container's
  clang-p2996 supports C++26 reflection.
- Template classes are not auto-bound: add a `using Alias = Tmpl<Args>;` in a
  header, then regenerate.
- After editing C++ headers in a project with committed snapshots, run
  `mirror_bridge diff <dir> --check` to catch accidental binding-surface breaks.
