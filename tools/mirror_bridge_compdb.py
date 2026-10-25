#!/usr/bin/env python3
"""Read a project's compile_commands.json and emit the flags a reflection pass needs.

Every large library in the compatibility corpus needed hand-written `--exclude`
and `-D` arguments before its headers would parse: opencv needs a generated
opencv_modules.hpp, open3d's core/ needs MKL, eigen needs sixteen exclude
patterns. That is tolerable for a few hundred thousand lines and impossible for
twenty million, because the information is not missing -- it is in the build
system, and nobody was asking.

A compile_commands.json records, per translation unit, the include paths and
defines that unit really compiled with. This reads that file and hands back the
subset a reflection pass can use.

Only flags that affect *how a header parses* are taken, by allowlist rather
than by dropping known-bad ones: an unrecognised flag in a 20M-line project's
build is far more likely to be an optimisation, a sanitiser or a dependency-
generation flag than something the reflection pass needs, and taking it on
faith is how a generator ends up passing -flto to a syntax-only probe.

`-std=` is deliberately dropped. Reflection needs C++26 and the project is
almost certainly on something older; its own standard flag winning would break
the pass outright. That is a decision a reader of the output should be able to
see, so it is reported rather than silent.

Invoked by `mirror_bridge generate --compile-commands`; run with --help for the
flags. Output is one flag per line, ready for a shell array.
"""

import argparse
import json
import os
import re
import shlex
import sys
from collections import OrderedDict

# One ordered table rather than a loop per spelling. Order is load-bearing in
# exactly one place: -FI (clang-cl's forced include) must be tried before -F
# (a macOS framework path), because the first is a prefix of the second. The
# rest are mutually unambiguous and listed by family for reading.
#
# kind:
#   path     the argument is a directory, resolved against entry["directory"]
#   file     the argument is a file, resolved the same way
#   verbatim the argument is taken as written (-D, -U)
#
# MSVC and clang-cl spellings are here because a 20M-line Windows codebase is
# the case this script exists for, and its compile_commands.json -- from
# CMake's Ninja generator with cl.exe, or from clang-cl -- carries these and
# not the GNU ones. /external:I is MSVC's -isystem.
RULES = (
    ("-FI",         "-include",  "file"),
    ("/FI",         "-include",  "file"),
    ("/external:I", "-isystem",  "path"),
    ("/I",          "-I",        "path"),
    ("/D",          "-D",        "verbatim"),
    ("/U",          "-U",        "verbatim"),
    ("-isystem",    "-isystem",  "path"),
    ("-iquote",     "-iquote",   "path"),
    ("-idirafter",  "-idirafter","path"),
    ("-iframework", "-iframework","path"),
    ("-include",    "-include",  "file"),
    ("-I",          "-I",        "path"),
    ("-F",          "-F",        "path"),
    ("-D",          "-D",        "verbatim"),
    ("-U",          "-U",        "verbatim"),
)


# A drive letter (C:\ or C:/) or a UNC share (\\host\share). os.path.isabs
# answers False for both when this runs on POSIX, which is the normal case
# here and not an edge one: MSVC cannot do reflection, so a Windows project's
# compile_commands.json gets read on a Linux box. Treating C:/proj/inc as
# relative silently produced -I/linux/build/C:/proj/inc, an include path that
# cannot match anything and reports no error.
_WIN_ABS = re.compile(r"^(?:[A-Za-z]:[\\/]|\\\\|//[^/])")


def _is_abs(path):
    return os.path.isabs(path) or bool(_WIN_ABS.match(path))


def _norm(directory, path):
    """Resolve a possibly-relative include path against the entry's directory."""
    if _is_abs(path):
        # normpath on a backslash path is a no-op on POSIX, which is what we
        # want: the path is passed through as the build recorded it, and
        # --replace-prefix is how it gets mapped onto this filesystem.
        return os.path.normpath(path)
    return os.path.normpath(os.path.join(directory, path))


def _remap(path, rules):
    """Rewrite a leading path prefix, first matching rule wins.

    A compile_commands.json written on one machine names directories that do
    not exist on the machine reading it -- the Windows-build/Linux-generate
    split above is the common case, and a container mount is the other. Without
    this the flags are faithful and useless.
    """
    for old, new in rules:
        if path.startswith(old):
            return new + path[len(old):]
    return path


def _argv_of(entry):
    if isinstance(entry.get("arguments"), list):
        return [str(a) for a in entry["arguments"]]
    command = entry.get("command")
    if not command:
        return []
    try:
        return shlex.split(command)
    except ValueError:
        # An unbalanced quote in one entry should cost that entry, not the run.
        return []


def harvest(entry, remap=()):
    """The parse-affecting flags of one compile_commands.json entry."""
    directory = entry.get("directory") or os.getcwd()
    argv = _argv_of(entry)
    out = []
    i = 1 if argv else 0   # argv[0] is the compiler
    while i < len(argv):
        a = argv[i]
        for spelling, emit, kind in RULES:
            if a == spelling:
                if i + 1 >= len(argv):
                    i += 1
                    break
                value = argv[i + 1]
                i += 2
            elif a.startswith(spelling) and len(a) > len(spelling):
                value = a[len(spelling):]
                i += 1
            else:
                continue
            out.append(emit + (value if kind == "verbatim"
                               else _remap(_norm(directory, value), remap)))
            break
        else:
            i += 1
    return out


