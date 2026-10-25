#!/usr/bin/env python3
"""The C# backend: the C ABI it emits, and the C# source that calls it.

There is no C# compiler in the dev container, so the ABI is exercised through
ctypes. That is not a stand-in for a real test: P/Invoke and ctypes call the
same flat C entry points with the same calling convention, so a signature that
works here is the signature the generated DllImport declares. What ctypes
cannot check is whether the .cs compiles, so the C# is checked structurally
instead -- every DllImport target must exist in the shared library -- and the
gap is named here rather than papered over.

Run after: mirror_bridge generate tests/csharp_bindings --module csb --lang csharp
"""

import ctypes
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
CLI = os.path.join(ROOT, "tools", "mirror_bridge")

PASS = FAIL = 0


def _which(name):
    from shutil import which
    return which(name)


# Written here rather than committed beside the fixture: it has to match the
# generated API exactly, so it belongs next to the assertions about that API.
DRIVER_CS = """
using System;
using csb;

class Driver {
    static int Main() {
        using (var d = new Device()) {
            Console.WriteLine($"id={d.id} cal={d.calibration} online={d.online} label={d.label} serial={d.serial}");
            d.id = 21;
            d.calibration = 2.0;
            Console.WriteLine($"doubled()={d.doubled()} scale(4)={d.scale(4.0)} sum(3,4)={d.sum(3, 4)}");
            d.label = "sensor";
            Console.WriteLine($"describe()={d.describe()}");
            d.rename("probe-2");
            Console.WriteLine($"after rename label={d.label}");
            Console.WriteLine($"toggle(false)={d.toggle(false)} online={d.online}");
            try {
                d.fail();
                Console.WriteLine("BUG: fail() did not raise");
                return 1;
            } catch (InvalidOperationException ex) {
                Console.WriteLine($"fail() raised: {ex.Message}");
            }
        }
        using (var c = new Counter()) {
            Console.WriteLine($"bump()={c.bump()} bump()={c.bump()}");
        }
        return 0;
    }
}
"""


def check(label, got, want):
    global PASS, FAIL
    if got == want:
        PASS += 1
        print(f"  PASS  {label}")
    else:
        FAIL += 1
        print(f"  FAIL  {label}\n        got  {got!r}\n        want {want!r}")


def truthy(label, cond):
    check(label, bool(cond), True)


work = tempfile.mkdtemp(prefix="csb_")
print("=== generate ===")
run = subprocess.run(
    [CLI, "generate", HERE, "--module", "csb", "--lang", "csharp", "-o", work],
    capture_output=True, text=True)
if run.returncode != 0:
    print(run.stdout[-3000:])
    print(run.stderr[-2000:])
    print("FAIL: csharp generation did not succeed")
    sys.exit(1)
truthy("generate exits cleanly", run.returncode == 0)

lib_path = os.path.join(work, "libcsb.so")
cs_path = os.path.join(work, "csb.cs")
shim_path = os.path.join(work, "csb_shim.cpp")
truthy("the shared library was built", os.path.isfile(lib_path))
truthy("the C# source was written", os.path.isfile(cs_path))
truthy("the shim source was kept", os.path.isfile(shim_path))

print("\n=== the shim needs no reflection to compile ===")
# The point of emitting text: a consumer rebuilds this with whatever compiler
# they have. Most people who want C# bindings are on MSVC, which has no
# reflection support at all, so a shim that needed it would be unusable.
shim_src = open(shim_path).read()
check("no <meta> include", "<meta>" in shim_src, False)
check("no reflection operator", "^^" in shim_src, False)
check("no splice", "[:" in shim_src, False)
truthy("declares its exports", "MB_EXPORT" in shim_src)

print("\n=== the C ABI, called the way P/Invoke calls it ===")
lib = ctypes.CDLL(lib_path)

VOID = ctypes.c_void_p
I32 = ctypes.c_int32
F64 = ctypes.c_double
STR = ctypes.c_char_p


def bind(name, restype, *argtypes):
    fn = getattr(lib, name)
    fn.restype = restype
    fn.argtypes = list(argtypes)
    return fn


