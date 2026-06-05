"""mirror-bridge: pip-installable front end for the mirror_bridge binding generator.

C++26 reflection requires Bloomberg's clang-p2996 fork, which is impractical
to install natively today. This wrapper runs the real CLI inside the official
Docker image instead: the user's working directory is mounted at /workspace
and the bundled mirror_bridge runtime (headers + CLI scripts) is mounted
read-only, so generated artifacts land in the caller's directory exactly as
if the CLI ran natively.

When stock compilers ship P2996 reflection this package will switch to native
invocation with no user-facing change.
"""

import os
import shutil
import stat
import subprocess
import sys
from pathlib import Path

DEFAULT_IMAGE = "ghcr.io/franciscothiesen/mirror_bridge:latest"
RUNTIME_MOUNT = "/opt/mirror_bridge_runtime"

# Scripts that must be executable inside the container. Wheel installation
# does not reliably preserve the executable bit, so we restore it on every
# invocation (cheap, idempotent).
EXECUTABLE_SCRIPTS = (
    "tools/mirror_bridge",
    "tools/gen_llms_txt.sh",
    "mirror_bridge_doctor",
    "mirror_bridge_auto",
    "mirror_bridge_build",
    "mirror_bridge_generate",
)


def _runtime_dir() -> Path:
    runtime = Path(__file__).resolve().parent / "_runtime"
    if not (runtime / "tools" / "mirror_bridge").is_file():
        sys.exit(
            "mirror-bridge: bundled runtime is missing or incomplete. "
            "This indicates a broken installation - try reinstalling: "
            "pip install --force-reinstall mirror-bridge"
        )
    return runtime


def _ensure_executable(runtime: Path) -> None:
    for rel in EXECUTABLE_SCRIPTS:
        script = runtime / rel
        if script.is_file():
            try:
                script.chmod(script.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
            except OSError:
                # Read-only install location; the docker mount will still work
                # for the main CLI, which we invoke via bash explicitly.
                pass


def _docker() -> str:
    for candidate in ("docker", "podman"):
        path = shutil.which(candidate)
        if path:
            return path
    sys.exit(
        "mirror-bridge: docker (or podman) is required but was not found on PATH.\n"
        "The C++26 reflection compiler (clang-p2996) only ships in the official\n"
        "container image. Install Docker Desktop, OrbStack, or podman and retry."
    )


def _ensure_image(docker: str, image: str) -> None:
    inspect = subprocess.run(
        [docker, "image", "inspect", image],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if inspect.returncode == 0:
        return
    print(
        f"mirror-bridge: pulling {image}\n"
        "  (one-time download of the clang-p2996 reflection toolchain - several GB)",
        file=sys.stderr,
    )
    pull = subprocess.run([docker, "pull", image])
    if pull.returncode != 0:
        sys.exit(f"mirror-bridge: failed to pull {image}")


def main(argv=None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    image = os.environ.get("MIRROR_BRIDGE_IMAGE", DEFAULT_IMAGE)

    docker = _docker()
    runtime = _runtime_dir()
    _ensure_executable(runtime)
    _ensure_image(docker, image)

    cmd = [
        docker, "run", "--rm",
        "-v", f"{Path.cwd()}:/workspace",
        "-v", f"{runtime}:{RUNTIME_MOUNT}:ro",
        "-w", "/workspace",
    ]
    if sys.stdin.isatty() and sys.stdout.isatty():
        cmd.append("-it")

    # `mirror_bridge shell` is wrapper-specific sugar: drop into bash inside
    # the toolchain container with the runtime mounted.
    if args and args[0] == "shell":
        cmd += [image, "bash"]
    else:
        # Invoke via bash so a lost executable bit can never break the CLI.
        cmd += [image, "bash", f"{RUNTIME_MOUNT}/tools/mirror_bridge", *args]

    try:
        return subprocess.run(cmd).returncode
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())
