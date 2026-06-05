"""MCP server exposing mirror_bridge to AI agents.

Run with `mirror-bridge-mcp` (stdio transport). Requires the `mcp` extra:
pip install 'mirror-bridge[mcp]'.

Register with Claude Code:
    claude mcp add mirror_bridge -- mirror-bridge-mcp

Tools mirror the CLI's --json contract: every call returns the parsed JSON
object the CLI emits, with container paths translated back to host paths so
agents can act on the results directly.
"""

import json
import os
import subprocess
from pathlib import Path
from typing import List, Optional

from mcp.server.fastmcp import FastMCP

from . import _docker, _ensure_executable, _ensure_image, _runtime_dir, RUNTIME_MOUNT

DEFAULT_IMAGE = os.environ.get(
    "MIRROR_BRIDGE_IMAGE", "ghcr.io/franciscothiesen/mirror_bridge:latest"
)

mcp = FastMCP("mirror_bridge")


def _container_cli(mounts: List[str], cli_args: List[str]) -> dict:
    """Run the bundled CLI in the toolchain container and parse its --json output."""
    docker = _docker()
    runtime = _runtime_dir()
    _ensure_executable(runtime)
    _ensure_image(docker, DEFAULT_IMAGE)

    cmd = [docker, "run", "--rm", "-v", f"{runtime}:{RUNTIME_MOUNT}:ro"]
    for m in mounts:
        cmd += ["-v", m]
    cmd += [DEFAULT_IMAGE, "bash", f"{RUNTIME_MOUNT}/tools/mirror_bridge", *cli_args]

    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    try:
        return json.loads(proc.stdout)
    except json.JSONDecodeError:
        # The CLI guarantees one JSON object on stdout in --json mode, so a
        # parse failure means something broke before the CLI ran (docker
        # error, missing mount). Surface the raw output for diagnosis.
        return {
            "status": "error",
            "stage": "wrapper",
            "reason": (proc.stderr or proc.stdout or "no output").strip()[-2000:],
            "exit_code": proc.returncode,
        }


@mcp.tool()
def generate_bindings(
    src_dir: str,
    module: str,
    lang: str = "python",
    output_dir: Optional[str] = None,
    include_dirs: Optional[List[str]] = None,
) -> dict:
    """Generate language bindings for every C++ class found in src_dir.

    Scans all .hpp/.h files under src_dir (absolute host path), auto-discovers
    struct/class definitions via C++26 reflection, and compiles a binding
    module. lang is one of: python, lua, js, all. The compiled .so/.node lands
    in output_dir (default: <src_dir's parent>/build). Returns the structured
    result: discovered classes, built outputs (host paths), and per-language
    errors with actionable suggestions on failure.
    """
    src = Path(src_dir).resolve()
    if not src.is_dir():
        return {"status": "error", "stage": "args", "reason": f"src_dir not found: {src}"}
    out = Path(output_dir).resolve() if output_dir else src.parent / "build"
    out.mkdir(parents=True, exist_ok=True)

    mounts = [f"{src}:/workspace/src:ro", f"{out}:/workspace/out"]
    args = ["generate", "/workspace/src", "--module", module, "--lang", lang,
            "-o", "/workspace/out", "--json"]
    for i, inc in enumerate(include_dirs or []):
        inc_path = Path(inc).resolve()
        mounts.append(f"{inc_path}:/workspace/inc{i}:ro")
        args += ["-I", f"/workspace/inc{i}"]

    result = _container_cli(mounts, args)
    if isinstance(result.get("outputs"), list):
        result["outputs"] = [
            o.replace("/workspace/out", str(out), 1) for o in result["outputs"]
        ]
    return result


@mcp.tool()
def doctor() -> dict:
    """Diagnose the mirror_bridge toolchain: compiler + reflection support,
    Python/Lua/Node dev files, library paths, and an end-to-end binding
    compile + import test. Returns pass/warn/fail per check with hints."""
    return _container_cli([], ["doctor", "--json"])


@mcp.tool()
def check_binding_drift(src_dir: str, build_dir: Optional[str] = None) -> dict:
    """Compare the current binding surface of src_dir against the stored
    snapshot (created by `mirror_bridge diff --update`, kept under
    build_dir/.mirror_bridge). Returns changed/added/removed counts. Useful
    as a pre-commit or CI check after editing C++ headers."""
    src = Path(src_dir).resolve()
    if not src.is_dir():
        return {"status": "error", "stage": "args", "reason": f"src_dir not found: {src}"}
    build = Path(build_dir).resolve() if build_dir else src.parent / "build"
    build.mkdir(parents=True, exist_ok=True)
    mounts = [f"{src}:/workspace/src:ro", f"{build}:/workspace/out"]
    return _container_cli(
        mounts, ["diff", "/workspace/src", "-o", "/workspace/out", "--check", "--json"]
    )


def run() -> None:
    mcp.run()


if __name__ == "__main__":
    run()
