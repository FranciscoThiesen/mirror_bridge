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
from typing import Any, List, Optional, Tuple

from mcp.server.fastmcp import FastMCP

from . import _docker, _ensure_executable, _ensure_image, _runtime_dir, RUNTIME_MOUNT

DEFAULT_IMAGE = os.environ.get(
    "MIRROR_BRIDGE_IMAGE", "ghcr.io/franciscothiesen/mirror_bridge:latest"
)

mcp = FastMCP("mirror_bridge")

# Where the CLI's own output lands inside the container. Everything it reports
# back is a path under one of these, and every one of them has to be rewritten
# before an agent sees it.
OUT_MOUNT = "/workspace/out"
SRC_MOUNT = "/workspace/src"


def _translate(value: Any, mapping: List[Tuple[str, str]]) -> Any:
    """Rewrite container paths to host paths, everywhere they appear.

    The CLI reports paths in more places than `outputs`: a failed build now
    names the generated binding it kept, `--stubs` names the .pyi, and the
    planner names its log. Translating only the one field the wrapper knew
    about handed an agent paths that do not exist on its filesystem, which is
    worse than no path at all because it looks actionable.
    """
    if isinstance(value, str):
        for container, host in mapping:
            value = value.replace(container, host)
        return value
    if isinstance(value, list):
        return [_translate(v, mapping) for v in value]
    if isinstance(value, dict):
        return {k: _translate(v, mapping) for k, v in value.items()}
    return value


def _generate_argv(
    module: str,
    lang: str,
    include_dirs: Optional[List[str]],
    batch: bool,
    stubs: bool,
    release_gil: bool,
    keep_generated: bool,
    defines: Optional[List[str]],
    exclude: Optional[List[str]],
    link_args: Optional[str],
) -> Tuple[List[str], List[str]]:
    """Build (extra mounts, cli args) for a generate call.

    Separate from the tool so it can be tested without Docker: this is where
    a flag silently fails to reach the CLI, and there was no test at all.
    """
    mounts: List[str] = []
    args = ["generate", SRC_MOUNT, "--module", module, "--lang", lang,
            "-o", OUT_MOUNT, "--json"]

    for i, inc in enumerate(include_dirs or []):
        inc_path = Path(inc).resolve()
        mounts.append(f"{inc_path}:/workspace/inc{i}:ro")
        args += ["-I", f"/workspace/inc{i}"]

    if batch:
        args.append("--batch")
    if not stubs:
        args.append("--no-stubs")
    if release_gil:
        args.append("--release-gil")
    if keep_generated:
        args.append("-k")
    for d in defines or []:
        args += ["-D", d]
    for pattern in exclude or []:
        args += ["--exclude", pattern]

    if link_args:
        # Link inputs are host paths to built libraries, so each one has to be
        # mounted before the container can see it. Anything that is not an
        # existing file (`-lz`, `-L/usr/lib`) passes through untouched.
        translated = []
        for i, token in enumerate(link_args.split()):
            p = Path(token)
            if p.is_file():
                p = p.resolve()
                mounts.append(f"{p.parent}:/workspace/lib{i}:ro")
                translated.append(f"/workspace/lib{i}/{p.name}")
            else:
                translated.append(token)
        args += ["--link-args", " ".join(translated)]

    return mounts, args


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
        result = json.loads(proc.stdout)
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

    # The CLI writes its human-readable diagnosis to stderr and one grepped
    # line to the JSON, and the agent reading this is the one that cannot see
    # a terminal. On a failure the difference between those two is the whole
    # compiler error.
    if isinstance(result, dict) and result.get("status") != "ok" and proc.stderr.strip():
        result["stderr"] = proc.stderr.strip()[-8000:]
    return result


@mcp.tool()
def generate_bindings(
    src_dir: str,
    module: str,
    lang: str = "python",
    output_dir: Optional[str] = None,
    include_dirs: Optional[List[str]] = None,
    batch: bool = False,
    stubs: bool = True,
    release_gil: bool = False,
    keep_generated: bool = False,
    defines: Optional[List[str]] = None,
    exclude: Optional[List[str]] = None,
    link_args: Optional[str] = None,
) -> dict:
    """Generate language bindings for every C++ class found in src_dir.

    Scans all .hpp/.h files under src_dir (absolute host path), auto-discovers
    struct/class definitions via C++26 reflection, and compiles a binding
    module. lang is one of: python, lua, js, all. The compiled .so/.node lands
    in output_dir (default: <src_dir's parent>/build). Returns the structured
    result: discovered classes, built outputs (host paths), and per-language
    errors with actionable suggestions on failure.

    batch: also generate a `<name>_batch` sibling for each method taking and
      returning one arithmetic value, so a Python loop crosses the boundary
      once instead of once per element. Measured at 6.8x from a list and 12.9x
      from an array.array over 100,000 elements. Costs compile time, so it is
      off unless asked for. Python only.
    stubs: write <module>.pyi beside the module, with reflection-exact
      signatures and real C++ parameter names. On by default; the path comes
      back in `stub` and the text in `stub_text`.
    release_gil: drop the GIL while C++ method bodies run. Worth it only when
      the bodies are long: the save/restore pair costs about 19 ns per call,
      which is several times the whole binding cost of a short one.
    keep_generated: keep the generated binding .cpp even on success. It is
      kept on failure either way, and its path comes back in the error.
    defines: preprocessor macros, as "NAME" or "NAME=VALUE".
    exclude: header path patterns to skip during discovery.
    link_args: extra link inputs for libraries that are not header-only, e.g.
      "/path/to/libFoo.a -lz". Host paths are mounted automatically.
    """
    src = Path(src_dir).resolve()
    if not src.is_dir():
        return {"status": "error", "stage": "args", "reason": f"src_dir not found: {src}"}
    out = Path(output_dir).resolve() if output_dir else src.parent / "build"
    out.mkdir(parents=True, exist_ok=True)

    extra_mounts, args = _generate_argv(
        module, lang, include_dirs, batch, stubs, release_gil,
        keep_generated, defines, exclude, link_args,
    )
    mounts = [f"{src}:{SRC_MOUNT}:ro", f"{out}:{OUT_MOUNT}"] + extra_mounts

    result = _container_cli(mounts, args)
    result = _translate(result, [(OUT_MOUNT, str(out)), (SRC_MOUNT, str(src))])

    # The .pyi is the one artifact whose contents an agent always wants and
    # would otherwise have to go and read. It is small and it is the answer to
    # "what does this module expose".
    if stubs and lang in ("python", "all") and isinstance(result, dict):
        stub = out / f"{module}.pyi"
        if stub.is_file():
            result["stub"] = str(stub)
            try:
                result["stub_text"] = stub.read_text()
            except OSError:
                pass
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
    mounts = [f"{src}:{SRC_MOUNT}:ro", f"{build}:{OUT_MOUNT}"]
    result = _container_cli(
        mounts, ["diff", SRC_MOUNT, "-o", OUT_MOUNT, "--check", "--json"]
    )
    return _translate(result, [(OUT_MOUNT, str(build)), (SRC_MOUNT, str(src))])


def run() -> None:
    mcp.run()


if __name__ == "__main__":
    run()
