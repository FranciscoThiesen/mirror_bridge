#!/usr/bin/env python3
"""The compile_commands.json reader, which needs no compiler to test.

Every assertion here is a flag spelling some real build system emits. The MSVC
ones matter most: the case the reader exists for is a large Windows codebase,
whose compile_commands.json carries /I and /D rather than -I and -D.
"""

import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.join(os.path.dirname(HERE), "..", "tools")
sys.path.insert(0, os.path.abspath(TOOLS))

import mirror_bridge_compdb as compdb  # noqa: E402

PASS, FAIL = 0, 0


def check(label, got, want):
    global PASS, FAIL
    if got == want:
        PASS += 1
        print(f"  PASS  {label}")
    else:
        FAIL += 1
        print(f"  FAIL  {label}\n        got  {got!r}\n        want {want!r}")


def entry(command=None, arguments=None, directory="/proj/build", file="/proj/src/a.cpp"):
    e = {"directory": directory, "file": file}
    if arguments is not None:
        e["arguments"] = arguments
    else:
        e["command"] = command
    return e


print("=== harvest: GNU spellings ===")
check("-I joined",
      compdb.harvest(entry("c++ -I/abs/inc -c a.cpp")), ["-I/abs/inc"])
check("-I separated",
      compdb.harvest(entry("c++ -I /abs/inc -c a.cpp")), ["-I/abs/inc"])
check("-I relative resolves against directory",
      compdb.harvest(entry("c++ -I../include -c a.cpp")), ["-I/proj/include"])
check("-D joined",
      compdb.harvest(entry("c++ -DFOO=1 -c a.cpp")), ["-DFOO=1"])
check("-D separated",
      compdb.harvest(entry("c++ -D FOO=1 -c a.cpp")), ["-DFOO=1"])
check("-D bare",
      compdb.harvest(entry("c++ -DNDEBUG -c a.cpp")), ["-DNDEBUG"])
check("-U kept",
      compdb.harvest(entry("c++ -UFOO -c a.cpp")), ["-UFOO"])
check("-isystem",
      compdb.harvest(entry("c++ -isystem /usr/third -c a.cpp")),
      ["-isystem/usr/third"])
check("-iquote and -idirafter",
      compdb.harvest(entry("c++ -iquote /q -idirafter /d -c a.cpp")),
      ["-iquote/q", "-idirafter/d"])
check("-include resolves as a file",
      compdb.harvest(entry("c++ -include ../cfg.h -c a.cpp")),
      ["-include/proj/cfg.h"])

print("\n=== harvest: what must be dropped ===")
check("-std is dropped, the pass sets its own",
      compdb.harvest(entry("c++ -std=c++17 -I/inc -c a.cpp")), ["-I/inc"])
check("optimisation, warnings, sanitisers, LTO all dropped",
      compdb.harvest(entry(
          "c++ -O2 -Wall -Wextra -fsanitize=address -flto -g -MD -MF a.d "
          "-fPIC -pthread -o a.o -c a.cpp")), [])
check("argv[0] is never read as a flag",
      compdb.harvest(entry(arguments=["-Ifoo", "-I/real"])), ["-I/real"])

print("\n=== harvest: MSVC and clang-cl ===")
check("/I joined",
      compdb.harvest(entry("cl.exe /IC:/proj/inc /c a.cpp",
                           directory="C:/proj/build")), ["-IC:/proj/inc"])
check("/I separated",
      compdb.harvest(entry("cl.exe /I C:/proj/inc /c a.cpp",
                           directory="C:/proj/build")), ["-IC:/proj/inc"])
check("/D becomes -D",
      compdb.harvest(entry("cl.exe /DWIN32 /D_DEBUG=1 /c a.cpp")),
      ["-DWIN32", "-D_DEBUG=1"])
check("/U becomes -U",
      compdb.harvest(entry("cl.exe /UFOO /c a.cpp")), ["-UFOO"])
check("/external:I becomes -isystem",
      compdb.harvest(entry("cl.exe /external:I C:/third /c a.cpp")),
      ["-isystemC:/third"])