def entry_file(entry):
    directory = entry.get("directory") or ""
    f = entry.get("file") or ""
    if not f:
        return ""
    return _norm(directory, f)


def collect(entries, under=None, remap=()):
    """Union the flags of every entry, first-seen order, with a report.

    Order is first-seen rather than sorted because include paths are ordered:
    the first directory holding a given header name wins, and re-sorting them
    alphabetically would change which header the pass reads.
    """
    flags = OrderedDict()
    defines = {}
    conflicts = []
    dropped_whitespace = []
    scanned = 0
    contributed = 0

    # abspath would prepend the cwd to a Windows-style path, since POSIX does
    # not recognise a drive letter as absolute -- the same trap _is_abs exists
    # for, reached from the other direction.
    roots = [os.path.normpath(p if _is_abs(p) else os.path.abspath(p))
             for p in (under or [])]

    for entry in entries:
        if roots:
            f = _remap(entry_file(entry), remap)
            if not f or not any(
                    f == r or f.startswith(r + os.sep) for r in roots):
                continue
        scanned += 1
        got = harvest(entry, remap)
        if got:
            contributed += 1
        for flag in got:
            # The CLI interpolates these into a flag string that word-splits,
            # so a flag carrying whitespace would silently become two flags
            # and change what the compiler is told. Refusing it loudly is the
            # only safe option; -D with a spaced value is the realistic case.
            if any(c.isspace() for c in flag):
                if flag not in dropped_whitespace:
                    dropped_whitespace.append(flag)
                continue
            if flag.startswith("-D"):
                body = flag[2:]
                name, _, value = body.partition("=")
                if name in defines and defines[name] != value:
                    pair = (name, defines[name], value)
                    if pair not in conflicts:
                        conflicts.append(pair)
                    continue
                defines.setdefault(name, value)
            flags.setdefault(flag, None)

    return list(flags), {
        "entries_total": len(entries),
        "entries_scanned": scanned,
        "entries_contributing": contributed,
        "flags": len(flags),
        "define_conflicts": [
            {"macro": n, "kept": k, "ignored": i} for n, k, i in conflicts],
        "dropped_for_whitespace": dropped_whitespace,
        "note": "-std= is dropped: the reflection pass sets its own standard",
    }


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Emit the parse-affecting flags from a compile_commands.json")
    ap.add_argument("compdb", help="path to compile_commands.json")
    ap.add_argument("--for", dest="under", action="append", metavar="PATH",
                    help="only use entries whose source file is under PATH "
                         "(repeatable). On a large monorepo the union of every "
                         "subsystem's flags is both huge and contradictory; "
                         "narrowing to the subtree being bound is usually right")
    ap.add_argument("--replace-prefix", action="append", default=[],
                    metavar="OLD=NEW",
                    help="rewrite a leading path prefix in every harvested "
                         "path, e.g. --replace-prefix 'C:/proj=/mnt/proj' "
                         "(repeatable, first match wins). A compile DB written "
                         "on another machine names directories this one does "
                         "not have; this is how they are mapped")
    ap.add_argument("--json", action="store_true",
                    help="write a JSON report to stdout instead of bare flags")
    args = ap.parse_args(argv)

    try:
        with open(args.compdb) as fh:
            entries = json.load(fh)
    except FileNotFoundError:
        print(f"compile_commands.json not found: {args.compdb}", file=sys.stderr)
        return 2
    except json.JSONDecodeError as exc:
        print(f"{args.compdb} is not valid JSON: {exc}", file=sys.stderr)
        return 2

    if not isinstance(entries, list):
        print(f"{args.compdb} should hold a JSON array of entries",
              file=sys.stderr)
        return 2

    remap = []
    for rule in args.replace_prefix:
        old, sep, new = rule.partition("=")
        if not sep or not old:
            print(f"--replace-prefix wants OLD=NEW, got {rule!r}", file=sys.stderr)
            return 2
        remap.append((old, new))

    flags, report = collect(entries, args.under, remap)

    if args.json:
        json.dump({"flags": flags, "report": report}, sys.stdout, indent=1)
        sys.stdout.write("\n")
        return 0

    for flag in flags:
        print(flag)

    if report["define_conflicts"]:
        for c in report["define_conflicts"]:
            print(f"warning: {c['macro']} is defined as both "
                  f"'{c['kept']}' and '{c['ignored']}'; kept the first",
                  file=sys.stderr)
    if report["dropped_for_whitespace"]:
        for flag in report["dropped_for_whitespace"]:
            print(f"warning: dropped {flag!r}: a flag containing whitespace "
                  f"cannot be passed through the generate pipeline",
                  file=sys.stderr)
    if report["entries_scanned"] == 0:
        print("warning: no entries matched; --for may not match any source path",
              file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
