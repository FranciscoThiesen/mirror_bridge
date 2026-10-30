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
import atexit
import fnmatch
import json
import os
import re
import shutil
import tempfile
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
    # Only the compiler's own complaint, not the CLI's warning. The CLI prints
    # "'Factory' is the binding name of 2 classes", picks the one in the
    # shallowest namespace and carries on, so that line is advice and not a
    # cause. Matching it made the matrix report jsoncpp as a name collision
    # for a week while the real failure was a conversion further down, and a
    # category here is a claim that we know what the problem is.
    ("duplicate-binding-name",
     r"redefinition of '\w+'",
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
    # A LINK failure, which proves discovery and codegen both worked: the shim
    # calls functions declared in the headers and defined in the library's own
    # sources, so the library is not header-only and its objects have to be
    # linked in. That is `generate --link-args`, and it is the caller's to
    # supply, so it is not a mirror_bridge failure.
    #
    # It has to sit above the generic rule, because clang's "linker command
    # failed with exit code 1" contains the word error and was being published
    # as an unclassified compile failure. For the C# backend it is reachable at
    # BUILD time rather than at import, because --no-undefined refuses to
    # produce a .so that cannot be loaded -- so this is the only place such a
    # library can be recognised.
    ("needs-linking",
     r"undefined reference to|undefined symbol",
     "not header-only: pass the built library to `generate --link-args`",
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
    # Prefer an actual compiler diagnostic over the last line of output. The
    # last line is a good guess for a backend whose failures end the run, but
    # the C# backend prints its machine-readable summary last, so an
    # unclassified C# failure reported the JSON and said nothing about why the
    # shim did not compile -- which made the corpus useless for diagnosing it.
    diag = [l for l in tail
            if re.search(r"error:|error CS[0-9]+|fatal error", l)]
    pick = diag[0] if diag else (tail[-1] if tail else "")
    return ("unknown", "no recognisable error in the output", pick[:220])


# Reflection toolchains, most specific first. MB_CXX wins if it is set, which
# is the same override the CLI honours.
BARE_TOOLCHAINS = [
    ("clang++", ["-std=c++2c", "-freflection", "-freflection-latest", "-stdlib=libc++"]),
    ("g++", ["-std=c++26", "-freflection"]),
]


HEADER_SNIFF = re.compile(
    r"^[ \t]*(#[ \t]*(include|pragma|ifndef|define|if)|namespace[ \t]|template[ \t]*<"
    r"|class[ \t]|struct[ \t]|using[ \t])", re.MULTILINE)


def looks_like_cxx_header(path):
    """Same question the CLI's find_header_files asks of an extensionless file.

    A public header is not always *.h or *.hpp: Eigen's whole interface is 30
    extensionless files and Qt's is the same shape. The CLI learned to see them;
    this probe had not, so it kept including Eigen's non-self-contained
    internals directly and reporting that Eigen does not compile -- which is
    the exact false accusation the probe exists to prevent. The two have to
    agree about what a header is.
    """
    try:
        with open(path, "rb") as f:
            head = f.read(8192)
    except OSError:
        return False
    if b"\0" in head:
        return False
    try:
        text = head.decode("utf-8", "ignore")
    except ValueError:
        return False
    return bool(HEADER_SNIFF.search(text))


def header_is_excluded(rel, patterns):
    """Same semantics as the CLI's --exclude: a pattern with no leading slash
    matches at any depth, the way .gitignore does."""
    for pattern in patterns:
        if not pattern:
            continue
        if fnmatch.fnmatchcase(rel, pattern):
            return True
        if not pattern.startswith("/") and fnmatch.fnmatchcase(rel, "*/" + pattern):
            return True
    return False


def detect_reflection_compiler():
    override = os.environ.get("MB_CXX")
    candidates = BARE_TOOLCHAINS
    if override:
        base = os.path.basename(override)
        flags = next((f for n, f in BARE_TOOLCHAINS if n in base), BARE_TOOLCHAINS[0][1])
        candidates = [(override, flags)] + BARE_TOOLCHAINS
    for binary, flags in candidates:
        if not shutil.which(binary) and not os.path.isfile(binary):
            continue
        probe = "#include <meta>\nint main() { return 0; }\n"
        with tempfile.NamedTemporaryFile("w", suffix=".cpp", delete=False) as f:
            f.write(probe)
            path = f.name
        try:
            r = subprocess.run([binary] + flags + ["-fsyntax-only", path],
                               capture_output=True, text=True, timeout=120)
            if r.returncode == 0:
                return binary, flags
        except (OSError, subprocess.TimeoutExpired):
            pass
        finally:
            os.unlink(path)
    return None, None


def compiles_bare(lib, checkout, headers, work_dir, timeout):
    """Whether the library's own headers compile with no mirror_bridge at all.

    This is the question that decides whose bug a discovery failure is, and
    guessing it wrong sends five libraries into the work queue that do not
    belong there. One translation unit including the library's headers and
    nothing else, same compiler and flags the reflection pass uses:

        spdlog, fmt   fmt's own consteval format strings
        CLI11         std::wstring_convert, which C++26 removed

    None of those is reachable from here. Returns (ok, first error line), or
    (None, reason) when the probe itself could not run -- which must not be
    reported as the library's fault.
    """
    binary, flags = detect_reflection_compiler()
    if not binary:
        return None, "no reflection compiler found for the bare probe"

    tu = os.path.join(work_dir, lib["name"] + "-bare.cpp")
    includes = []
    with open(tu, "w") as f:
        for root, _dirs, files in os.walk(headers):
            for name in sorted(files):
                full = os.path.join(root, name)
                if not name.endswith((".h", ".hpp")):
                    # Extensionless only, and only when the contents say header.
                    if "." in name or not looks_like_cxx_header(full):
                        continue
                rel = os.path.relpath(full, headers)
                # The probe has to skip exactly what `generate --exclude` skips.
                # Including a header the real run never sees makes the library
                # look broken for a file we deliberately do not bind: doctest's
                # `extensions/*` needs an MPI installation, and probing it
                # reported doctest as uncompilable on a machine without MPI.
                if header_is_excluded(rel, lib.get("exclude") or []):
                    continue
                includes.append(rel)
                f.write('#include "%s"\n' % rel)
        f.write("int main() { return 0; }\n")
    if not includes:
        return None, "no headers to probe"

    cmd = [binary] + flags + ["-w", "-fsyntax-only", "-I" + headers]
    if "clang" in os.path.basename(binary):
        cmd.append("-ferror-limit=1")
    for inc in lib.get("include_dirs", []):
        cmd.append("-I" + os.path.normpath(os.path.join(checkout, inc)))
    for macro in lib.get("defines", []):
        cmd.append("-D" + macro)
    cmd.append(tu)
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return None, "the bare probe timed out"
    if "error: no input files" in r.stderr:
        return None, "the bare probe built no translation unit"
    if r.returncode == 0:
        return True, ""
    err = strip_ansi(r.stderr)
    first = next((l.strip() for l in err.splitlines() if "error:" in l), "")
    return False, first[:220]


def classify_exports(wanted, exported):
    """Judge a module that imports by what it actually exposes.

    Building and importing is not the bar. entt produced a module exporting
    `group_descriptor`, `null_t` and `tombstone_t` -- three internal tag types
    -- and nanoflann one exporting a single comparison functor, and both
    counted as successes beside jsoncpp. `must_export` is what the manifest
    says a usable binding has to expose, taken from the library's own README
    rather than from what happens to bind.

    Returns the fields to merge into the result row.
    """
    out = {"must_export_present": [], "must_export_missing": []}
    if not wanted:
        out.update(status="ok", category="ok", reason="", detail="")
        return out
    have = set(exported)
    present = [n for n in wanted if n in have]
    missing = [n for n in wanted if n not in have]
    out["must_export_present"] = present
    out["must_export_missing"] = missing
    if not present:
        out.update(status="token-binding", category="token-binding",
                   reason="the module imports but exposes none of the API a "
                          "caller needs: " + ", ".join(wanted),
                   detail="exports " + ", ".join(exported[:8]))
    elif missing:
        out.update(status="partial-binding", category="partial-binding",
                   reason="the module is missing API a caller needs: "
                          + ", ".join(missing),
                   detail="has " + ", ".join(present))
    else:
        out.update(status="ok", category="ok", reason="", detail="")
    return out


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


# The file whose existence means "a module was built", per backend. Python and
# Lua both emit <module>.so, Node emits <module>.node, and C# emits
# lib<module>.so beside the <module>.cs a dotnet project compiles -- the shared
# library is the right thing to test for, because the .cs is written before the
# shim is compiled and so exists even for a build that failed at the link.
def module_artifact(module, lang):
    if lang == "js":
        return module + ".node"
    if lang == "csharp":
        return "lib" + module + ".so"
    return module + ".so"


def run_one(lib, cli, cache_dir, work_dir, timeout, lang="python"):
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
           "--lang", lang, "--output", out, "--force", "--json"]
    # .pyi stubs are a Python artifact; asking for them under another backend
    # is an argument error rather than a library finding.
    if lang == "python":
        cmd += ["--stubs"]
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

    # Each backend names its artifact differently, and this check used to know
    # only Python's. A C# build writes lib<module>.so, so os.path.isfile of
    # <module>.so was False for EVERY C# library whether or not the build
    # succeeded -- which made "0/32 bind" a statement about this function
    # rather than about the backend. Adding --lang threaded the flag through
    # and left this behind, which is this project's recurring defect: one fact
    # about the build recorded in two places.
    module = os.path.join(out, module_artifact(lib["module"], lang))
    built = os.path.isfile(module)
    row["module_built"] = built
    row["module_bytes"] = os.path.getsize(module) if built else 0
    row["module_artifact"] = os.path.basename(module)
    row["stub_written"] = (lang == "python"
                           and os.path.isfile(os.path.join(out, lib["module"] + ".pyi")))

    if not built:
        category, reason, line = classify_failure(cli_json, combined)
        # A link failure is not a discovery failure, whatever the discovery
        # field says: the shim could not have referenced the library's symbols
        # unless reflection had read its headers. tinyobjloader was published
        # as "reflection could not read these headers" when the truth was that
        # it is not header-only -- a damning cause for an innocent library, and
        # the override is what produced it.
        if row["discovery"] == "text-scan" and category != "needs-linking":
            category = "reflection-tu-failed"
            # Terse on purpose: this is the most common row, the Discovery
            # column already says `text-scan`, and the page explains what that
            # means once instead of sixteen times in a table.
            reason = "reflection could not read these headers"
            # Before blaming the reflection pass, check whether the headers
            # compile at all. Only here, because this is the one category
            # where whose bug it is is in question, and the probe costs a
            # compile.
            bare_ok, bare_err = compiles_bare(lib, checkout, headers, work_dir, timeout)
            if bare_ok is False:
                category = "library-needs-newer-compiler"
                reason = ("these headers do not compile under this reflection "
                          "compiler at all, with no mirror_bridge involved")
                row["bare_error"] = bare_err
            elif bare_ok is None:
                row["bare_probe"] = bare_err
        row.update(status="failed", category=category, reason=reason, detail=line)
        return row

    # Built is not the bar: a module that will not load is not usable. For
    # Python, Lua and Node that means importing it. For C# there is nothing to
    # import -- the artifact is a plain shared library a dotnet project
    # P/Invokes into -- so the equivalent is that it dlopens and that the
    # entry points for the API are really exported.
    if lang == "csharp":
        return finish_csharp(row, lib, out, module)

    # The names come back too, not just the count, because `must_export` below
    # has to be checked against them.
    probe = subprocess.run(
        [sys.executable, "-c",
         "import sys; sys.path.insert(0, sys.argv[1]); "
         "m = __import__(sys.argv[2]); "
         "print('\\n'.join(sorted(n for n in dir(m) if not n.startswith('_'))))",
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

    exported = [n for n in probe.stdout.split("\n") if n.strip()]
    row["exported_names"] = len(exported)
    row["exported_sample"] = exported[:40]

    # A module that imports is not the bar either. entt produced one exporting
    # `group_descriptor`, `null_t` and `tombstone_t` -- three internal tag types
    # -- and nanoflann one exporting a single comparison functor, and both
    # counted as successes beside jsoncpp. `must_export` is what the manifest
    # says a usable binding of this library has to expose, chosen from the
    # library's own documentation rather than from what happens to bind.
    verdict = classify_exports(lib.get("must_export") or [], exported)
    row.update(verdict)
    return row


# The C# half of "is this usable". Two questions, in order: does the library
# load at all, and does the generated API actually name the classes a caller
# needs.
#
# dlopen rather than a symbol listing, because an undefined symbol or a missing
# dependency is exactly the failure --no-undefined exists to catch and a symbol
# table would not see it. Then the class list from the generated .cs, because
# that is the surface a C# caller writes against -- and because a borrow-only
# class deliberately has no mb_<Class>_delete symbol, so the symbol table is no
# longer a complete index of what bound.
def finish_csharp(row, lib, out, module):
    probe = subprocess.run(
        [sys.executable, "-c",
         "import ctypes, sys; ctypes.CDLL(sys.argv[1]); print('ok')", module],
        capture_output=True, text=True, timeout=120)
    if probe.returncode != 0:
        err = strip_ansi(probe.stderr or "").strip()
        last = err.splitlines()[-1][:200] if err else ""
        if "undefined symbol" in err:
            row.update(status="needs-linking", category="needs-linking",
                       reason="not header-only: pass the built library to "
                              "`generate --link-args`",
                       detail=last)
        else:
            row.update(status="imports-failed", category="imports-failed",
                       reason="the library built but will not load",
                       detail=last)
        return row

    cs_path = os.path.join(out, lib["module"] + ".cs")
    try:
        with open(cs_path, encoding="utf-8", errors="replace") as f:
            cs = f.read()
    except OSError as exc:
        row.update(status="imports-failed", category="imports-failed",
                   reason="the library loaded but the generated C# is missing",
                   detail=str(exc)[:200])
        return row

    declared = set(re.findall(r"public sealed class (\w+)", cs))

    # The .cs also declares one <Class>Handle per bound class and the Mb*
    # collection views, so the raw count overstates the API: jsoncpp's 23
    # classes read as 45. Filtered for the published count, UNFILTERED for the
    # verdict, because a library whose own class is called FooHandle or MbFoo
    # must not be judged by a heuristic -- a wrong count is a cosmetic error
    # and a wrong verdict is not.
    def generated(name):
        if name == "MbPin":
            return True
        if name.endswith("Handle") and name[:-len("Handle")] in declared:
            return True
        return len(name) > 2 and name.startswith("Mb") and name[2].isupper()

    api = sorted(n for n in declared if not generated(n))
    row["exported_names"] = len(api)
    row["exported_sample"] = api[:40]
    row.update(classify_exports(lib.get("must_export") or [], sorted(declared)))
    return row


# How good an outcome is, for comparing against the manifest's `expect`. The
# ladder exists because `must_export` RAISED the bar: tinyxml2 binds a module
# and always did, but without XMLElement, and calling that a regression would
# be wrong -- nothing got worse, the measurement got honest. A library may not
# do worse than its recorded expectation; doing better is fine and the run says
# so rather than failing.
OUTCOME_RANK = {
    "ok": 3,
    "partial-binding": 2,
    "token-binding": 1,
}


def outcome_rank(status):
    return OUTCOME_RANK.get(status, 0)


EXPECT_RANK = {
    "bind": 3,
    "partial": 2,
    "token": 1,
    "unknown": 0,
}


def write_results(path, rows):
    payload = {
        "generated_unix": int(time.time()),
        "results": rows,
        "summary": {
            "total": len(rows),
            "ok": sum(1 for r in rows if r["status"] == "ok"),
            "partial": sum(1 for r in rows if r["status"] == "partial-binding"),
            "token": sum(1 for r in rows if r["status"] == "token-binding"),
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
    ap.add_argument("--work-dir", default="/tmp/mb-compat-work",
                    help="scratch for checkouts' build output. Two runs must "
                         "not share one: run_one rmtree's <work-dir>/<library> "
                         "before each build, so concurrent runs delete each "
                         "other's output. A lock makes that refuse rather than "
                         "corrupt.")
    ap.add_argument("--out", default="compatibility/results.json")
    ap.add_argument("--only", action="append", default=[],
                    help="run just this library (repeatable)")
    ap.add_argument("--timeout", type=int, default=600,
                    help="per-library budget in seconds")
    ap.add_argument("--lang", default="python",
                    help="backend to point at the corpus (python, lua, js, "
                         "csharp). Pair a non-default one with its own --out: "
                         "results.json is the Python baseline CI regenerates, "
                         "and a run of another backend is not comparable to it.")
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

    # Two runs sharing a --work-dir corrupt each other, because run_one
    # rmtree's <work-dir>/<library> before each build. The symptom is a library
    # "failing" for a plausible reason: comparing a branch against main, cxxopts
    # came back `failed [reflection-tu-failed] (28 classes)` from one of two
    # concurrent runs and `ok (27 classes)` from the same code alone -- 28 is
    # the text-scan count and 27 the reflection one, so it reads exactly like a
    # discovery regression and cost an hour.
    #
    # O_EXCL rather than a check-then-create, because the race this guards is
    # the whole point. The pid is in the file so a stale lock can be identified
    # rather than merely cursed at.
    lock = os.path.join(args.work_dir, ".run.lock")
    try:
        fd = os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    except FileExistsError:
        try:
            with open(lock) as f:
                held = f.read().strip() or "unknown"
        except OSError:
            held = "unknown"
        sys.exit("another corpus run holds %s (%s). Two runs sharing a "
                 "--work-dir delete each other's builds, so this refuses "
                 "rather than corrupting both. Pass a different --work-dir, or "
                 "remove the lock if that pid is gone." % (lock, held))
    try:
        os.write(fd, ("pid %d\n" % os.getpid()).encode())
    finally:
        os.close(fd)
    atexit.register(lambda: os.path.exists(lock) and os.unlink(lock))

    if args.validate:
        bad = 0
        for lib in corpus:
            checkout, err = fetch(lib, args.cache_dir)
            if err:
                print("%-16s could not clone: %s" % (lib["name"], err.splitlines()[-1][:80]))
                bad += 1
                continue
            missing = []
            # A manifest entry with no must_export silently skips the usability
            # bar, which is exactly the hole this field closes.
            if not lib.get("must_export"):
                missing.append("must_export is empty")
            if not os.path.isdir(os.path.normpath(os.path.join(checkout, lib["headers"]))):
                missing.append("headers=" + lib["headers"])
            build = lib.get("build") or ""
            for inc in lib.get("include_dirs", []):
                if os.path.isdir(os.path.normpath(os.path.join(checkout, inc))):
                    continue
                # A directory the build step creates is not missing, it is not
                # there yet. OpenCV's entry writes the opencv_modules.hpp that
                # CMake would generate into `mb_generated` before anything is
                # compiled, and validating ahead of the build called that a
                # broken manifest.
                if inc in build:
                    continue
                missing.append("include_dirs=" + inc)
            print("%-16s %s" % (lib["name"], "ok" if not missing else "MISSING " + ", ".join(missing)))
            bad += bool(missing)
        return 1 if bad else 0

    os.makedirs(os.path.dirname(os.path.abspath(args.out)) or ".", exist_ok=True)

    rows = []
    for i, lib in enumerate(corpus, 1):
        sys.stderr.write("[%d/%d] %s @ %s ... " % (i, len(corpus), lib["name"], lib["ref"]))
        sys.stderr.flush()
        row = run_one(lib, cli, args.cache_dir, args.work_dir, args.timeout,
                      args.lang)
        rows.append(row)
        sys.stderr.write("%s [%s] (%s classes, %.0fs)\n" % (
            row["status"], row.get("category", "?"),
            row.get("classes_found", "?"), row.get("seconds", 0)))
        sys.stderr.flush()
        # Rewritten after every library: a corpus run takes long enough that a
        # cancelled job should still leave usable data behind.
        write_results(args.out, rows)

    payload = write_results(args.out, rows)
    sys.stderr.write("\n%d/%d bind usefully (%d partial, %d token) -> %s\n" % (
        payload["summary"]["ok"], len(rows), payload["summary"]["partial"],
        payload["summary"]["token"], args.out))

    # The manifest's `expect` is the PYTHON baseline, so comparing another
    # backend against it reports every difference as a regression and exits
    # non-zero on a run that was perfectly informative. A non-default backend
    # therefore reports its categories and stops there -- what a C# run is
    # measured against is a previous C# run, not Python's.
    if args.lang != "python":
        by_cat = payload["summary"]["by_category"]
        for cat in sorted(by_cat):
            sys.stderr.write("  %-26s %d\n" % (cat, by_cat[cat]))
        sys.stderr.write("no expectation comparison: `expect` in the manifest is "
                         "the python baseline\n")
        return 0

    # A library doing WORSE than its recorded expectation is a failure. A
    # library that has never bound is data, not a broken build, and one that
    # does better than expected is news rather than an error.
    regressed, improved = [], []
    for r in rows:
        want = EXPECT_RANK.get(r.get("expect", "unknown"), 0)
        got = outcome_rank(r["status"])
        if got < want:
            regressed.append("%s (expected %s, got %s)" % (r["name"], r.get("expect"), r["status"]))
        elif got > want and want > 0:
            improved.append("%s (expected %s, got %s)" % (r["name"], r.get("expect"), r["status"]))
    if improved:
        sys.stderr.write("IMPROVED: %s\n" % ", ".join(improved))
    if regressed:
        sys.stderr.write("REGRESSED: %s\n" % ", ".join(regressed))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