new = bind("mb_Device_new", VOID)
delete = bind("mb_Device_delete", None, VOID)
get_id = bind("mb_Device_get_id", I32, VOID)
set_id = bind("mb_Device_set_id", None, VOID, I32)
get_cal = bind("mb_Device_get_calibration", F64, VOID)
set_cal = bind("mb_Device_set_calibration", None, VOID, F64)
get_online = bind("mb_Device_get_online", I32, VOID)
set_online = bind("mb_Device_set_online", None, VOID, I32)
get_label = bind("mb_Device_get_label", VOID, VOID)
set_label = bind("mb_Device_set_label", None, VOID, STR)
get_serial = bind("mb_Device_get_serial", I32, VOID)
doubled = bind("mb_Device_doubled", I32, VOID)
scale = bind("mb_Device_scale", F64, VOID, F64)
toggle = bind("mb_Device_toggle", I32, VOID, I32)
describe = bind("mb_Device_describe", VOID, VOID)
rename = bind("mb_Device_rename", None, VOID, STR)
msum = bind("mb_Device_sum", I32, VOID, I32, I32)
fail = bind("mb_Device_fail", None, VOID)
last_error = bind("mb_last_error", STR)
clear_error = bind("mb_clear_error", None)
free_string = bind("mb_free_string", None, VOID)


def take(ptr):
    """What Native.TakeString does on the C# side: copy, then free."""
    if not ptr:
        return None
    text = ctypes.cast(ptr, ctypes.c_char_p).value.decode("utf-8")
    free_string(ptr)
    return text


d = new()
truthy("the constructor returns a handle", d)
check("int member reads its C++ initialiser", get_id(d), 7)
check("double member reads its initialiser", get_cal(d), 1.25)
check("bool member crosses as 1", get_online(d), 1)
check("string member round-trips as UTF-8", take(get_label(d)), "probe")
check("const member is readable", get_serial(d), 4242)
truthy("a const member gets no setter",
       not hasattr(lib, "mb_Device_set_serial"))

set_id(d, 21)
check("int member writes through", get_id(d), 21)
check("the method sees the written value", doubled(d), 42)
set_cal(d, 2.0)
check("double member writes through", scale(d, 4.0), 8.0)
set_online(d, 0)
check("bool member writes through as 0", get_online(d), 0)
check("a bool-returning method answers on the wire", toggle(d, 1), 1)
check("and the write landed", get_online(d), 1)
set_label(d, b"probe-2")
check("string member writes through", take(get_label(d)), "probe-2")
check("a string-returning method allocates a copy", take(describe(d)), "probe-2/21")
rename(d, b"sensor")
check("a string parameter reaches C++", take(get_label(d)), "sensor")
check("two scalar parameters arrive in order", msum(d, 3, 4), 7)

print("\n=== a C++ exception does not cross the boundary ===")
clear_error()
check("no error pending to start", last_error(), None)
fail(d)
err = last_error()
truthy("the throw was recorded rather than unwound", err is not None)
check("and carries what() through", b"device is on fire" in (err or b""), True)
clear_error()
check("clearing works", last_error(), None)

# The managed side reads mb_last_error straight after a call and treats anything
# there as that call's failure. So an error left behind by a call nobody checked
# -- a string property accessor has no natural place to raise -- would surface
# as the next method's failure. Every entry point that can record an error
# clears first, which is what makes the reading unambiguous.
fail(d)
truthy("an error is pending after the throw", last_error() is not None)
check("a later successful call clears it rather than inheriting it",
      (msum(d, 1, 1), last_error()), (2, None))
fail(d)
truthy("and a string getter clears it too", (take(get_label(d)), last_error())[1] is None)

delete(d)

c = bind("mb_Counter_new", VOID)()
bump = bind("mb_Counter_bump", I32, VOID)
check("a second class is bound too", bump(c), 1)
check("and keeps its own state", bump(c), 2)
bind("mb_Counter_delete", None, VOID)(c)

print("\n=== the generated C# matches the library ===")
cs = open(cs_path).read()
targets = re.findall(r'EntryPoint = "([^"]+)"', cs)
truthy("it declares entry points at all", len(targets) > 10)
missing = [t for t in sorted(set(targets)) if not hasattr(lib, t)]
check("every DllImport target exists in the shared library", missing, [])

