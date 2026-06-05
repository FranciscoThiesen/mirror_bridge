# Claude Code integration

Two ways to give Claude Code (or any MCP-capable agent) first-class
mirror_bridge support in **your own projects**:

## 1. MCP server (recommended)

```bash
pip install 'mirror-bridge[mcp]'
claude mcp add mirror_bridge -- mirror-bridge-mcp
```

Claude gets three tools: `generate_bindings(src_dir, module, lang, ...)`,
`doctor()`, and `check_binding_drift(src_dir)`. Each returns structured JSON
with actionable suggestions on failure, so the agent self-corrects (e.g.
adding `[[=exclude{}]]` to an unconvertible member and retrying).

## 2. The `bind-cpp` skill

Copy [`skills/bind-cpp/`](skills/bind-cpp/) into your project's
`.claude/skills/` (or `~/.claude/skills/` for all projects):

```bash
mkdir -p .claude/skills
cp -R integrations/claude-code/skills/bind-cpp .claude/skills/
```

It teaches the agent the full workflow: locate the toolchain, generate with
`--json`, fix unconvertible members via annotations, verify the import, and
guard against binding-surface drift with `diff --check`.

Both integrations also work for other agents: the skill file is plain
markdown instructions, and the MCP server speaks the standard protocol
(Cursor, Cline, Zed, etc.).