check("/FI becomes -include",
      compdb.harvest(entry("cl.exe /FI C:/proj/pch.h /c a.cpp")),
      ["-includeC:/proj/pch.h"])
check("-FI is tried before -F so it is not read as a framework path",
      compdb.harvest(entry("clang-cl -FI /proj/force.h -c a.cpp")),
      ["-include/proj/force.h"])
check("-F still works as a framework path",
      compdb.harvest(entry("c++ -F/Frameworks -c a.cpp")), ["-F/Frameworks"])
check("MSVC noise dropped",
      compdb.harvest(entry("cl.exe /nologo /EHsc /MD /std:c++20 /Fo a.obj /c a.cpp")),
      [])

print("\n=== harvest: malformed input costs the entry, not the run ===")
check("unbalanced quote yields nothing",
      compdb.harvest(entry('c++ -I"/unclosed -c a.cpp')), [])
check("trailing flag with no argument is not a crash",
      compdb.harvest(entry(arguments=["c++", "-I"])), [])
check("no command and no arguments",
      compdb.harvest({"directory": "/d", "file": "/d/a.cpp"}), [])
check("arguments array form",
      compdb.harvest(entry(arguments=["c++", "-I", "/abs", "-DX=2", "-c", "a.cpp"])),
      ["-I/abs", "-DX=2"])

print("\n=== collect: union, order, conflicts ===")
flags, rep = compdb.collect([
    entry("c++ -I/first -I/second -DA=1 -c a.cpp"),
    entry("c++ -I/second -I/third -DA=1 -c b.cpp"),
])
check("duplicates collapse", flags, ["-I/first", "-I/second", "-DA=1", "-I/third"])
check("both entries counted", rep["entries_scanned"], 2)
check("both contributed", rep["entries_contributing"], 2)

flags, rep = compdb.collect([
    entry("c++ -DMODE=fast -c a.cpp"),
    entry("c++ -DMODE=slow -c b.cpp"),
])
check("first definition of a macro wins", flags, ["-DMODE=fast"])
check("the conflict is reported", rep["define_conflicts"],
      [{"macro": "MODE", "kept": "fast", "ignored": "slow"}])

flags, rep = compdb.collect([entry(arguments=["c++", "-DMSG=a b", "-I/ok"])])
check("a flag carrying whitespace is refused, not split", flags, ["-I/ok"])
check("and it is reported", rep["dropped_for_whitespace"], ["-DMSG=a b"])

print("\n=== Windows absolute paths, read on a POSIX box ===")
check("a drive letter is absolute, not joined onto the build dir",
      compdb.harvest(entry("cl.exe /IC:/proj/inc /c a.cpp",
                           directory="/linux/build")), ["-IC:/proj/inc"])
check("backslashes survive untouched",
      compdb.harvest(entry(arguments=["cl.exe", "/I", "C:\\proj\\inc"],
                           directory="/linux/build")), ["-IC:\\proj\\inc"])
check("a UNC share is absolute",
      compdb.harvest(entry(arguments=["cl.exe", "/I", "\\\\host\\share\\inc"],
                           directory="/linux/build")),
      ["-I\\\\host\\share\\inc"])
check("a relative path is still joined",
      compdb.harvest(entry(arguments=["cl.exe", "/I", "..\\inc"],
                           directory="/linux/build"))[0].endswith("inc"), True)

print("\n=== --replace-prefix maps one machine onto another ===")
check("a Windows prefix is rewritten",
      compdb.harvest(entry("cl.exe /IC:/proj/inc /c a.cpp"),
                     remap=[("C:/proj", "/mnt/proj")]), ["-I/mnt/proj/inc"])
check("first matching rule wins",
      compdb.harvest(entry("cl.exe /IC:/proj/inc /c a.cpp"),
                     remap=[("C:/proj", "/a"), ("C:/", "/b")]), ["-I/a/inc"])
check("a non-matching rule leaves the path alone",
      compdb.harvest(entry("cl.exe /IC:/other/inc /c a.cpp"),
                     remap=[("C:/proj", "/mnt")]), ["-IC:/other/inc"])