truthy("the namespace is a legal identifier", re.search(r"namespace \w+ \{", cs))
truthy("a class is generated per C++ class",
       "public sealed class Device" in cs and "public sealed class Counter" in cs)
truthy("it is disposable", "public void Dispose()" in cs)
truthy("a string return is marshalled and freed", "Native.TakeString" in cs)
truthy("a string parameter is marshalled in",
       "[MarshalAs(UnmanagedType.LPUTF8Str)] string" in cs)
truthy("a bool is converted at the edge", "!= 0" in cs)
truthy("a const member has a getter and no setter",
       "public int serial {" in cs and "Set_serial" not in cs)
truthy("errors are raised, not swallowed", "Native.Throw(" in cs)
truthy("a string property raises rather than returning null on failure",
       cs.count("Native.Throw(") >= 4)

print("\n=== and says what it left out ===")
# A generator that silently binds a subset is worse than one that refuses:
# the caller cannot tell a missing member from an absent one.
truthy("the skipped member is named in the C#", "samples --" in cs)
truthy("with the type that could not cross", "vector<double" in cs)
truthy("the skipped method too", "history() --" in cs)
check("a skipped member gets no property", "public  samples" in cs, False)

# The run itself must report them, so nobody has to read the generated file.
truthy("the generate run reports what it left out",
       "left out" in run.stdout and "samples" in run.stdout)

print("\n=== C# indentation is produced by one formatter ===")
# Every line in a generated class body sits at a multiple of four, which the
# brace-depth formatter guarantees and forty careful call sites did not.
bad = [ln for ln in cs.splitlines()
       if ln.strip() and (len(ln) - len(ln.lstrip(" "))) % 4 != 0]
check("no line is indented off the grid", bad[:3], [])

print("\n=== the generated C# compiles and runs ===")
# Structural checks pass on source no compiler accepts: the first version of
# this backend emitted `private static extern intGet_id(IntPtr self)`, which
# every check above was happy with. So when a C# toolchain is present the .cs
# is compiled and run for real, and that is the check that matters.
MCS = next((p for p in ("mcs", "csc") if _which(p)), None)
MONO = _which("mono")
if MCS and MONO:
    drive = os.path.join(work, "drive")
    os.makedirs(drive, exist_ok=True)
    for f in ("csb.cs", "libcsb.so"):
        subprocess.run(["cp", os.path.join(work, f), drive], check=True)
    with open(os.path.join(drive, "Driver.cs"), "w") as fh:
        fh.write(DRIVER_CS)
    build = subprocess.run([MCS, "-out:driver.exe", "csb.cs", "Driver.cs"],
                           cwd=drive, capture_output=True, text=True)
    if build.returncode != 0:
        check("the generated C# compiles", build.stdout.strip()[:400]
              + build.stderr.strip()[:400], "")
    else:
        truthy("the generated C# compiles", True)
        env = dict(os.environ, LD_LIBRARY_PATH=drive)
        runp = subprocess.run([MONO, "driver.exe"], cwd=drive, env=env,
                              capture_output=True, text=True)
        out = runp.stdout
        check("it runs to completion", runp.returncode, 0)
        truthy("members read their C++ initialisers",
               "id=7 cal=1.25 online=True label=probe serial=4242" in out)
        truthy("methods see values written from C#",
               "doubled()=42 scale(4)=8 sum(3,4)=7" in out)
        truthy("a string return arrives as a C# string",
               "describe()=sensor/21" in out)
        truthy("a string parameter reaches C++", "after rename label=probe-2" in out)
        truthy("a bool round-trips as bool, not int",
               "toggle(false)=False online=False" in out)
        truthy("a C++ throw becomes a C# exception carrying what()",
               "fail() raised" in out and "device is on fire" in out)
        truthy("each object keeps its own state", "bump()=1 bump()=2" in out)
        if runp.returncode != 0:
            print(out[-1200:]); print(runp.stderr[-600:])
else:
    print("  SKIP  no C# toolchain on PATH (mcs/mono); the .cs was checked")
    print("        structurally only. The dev container installs mono-mcs.")

print(f"\n{PASS} passed, {FAIL} failed")
if FAIL:
    print("FAIL: csharp bindings")
    sys.exit(1)
print("PASS: csharp bindings")
