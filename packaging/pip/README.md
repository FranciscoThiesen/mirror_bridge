# mirror-bridge

Write your C++ once. Get Python, Lua, and JavaScript bindings for free.

`mirror_bridge` is a binding generator built on C++26 reflection (P2996): it
discovers your classes at compile time and generates zero-overhead bindings
with **no hand-written glue code** — no `py::class_`, no `.def()`, nothing.

```bash
pip install mirror-bridge

# Point it at a directory of C++ headers:
mirror_bridge generate src/ --module my_module --lang python

python3 -c "import my_module"   # done
```

## How it works

C++26 reflection currently requires Bloomberg's
[clang-p2996](https://github.com/bloomberg/clang-p2996) fork or stock GCC 16+,
which most systems don't ship yet, so this package runs the toolchain inside
the official Docker image. Your working directory is mounted into the
container; generated `.so`/`.node` files land right where you ran the command.
The first invocation pulls the image (one-time, several GB).

**Requirements:** Python ≥ 3.9 and Docker (or podman).

Already have GCC 16+ or a clang-p2996 build? Skip this wrapper — the repo's
`tools/mirror_bridge` CLI runs natively and auto-detects both compilers. As
reflection support reaches stock toolchains, this package will switch to
native invocation — same commands, no Docker.

## Commands

Everything the [mirror_bridge CLI](https://github.com/FranciscoThiesen/mirror_bridge/blob/main/docs/reference/cli.md)
supports, plus `mirror_bridge shell` to drop into the toolchain container:

```bash
mirror_bridge generate src/ --module m --lang all   # Python + Lua + JS
mirror_bridge doctor --json                         # machine-readable diagnostics
mirror_bridge diff src/ --check                     # CI gate for binding drift
mirror_bridge shell                                 # interactive container
```

## MCP server (for AI agents)

The package ships an [MCP](https://modelcontextprotocol.io) server so AI
coding agents can generate bindings as a native tool call:

```bash
pip install 'mirror-bridge[mcp]'

# Register with Claude Code:
claude mcp add mirror_bridge -- mirror-bridge-mcp
```

Exposed tools: `generate_bindings(src_dir, module, lang, ...)`, `doctor()`,
and `check_binding_drift(src_dir)` — each returns the CLI's structured JSON
result, including actionable suggestions on failure.

## Configuration

| Environment variable | Effect |
|---|---|
| `MIRROR_BRIDGE_IMAGE` | Override the container image (default `ghcr.io/franciscothiesen/mirror_bridge:latest`) |

## Links

- [Repository](https://github.com/FranciscoThiesen/mirror_bridge)
- [Documentation](https://github.com/FranciscoThiesen/mirror_bridge/tree/main/docs)
- [Porting Open3D: 25,262 hand-written binding lines → 71 auto-generated](https://chico.dev/Mirror-Bridge-Open3D-71-Lines/)

Apache-2.0 licensed.