check("defines are not path-remapped",
      compdb.harvest(entry("cl.exe /DROOT=C:/proj /c a.cpp"),
                     remap=[("C:/proj", "/mnt")]), ["-DROOT=C:/proj"])
check("--for matches after the remap, in the local namespace",
      compdb.collect([entry("cl.exe /IC:/proj/inc /c a.cpp",
                            file="C:/proj/core/a.cpp")],
                     under=["/mnt/proj/core"],
                     remap=[("C:/proj", "/mnt/proj")])[0], ["-I/mnt/proj/inc"])

print("\n=== collect: --for narrowing ===")
rows = [
    entry("c++ -I/core -c a.cpp", file="/proj/core/a.cpp"),
    entry("c++ -I/gui -c b.cpp", file="/proj/gui/b.cpp"),
]
flags, rep = compdb.collect(rows, under=["/proj/core"])
check("only the named subtree contributes", flags, ["-I/core"])
check("scanned count reflects the filter", rep["entries_scanned"], 1)
check("total is still reported", rep["entries_total"], 2)
flags, _ = compdb.collect(rows, under=["/proj"])
check("a parent path matches both", flags, ["-I/core", "-I/gui"])
flags, rep = compdb.collect(rows, under=["/proj/nope"])
check("a path matching nothing yields nothing", flags, [])
check("and says so", rep["entries_scanned"], 0)
flags, _ = compdb.collect(rows, under=["/proj/cor"])
check("prefix match does not leak across a path component", flags, [])

win = [entry("cl.exe /IC:/proj/inc /c a.cpp", directory="C:/proj/build",
             file="C:/proj/core/a.cpp")]
flags, _ = compdb.collect(win, under=["C:/proj/core"])
check("a Windows --for path is not prefixed with the cwd", flags, ["-IC:/proj/inc"])

print("\n=== end to end through the CLI entry point ===")
with tempfile.TemporaryDirectory() as td:
    db = os.path.join(td, "compile_commands.json")
    with open(db, "w") as fh:
        json.dump([
            {"directory": td, "file": os.path.join(td, "a.cpp"),
             "command": "c++ -std=c++17 -I../inc -DREAL=1 -O2 -c a.cpp"},
        ], fh)
    r = subprocess.run([sys.executable, os.path.join(TOOLS, "mirror_bridge_compdb.py"), db],
                       capture_output=True, text=True)
    want = [f"-I{os.path.normpath(os.path.join(td, '..', 'inc'))}", "-DREAL=1"]
    check("bare output is one flag per line", r.stdout.split(), want)
    check("exit 0", r.returncode, 0)

    r = subprocess.run([sys.executable, os.path.join(TOOLS, "mirror_bridge_compdb.py"),
                        db, "--json"], capture_output=True, text=True)
    doc = json.loads(r.stdout)
    check("--json carries the flags", doc["flags"], want)
    check("--json carries the report", doc["report"]["entries_total"], 1)

    r = subprocess.run([sys.executable, os.path.join(TOOLS, "mirror_bridge_compdb.py"),
                        os.path.join(td, "nope.json")], capture_output=True, text=True)
    check("a missing file exits 2", r.returncode, 2)

    bad = os.path.join(td, "bad.json")
    open(bad, "w").write("{not json")
    r = subprocess.run([sys.executable, os.path.join(TOOLS, "mirror_bridge_compdb.py"), bad],
                       capture_output=True, text=True)
    check("invalid JSON exits 2", r.returncode, 2)

    obj = os.path.join(td, "obj.json")
    json.dump({"not": "an array"}, open(obj, "w"))
    r = subprocess.run([sys.executable, os.path.join(TOOLS, "mirror_bridge_compdb.py"), obj],
                       capture_output=True, text=True)
    check("a JSON object instead of an array exits 2", r.returncode, 2)

print(f"\n{PASS} passed, {FAIL} failed")
if FAIL:
    print("FAIL: compile_commands reader")
    sys.exit(1)
print("PASS: compile_commands reader")
