#!/usr/bin/env python3
"""The agent-facing interface can reach the features built for agents.

mcp_server.py is the one interface in this project written for an LLM agent,
and it had no tests. `generate_bindings` took five parameters while the CLI
accepts nineteen flags, so `--batch` — the largest shipped speedup for the
acceleration use case, 6.8x from a list and 12.9x from an array.array — was
unreachable from it, and so was the .pyi. The CLI's human-readable diagnosis
goes to stderr and the wrapper discarded it, so on a failed build an agent
got one grepped line and a line number with no file.

These test the two pure parts — argv construction and container-to-host path
translation — which is where a flag silently fails to arrive. Running the
real thing needs Docker and the toolchain image; the harness has neither
guaranteed, so the parts that need them are left to the CLI's own tests.
"""
import os
import re
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "packaging", "pip", "src"))

# The module imports `mcp`, which is an optional extra. Load just the pieces
# under test by executing the file with that import stubbed, rather than
# making the whole test conditional on an extra nobody installs to run tests.
import types


def _load():
    fake_mcp = types.ModuleType("mcp")
    server = types.ModuleType("mcp.server")
    fastmcp = types.ModuleType("mcp.server.fastmcp")

    class FastMCP:
        def __init__(self, *a, **k):
            pass

        def tool(self, *a, **k):
            return lambda fn: fn

        def run(self):
            pass

    fastmcp.FastMCP = FastMCP
    server.fastmcp = fastmcp
    fake_mcp.server = server
    for name, mod in (("mcp", fake_mcp), ("mcp.server", server),
                      ("mcp.server.fastmcp", fastmcp)):
        sys.modules.setdefault(name, mod)
    from mirror_bridge_cli import mcp_server
    return mcp_server


mcp_server = _load()


def argv(**kw):
    defaults = dict(module="m", lang="python", include_dirs=None, batch=False,
                    stubs=True, release_gil=False, keep_generated=False,
                    defines=None, exclude=None, link_args=None)
    defaults.update(kw)
    return mcp_server._generate_argv(**defaults)


class GenerateArgv(unittest.TestCase):
    def test_the_defaults_are_a_working_command(self):
        mounts, args = argv()
        self.assertEqual(mounts, [])
        self.assertEqual(args[:2], ["generate", mcp_server.SRC_MOUNT])
        for expected in ("--module", "m", "--lang", "python", "--json"):
            self.assertIn(expected, args)

    def test_batch_reaches_the_cli(self):
        self.assertIn("--batch", argv(batch=True)[1])
        self.assertNotIn("--batch", argv()[1])

    def test_stubs_are_on_unless_turned_off(self):
        # The CLI writes the .pyi by default, so the wrapper only ever has to
        # say so when the caller does not want one.
        self.assertNotIn("--no-stubs", argv()[1])
        self.assertIn("--no-stubs", argv(stubs=False)[1])

    def test_the_remaining_flags_arrive(self):
        self.assertIn("--release-gil", argv(release_gil=True)[1])
        self.assertIn("-k", argv(keep_generated=True)[1])
        args = argv(defines=["FOO", "BAR=1"])[1]
        self.assertEqual(args[args.index("-D") + 1], "FOO")
        self.assertIn("BAR=1", args)
        self.assertIn("vendor/*", argv(exclude=["vendor/*"])[1])

    def test_include_dirs_are_mounted_and_passed(self):
        with tempfile.TemporaryDirectory() as d:
            mounts, args = argv(include_dirs=[d])
            self.assertTrue(any(m.startswith(os.path.realpath(d)) for m in mounts), mounts)
            self.assertIn("/workspace/inc0", args)

    def test_a_link_path_is_mounted_and_a_link_flag_is_not(self):
        with tempfile.TemporaryDirectory() as d:
            lib = os.path.join(d, "libFoo.a")
            open(lib, "w").close()
            mounts, args = argv(link_args=f"{lib} -lz")
            self.assertTrue(any("/workspace/lib" in m for m in mounts), mounts)
            passed = args[args.index("--link-args") + 1]
            self.assertIn("/workspace/lib", passed)
            self.assertIn("libFoo.a", passed)
            # -lz is a linker flag, not a path, so it must survive untouched.
            self.assertIn("-lz", passed.split())

    def test_every_cli_flag_the_tool_offers_is_one_the_cli_accepts(self):
        # Catches a flag renamed on one side only, which is this project's
        # recurring defect in its most literal form.
        with open(os.path.join(ROOT, "tools", "mirror_bridge")) as f:
            cli = f.read()
        emitted = {a for a in argv(batch=True, stubs=False, release_gil=True,
                                   keep_generated=True, defines=["X"],
                                   exclude=["y"], include_dirs=None)[1]
                   if a.startswith("-")}
        emitted.add("--link-args")
        # A case label is `--batch)` or `-D|--define)`, so the flag has to be
        # followed by `)` or `|`.
        cases = set(re.findall(r"^\s*([-|A-Za-z0-9]+)\)", cli, re.M))
        accepted = set()
        for label in cases:
            accepted.update(label.split("|"))
        missing = sorted(f for f in emitted if f not in accepted)
        self.assertEqual(missing, [],
                         "the MCP server passes flags the CLI's parser has no case "
                         "for: %s" % ", ".join(missing))


class PathTranslation(unittest.TestCase):
    def setUp(self):
        self.mapping = [(mcp_server.OUT_MOUNT, "/host/build"),
                        (mcp_server.SRC_MOUNT, "/host/src")]

    def test_it_reaches_into_nested_structures(self):
        # The old wrapper rewrote exactly one top-level list, so a path inside
        # errors[] came back pointing at a container that no longer exists.
        result = {
            "outputs": [f"{mcp_server.OUT_MOUNT}/m.so"],
            "errors": [{"lang": "python",
                        "generated_binding": f"{mcp_server.OUT_MOUNT}/m_python_binding.cpp",
                        "reason": f"{mcp_server.SRC_MOUNT}/a.hpp:3:1: error: boom"}],
            "classes": ["Thing"],
            "count": 1,
        }
        out = mcp_server._translate(result, self.mapping)
        self.assertEqual(out["outputs"], ["/host/build/m.so"])
        self.assertEqual(out["errors"][0]["generated_binding"],
                         "/host/build/m_python_binding.cpp")
        self.assertEqual(out["errors"][0]["reason"], "/host/src/a.hpp:3:1: error: boom")

    def test_it_leaves_non_strings_alone(self):
        out = mcp_server._translate({"count": 3, "ok": True, "none": None}, self.mapping)
        self.assertEqual(out, {"count": 3, "ok": True, "none": None})


if __name__ == "__main__":
    unittest.main(verbosity=2)
