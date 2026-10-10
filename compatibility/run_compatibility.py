#!/usr/bin/env python3
"""Run `mirror_bridge generate` against a corpus of real C++ libraries.

Auto-discovery is the thing that distinguishes mirror_bridge from pybind11 and
nanobind, and the only honest way to talk about it is to point it at libraries
nobody here wrote and publish what happens. This produces the data; a matrix
with a column saying *why* each failure happens is worth more than a pass rate,
because the reason is the work queue.

Writes one JSON object to --out. compatibility/format_matrix.py renders it.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time

# Each pattern maps compiler or CLI output onto a category a maintainer can act
# on. Order matters: the first match wins, so the specific ones come first and
# `compile-error` is the admission that something is not yet classified.
#
# A category here is a claim that we know what the problem is. Anything landing
# in `compile-error` is a gap in this table, not a gap in the library.
FAILURE_SIGNATURES = [
    # Discovery taking a macro for the class name is the #23 failure. Matched
    # case-sensitively on purpose: with IGNORECASE this also fired for real
    # duplicated class names like jsoncpp's two nested `Factory` classes,
    # which is a different problem with a different fix.
    ("export-macro-discovery",
     r"'[A-Z][A-Z0-9_]{3,}' is the binding name of",
     "discovery took an export macro for the class name (regression of #23)",
     0),
    ("unsupported-container",
     r"Unsupported container shape|must support indexing, push_back, or insert",
     "a member's container type has no insertion API the conversion layer can use",
     re.I),
    ("inaccessible-destructor",
     r"(destructor|~\w+)[^\n]*(is (private|protected)|deleted)|calling a (private|protected) destructor",
     "a bound class cannot be destroyed from the binding",
     re.I),
    ("duplicate-binding-name",
     r"binding name of \d+ classes|redefinition of '\w+'",
     "two classes share an unqualified name, so the module cannot name both",
     re.I),
    # The template planner names a type it is not allowed to name. tinyxml2's
    # MemPoolT<N>::Block is the case; it needs an access check.
    ("private-nested-type",
     r"'\w+' is a private member of",
     "the generated binding names a private nested type",
     0),
    # The compiler itself fell over. Not a binding problem and not fixable
    # here; it wants reporting upstream. yaml-cpp does this once the roots and
    # the unnamed-template guard are right.
    ("compiler-crash",
     r"clang frontend command failed due to signal|internal compiler error|Segmentation fault",
     "the compiler crashed on these headers",
     re.I),
    ("no-converter-for-parameter",
     r"no matching function for call to 'from_python'",
     "a method parameter has no conversion from Python",
     re.I),
    ("incomplete-type",
     r"incomplete type|has incomplete type|invalid application of 'sizeof'",
     "a member or parameter type is only forward-declared",
     re.I),
    ("constexpr-limit",
     r"constexpr evaluation hit maximum step limit|constexpr-steps",
     "reflection over this header exceeds the constexpr step budget",
     re.I),
    ("header-needs-flags",
     r"file not found|No such file or directory",
     "the headers need an include path or a dependency the manifest does not give",
     re.I),
    ("template-depth",
     r"template instantiation depth|recursive template instantiation",
     "template instantiation runs away during binding",
     re.I),
    # The admission that something is not yet classified. The pattern is the
    # compiler's own marker rather than "any line", because matching the first
    # line of output just reported the CLI's banner as the cause.
    ("compile-error", r"\berror\b", "unclassified compile failure", re.I),
]


ANSI = re.compile(r"\x1b\[[0-9;]*m")


def strip_ansi(text):
    return ANSI.sub("", text or "")


def classify_failure(cli_json, stderr):
    """Return (category, human reason, representative line)."""
    blob = strip_ansi(stderr)
    if cli_json:
        blob += "\n" + json.dumps(cli_json)

    if cli_json and cli_json.get("status") == "error":
        stage = cli_json.get("stage", "")
        reason = cli_json.get("reason", "")
        # Discovery naming nothing is its own answer, not a compile failure.
        if stage == "discover" and "No classes" in reason:
            return ("no-classes-found", reason, "")

    for category, pattern, reason, flags in FAILURE_SIGNATURES:
        if not re.search(pattern, blob, flags):
            continue
        line = ""
        for candidate in blob.splitlines():
            if re.search(pattern, candidate, flags):
                line = candidate.strip()[:220]
                break
        return (category, reason, line)

    tail = [l.strip() for l in blob.splitlines() if l.strip()]
    return ("unknown", "no recognisable error in the output",
            tail[-1][:220] if tail else "")


def fetch(lib, cache_dir):
    """Shallow-clone at the pinned ref. Returns (path, error)."""
    dest = os.path.join(cache_dir, lib["name"])
    stamp = os.path.join(dest, ".mb_compat_ref")
    if os.path.isdir(dest):
        try:
            with open(stamp) as f:
                if f.read().strip() == lib["ref"]:
                    return dest, None
        except IOError:
            pass
        shutil.rmtree(dest, ignore_errors=True)

    cmd = ["git", "clone", "--depth", "1", "--recurse-submodules", "--shallow-submodules",
           "--branch", lib["ref"], lib["repo"], dest]
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    if proc.returncode != 0:
        return None, (proc.stderr or proc.stdout).strip()[-300:]
    with open(stamp, "w") as f:
        f.write(lib["ref"])
    return dest, None


def build_library(lib, checkout, build_dir, timeout):
    """Run a manifest `build` command for a library that is not header-only.

    The command is repository-controlled, not user input, so a shell is fine
    and globs are useful. @CHECKOUT@ and @BUILD@ are substituted.

    It has to use the same standard library the module is compiled with. The
    first attempt here built Clipper2 against libstdc++ while the module links
    libc++, and the module then failed to import on
    `std::ios_base::Init::~Init()` -- a confusing symbol for what is really a
    mismatched -stdlib.
    """
    cmd = lib["build"].replace("@CHECKOUT@", checkout).replace("@BUILD@", build_dir)
    os.makedirs(build_dir, exist_ok=True)
    proc = subprocess.run(cmd, shell=True, capture_output=True, text=True,
                          timeout=timeout, cwd=build_dir)
    if proc.returncode != 0:
        return (strip_ansi(proc.stderr) or strip_ansi(proc.stdout)).strip()[-300:]
    return None


def run_one(lib, cli, cache_dir, work_dir, timeout):
    row = {
        "name": lib["name"], "ref": lib["ref"], "shape": lib.get("shape", "oo"),
        "expect": lib.get("expect", "unknown"), "repo": lib["repo"],
    }

    checkout, err = fetch(lib, cache_dir)
    if err:
        row.update(status="fetch-failed", category="fetch-failed",
                   reason="could not clone at the pinned ref", detail=err, seconds=0.0)
        return row

    headers = os.path.normpath(os.path.join(checkout, lib["headers"]))
    if not os.path.isdir(headers):
        row.update(status="fetch-failed", category="manifest-wrong",
                   reason="the manifest's headers path does not exist in the checkout",
                   detail=lib["headers"], seconds=0.0)
        return row

    out = os.path.join(work_dir, lib["name"])
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out, exist_ok=True)

    build_dir = os.path.join(work_dir, lib["name"] + "-lib")
    if lib.get("build"):
        err = build_library(lib, checkout, build_dir, timeout)
        if err:
            row.update(status="failed", category="library-build-failed",
                       reason="the library's own build step failed, so there is "
                              "nothing to link against",
                       detail=err, seconds=0.0)
            return row

    cmd = [cli, "generate", headers, "--module", lib["module"],
           "--lang", "python", "--output", out, "--force", "--json", "--stubs"]
    for inc in lib.get("include_dirs", []):
        cmd += ["-I", os.path.normpath(os.path.join(checkout, inc))]
    # Only set for libraries that are not header-only and that the manifest
    # knows how to point at; left out, such a library reports needs-linking,
    # which is the accurate answer rather than a failure.
    # Third-party headers cannot be annotated, so a subtree you do not want
    # bound is excluded from the command line. pmp-library's OpenGL viewer is
    # the case that made this necessary: nobody wants it callable from Python
    # and it needs GL libraries at link time.
    for pattern in lib.get("exclude", []):
        cmd += ["--exclude", pattern]
    # A library configured by macros. glm will not parse without
    # GLM_FORCE_ALIGNED_GENTYPES and GLM_ENABLE_EXPERIMENTAL, which is the
    # library's own requirement, not a mirror_bridge limitation.
    for macro in lib.get("defines", []):
        cmd += ["-D", macro]
    if lib.get("link_args"):
        cmd += ["--link-args", lib["link_args"]
                .replace("@CHECKOUT@", checkout).replace("@BUILD@", build_dir)]

    started = time.time()
    try:
        # cwd is deliberately not the checkout: the CLI puts $PROJECT_ROOT
        # first on the include path, and running from a library's own tree is
        # how a stray mirror_bridge.hpp would shadow it.
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              timeout=timeout, cwd=work_dir)
        elapsed = time.time() - started
    except subprocess.TimeoutExpired:
        row.update(status="timeout", category="timeout",
                   reason="generate did not finish within the per-library budget",
                   detail="%ds" % timeout, seconds=float(timeout))
        return row

    row["seconds"] = round(elapsed, 1)

    cli_json = None
    for line in reversed(proc.stdout.splitlines()):
        line = line.strip()
        if line.startswith("{"):
            try:
                cli_json = json.loads(line)
                break
            except ValueError:
                continue
    if cli_json is None and proc.stdout.strip().startswith("{"):
        try:
            cli_json = json.loads(proc.stdout)
        except ValueError:
            cli_json = None

    combined = strip_ansi(proc.stdout) + strip_ansi(proc.stderr)
    if "(text scan)" in combined:
        row["discovery"] = "text-scan"
    elif "(reflection)" in combined:
        row["discovery"] = "reflection"
    else:
        row["discovery"] = "unknown"

    classes = (cli_json or {}).get("classes", []) or []
    row["classes_found"] = len(classes)
    row["class_names"] = sorted({c.get("name", "") for c in classes})[:12]

    module = os.path.join(out, lib["module"] + ".so")
    built = os.path.isfile(module)
    row["module_built"] = built
    row["module_bytes"] = os.path.getsize(module) if built else 0
    row["stub_written"] = os.path.isfile(os.path.join(out, lib["module"] + ".pyi"))

    if not built:
        category, reason, line = classify_failure(cli_json, combined)
        if row["discovery"] == "text-scan":
            category = "reflection-tu-failed"
            # Terse on purpose: this is the most common row, the Discovery
            # column already says `text-scan`, and the page explains what that
            # means once instead of sixteen times in a table.
            reason = "reflection could not read these headers"
        row.update(status="failed", category=category, reason=reason, detail=line)
        return row

    # Built is not the bar: a module that will not import is not usable.
    probe = subprocess.run(
        [sys.executable, "-c",
         "import sys; sys.path.insert(0, sys.argv[1]); "
         "m = __import__(sys.argv[2]); "
         "print(len([n for n in dir(m) if not n.startswith('_')]))",
         out, lib["module"]],
        capture_output=True, text=True, timeout=120)
    if probe.returncode != 0:
        err = strip_ansi(probe.stderr or "").strip()
        last = err.splitlines()[-1][:200] if err else ""
        if "undefined symbol" in err:
            # Discovery and codegen both worked; the library simply is not
            # header-only, so its own object code has to be linked in. That is
            # what `generate --link-args` is for, and it is the caller's to
            # supply, so it is not a mirror_bridge failure.
            row.update(status="needs-linking", category="needs-linking",
                       reason="not header-only: pass the built library to "
                              "`generate --link-args`",
                       detail=last)
        else:
            row.update(status="imports-failed", category="imports-failed",
                       reason="the module built but will not import",
                       detail=last)
        return row

    row.update(status="ok", category="ok", reason="", detail="",
               exported_names=int(probe.stdout.strip() or 0))
    return row


def write_results(path, rows):
    payload = {
        "generated_unix": int(time.time()),
        "results": rows,
        "summary": {
            "total": len(rows),
            "ok": sum(1 for r in rows if r["status"] == "ok"),
            "by_category": {},
        },
    }
    for r in rows:
        c = r.get("category", "unknown")
        payload["summary"]["by_category"][c] = payload["summary"]["by_category"].get(c, 0) + 1
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(payload, f, indent=2, sort_keys=True)
        f.write("\n")
    os.replace(tmp, path)
    return payload


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--manifest", default=os.path.join(os.path.dirname(__file__), "libraries.json"))
    ap.add_argument("--cli", default=None, help="path to tools/mirror_bridge")
    ap.add_argument("--cache-dir", default="/tmp/mb-compat-cache")
    ap.add_argument("--work-dir", default="/tmp/mb-compat-work")
    ap.add_argument("--out", default="compatibility/results.json")
    ap.add_argument("--only", action="append", default=[],
                    help="run just this library (repeatable)")
    ap.add_argument("--timeout", type=int, default=600,
                    help="per-library budget in seconds")
    ap.add_argument("--validate", action="store_true",
                    help="check every manifest path against its checkout and exit; "
                         "a wrong path otherwise shows up as a library failure")
    args = ap.parse_args()

    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    cli = args.cli or os.path.join(repo_root, "tools", "mirror_bridge")

    with open(args.manifest) as f:
        corpus = json.load(f)["corpus"]
    if args.only:
        wanted = set(args.only)
        corpus = [c for c in corpus if c["name"] in wanted]
        missing = wanted - {c["name"] for c in corpus}
        if missing:
            sys.exit("not in the manifest: %s" % ", ".join(sorted(missing)))

    os.makedirs(args.cache_dir, exist_ok=True)
    os.makedirs(args.work_dir, exist_ok=True)

    if args.validate:
        bad = 0
        for lib in corpus:
            checkout, err = fetch(lib, args.cache_dir)
            if err:
                print("%-16s could not clone: %s" % (lib["name"], err.splitlines()[-1][:80]))
                bad += 1
                continue
            missing = []
            if not os.path.isdir(os.path.normpath(os.path.join(checkout, lib["headers"]))):
                missing.append("headers=" + lib["headers"])
            for inc in lib.get("include_dirs", []):
                if not os.path.isdir(os.path.normpath(os.path.join(checkout, inc))):
                    missing.append("include_dirs=" + inc)
            print("%-16s %s" % (lib["name"], "ok" if not missing else "MISSING " + ", ".join(missing)))
            bad += bool(missing)
        return 1 if bad else 0

    os.makedirs(os.path.dirname(os.path.abspath(args.out)) or ".", exist_ok=True)

    rows = []
    for i, lib in enumerate(corpus, 1):
        sys.stderr.write("[%d/%d] %s @ %s ... " % (i, len(corpus), lib["name"], lib["ref"]))
        sys.stderr.flush()
        row = run_one(lib, cli, args.cache_dir, args.work_dir, args.timeout)
        rows.append(row)
        sys.stderr.write("%s [%s] (%s classes, %.0fs)\n" % (
            row["status"], row.get("category", "?"),
            row.get("classes_found", "?"), row.get("seconds", 0)))
        sys.stderr.flush()
        # Rewritten after every library: a corpus run takes long enough that a
        # cancelled job should still leave usable data behind.
        write_results(args.out, rows)

    payload = write_results(args.out, rows)
    sys.stderr.write("\n%d/%d bind end to end -> %s\n" % (
        payload["summary"]["ok"], len(rows), args.out))

    # Only an expected-to-bind library regressing is a failure. A library that
    # has never bound is data, not a broken build.
    regressed = [r["name"] for r in rows if r.get("expect") == "bind" and r["status"] != "ok"]
    if regressed:
        sys.stderr.write("REGRESSED (expected to bind): %s\n" % ", ".join(regressed))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
