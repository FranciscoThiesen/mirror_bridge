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
    static void Expect<T>(Action a, string label) where T : Exception {
        try { a(); Console.WriteLine($"BUG {label}: nothing thrown"); }
        catch (T ex) { Console.WriteLine($"{label} -> {ex.GetType().Name}"); }
        catch (Exception ex) { Console.WriteLine($"BUG {label}: got {ex.GetType().Name}"); }
    }

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
        using (var d = new Device()) {
            d.id = 5;
            d.calibration = 2.5;
            Console.WriteLine($"mode={d.mode} promote(None)={d.promote(Mode.None)}");
            d.mode = Mode.Careful;
            Console.WriteLine($"mode set={d.mode} asint={(int)d.mode}");
            Console.WriteLine($"overloads scaled(3)={d.scaled(3)} scaled(3.0)={d.scaled(3.0)}");
            Console.WriteLine($"statics version()={Device.version()} describe={Device.describe_mode(Mode.Careful)}");
        }
        using (var c = new Counter(10)) Console.WriteLine($"ctor1 value={c.value}");
        using (var c = new Counter(10, 3)) Console.WriteLine($"ctor2 value={c.value}");
        using (var dd = new Derived()) {
            dd.base_id = 20;
            Console.WriteLine($"inherit base_id={dd.base_id} tag={dd.tag} base_method()={dd.base_method()} combined()={dd.combined()}");
        }
        using (var cl = new Clashing()) {
            Console.WriteLine($"renamed Handle_={cl.Handle_} Lib_={cl.Lib_} Dispose_()={cl.Dispose_()} New_()={cl.New_()}");
        }
        using (var e = new Device()) {
            Expect<InvalidOperationException>(() => e.fail(), "runtime_error");
            Expect<ArgumentException>(() => e.reject(), "invalid_argument");
            Expect<ArgumentOutOfRangeException>(() => e.overrun(), "out_of_range");
            Expect<OverflowException>(() => e.too_big(), "overflow_error");
        }
        var gone = new Device();
        gone.Dispose();
        try { var _ = gone.id; Console.WriteLine("BUG: read after dispose"); return 1; }
        catch (ObjectDisposedException) { Console.WriteLine("disposed read raises"); }
        gone.Dispose();
        Console.WriteLine("double dispose is a no-op");
        using (var v = new Device()) {
            v.samples.Add(1.5);
            v.samples.Add(2.5);
            v.samples[0] = 9.0;
            Console.WriteLine($"vector member: Count={v.samples.Count} [0]={v.samples[0]} sum={System.Linq.Enumerable.Sum(v.samples)}");
            using (var h = v.history())
                Console.WriteLine($"vector return: Count={h.Count} [0]={h[0]}");
            v.names.Add("alpha");
            v.names[0] = "gamma";
            Console.WriteLine($"string vector: [0]={v.names[0]} Count={v.names.Count}");
            try { var _ = v.samples[99]; Console.WriteLine("BUG: no range check"); }
            catch (ArgumentOutOfRangeException) { Console.WriteLine("vector range check raises"); }
        }
        var dead = new Outer();
        var deadView = dead.inner;
        var deadDev = new Device();
        deadDev.samples.Add(1.0);
        var deadVec = deadDev.samples;
        dead.Dispose();
        deadDev.Dispose();
        try { var _ = deadView.x; Console.WriteLine("BUG: read a freed subobject"); }
        catch (ObjectDisposedException) { Console.WriteLine("disposed owner: view raises"); }
        try { var _ = deadVec.Count; Console.WriteLine("BUG: read a freed vector"); }
        catch (ObjectDisposedException) { Console.WriteLine("disposed owner: vector raises"); }
        using (var o = new Outer()) {
            o.inner.x = 7.5;
            Console.WriteLine($"nested view write reaches the owner: inner.x={o.inner.x} scaled(2)={o.inner.scaled(2.0)}");
            using (var made = o.make())
                Console.WriteLine($"owned return: x={made.x} n={made.n}");
            using (var arg = new Inner()) {
                arg.x = 100.0;
                Console.WriteLine($"class parameter: consume={o.consume(arg)}");
            }
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
truthy("the skipped member is named in the C#", "flags --" in cs)
truthy("with the type that could not cross", "vector<bool" in cs)
truthy("the skipped method too", "widen() --" in cs)
check("a skipped member gets no property", "public  flags" in cs, False)

# The run itself must report them, so nobody has to read the generated file.
truthy("the generate run reports what it left out",
       "left out" in run.stdout and "flags" in run.stdout)
# And the honest inverse: what used to be skipped is now bound, so a stale
# claim here would be the test lying about its own subject.
check("samples is no longer in the skipped list", "samples --" in cs, False)
check("history() is no longer in the skipped list", "history() --" in cs, False)

print("\n=== C# indentation is produced by one formatter ===")
# Every line in a generated class body sits at a multiple of four, which the
# brace-depth formatter guarantees and forty careful call sites did not.
bad = [ln for ln in cs.splitlines()
       if ln.strip() and (len(ln) - len(ln.lstrip(" "))) % 4 != 0]
check("no line is indented off the grid", bad[:3], [])

print("\n=== enums cross as a real C# enum, not a bare integer ===")
truthy("the enum is declared", "public enum Mode : int {" in cs)
truthy("every enumerator is carried", all(k in cs for k in ("None = 0", "Fast = 1", "Careful = 7")))
# None is a keyword in Python and not in C#. The Python backend has to rename it;
# this one keeps the C++ spelling, which is the reason the two are not shared code.
check("a C# non-keyword is not escaped", "@None" in cs, False)
truthy("an enum member is typed", "public Mode mode {" in cs)
truthy("an enum return is typed", "public Mode promote(Mode m)" in cs)
truthy("and is cast back on the way out", "return (Mode)r;" in cs)

print("\n=== overloads bind, because C# overloads natively ===")
truthy("both arities of an overload set are present",
       "public int scaled(int factor)" in cs and "public double scaled(double factor)" in cs)
truthy("the second overload gets its own exported symbol",
       'EntryPoint = "mb_Device_scaled_1"' in cs)
# C++ tells long and long long apart; C# calls both `long`. Emitting both would
# be a duplicate member, so the second is refused by name.
truthy("an overload C# cannot tell apart is refused and says why",
       "widen() --" in cs and "C# does not" in cs)

print("\n=== static methods and parameterized constructors ===")
truthy("a static method is static in C#", "public static int version()" in cs)
truthy("a static taking an enum is bound", "public static string describe_mode(Mode m)" in cs)
truthy("its symbol carries the _s_ infix so it cannot collide with an instance method",
       'EntryPoint = "mb_Device_s_version"' in cs)
truthy("a one-argument constructor is bound", "public Counter(int start)" in cs)
truthy("a two-argument constructor is bound", "public Counter(int start, int bumps)" in cs)
# Binding a copy constructor would make `new Foo(other)` silently duplicate a
# C++ object, and C# has no spelling that would mark it as a copy.
check("the copy constructor is not bound", "public Counter(Counter" in cs, False)

print("\n=== inherited members and methods ===")
# core's member-function cache already walked bases; data members did not until
# this backend needed them, so both halves are checked.
derived_body = cs.split("public sealed class Derived :")[1].split("\n}\n")[0]
truthy("an inherited data member appears on the derived class",
       "public int base_id {" in derived_body)
truthy("an inherited method appears on the derived class",
       "public int base_method()" in derived_body)
truthy("the derived class keeps its own members too",
       "public double extra {" in derived_body)
# Pinning the split itself, because the first version matched
# "class DerivedHandle : SafeHandle" and read the nested handle type instead.
truthy("the split found the class body and not the nested handle",
       "SafeHandle" not in derived_body.split("public double extra")[0]
       or "public int base_id" in derived_body)

print("\n=== the handle is a SafeHandle, not a raw IntPtr ===")
# Not tidiness. With an IntPtr field the last read of `this` is inside the
# argument list, so the JIT may report the object dead and the finalizer may
# run Delete while the native call is still on the stack. A SafeHandle is
# ref-counted across the call, which closes that window; it also makes Dispose
# idempotent and thread-safe and gives the handle a critical finalizer.
truthy("a SafeHandle subclass is generated per class", "DeviceHandle : SafeHandle" in cs)
truthy("it releases through the native delete", "protected override bool ReleaseHandle()" in cs)
truthy("instance calls take the handle, so the runtime keeps it alive",
       "Get_id(DeviceHandle self)" in cs)
check("no hand-rolled finalizer is left behind", "~Device()" in cs, False)
check("the raw pointer is no longer handed out", "public IntPtr Handle =>" in cs, False)
truthy("Dispose delegates to the handle", "_h.Dispose();" in cs)

print("\n=== a C++ name that collides with the generated surface ===")
# `int Handle` and a method called `Dispose()` are house style at several large
# shops. Before this, such a class produced C# that would not compile.
truthy("the colliding member is renamed", "public int Handle_ {" in cs)
truthy("so is a colliding method", "public int Dispose_()" in cs)
truthy("and the rename is written where the reader is",
       "Renamed, because the C++ name is taken" in cs and "Handle -> Handle_" in cs)
truthy("the run reports it too", "renamed to Handle_" in run.stdout)

print("\n=== parameter names are the real C++ ones ===")
# The project's Python stubs carry the real names and say so in writing; this
# was the one backend whose output a human reads directly and it said a0, a1.
for sig in ("scale(double factor)", "toggle(bool on)", "rename(string next)",
            "sum(int a, int b)", "promote(Mode m)"):
    truthy(f"signature reads {sig}", "public " in cs and sig in cs)
check("no positional placeholders remain in the public surface",
      "(double a0)" in cs or "(int a0, int a1)" in cs, False)

print("\n=== every entry point catches, and says which exception it was ===")
truthy("the shim classifies with a dynamic_cast ladder", "mb_set_exception" in shim_src)
truthy("the kind crosses the boundary", "mb_last_error_kind" in shim_src)
truthy("delete catches too, against the shim's own invariant",
       "try { delete static_cast<" in shim_src)
truthy("a failed string allocation is an error, not a silent empty string",
       "out of memory copying a string" in shim_src)
truthy("the managed side maps the kind to a catchable type",
       "ArgumentOutOfRangeException" in cs and "OverflowException" in cs)
truthy("ExactSpelling is set so the runtime does not probe a second name",
       cs.count("ExactSpelling = true") > 10)

print("\n=== nested objects: a member is borrowed, a return is owned ===")
truthy("a class-typed member is a property of the bound C# type",
       "public Inner inner {" in cs)
truthy("and is documented as a view rather than a copy",
       "not a copy: writes reach" in cs)
truthy("it is constructed not-owning, so its handle will not free the owner's field",
       "(Get_inner(Check()), false, this)" in cs)
# Assigning a whole subobject would need C++ copy-assignment, and in C#
# `obj.inner = other` reads as rebinding a reference. Mutate through the view.
check("a class-typed member has no setter", "Set_inner" in cs, False)
truthy("a class returned by value is owned by the caller",
       "public Inner make()" in cs and "new Inner(r, true, null)" in cs)
truthy("the shim copies a returned value rather than pointing at the temporary",
       "new csb::Inner(" in shim_src)
truthy("a class parameter crosses as the wrapper's handle",
       "public double consume(Inner other)" in cs and "other.Borrow()" in cs)
truthy("the wrapper holds its owner so the GC cannot collect it first",
       "private readonly object _parent;" in cs)
# _parent is otherwise written and never read, which is CS0414 and an error
# under warnings-as-errors; the read is also the guarantee being made.
truthy("and that reference is actually read", "GC.KeepAlive(_parent);" in cs)

print("\n=== two name spaces that were never disjoint ===")
# A member x and a method get_x both want mb_<C>_get_x, which is a link error.
# And a C# property X compiles to hidden get_X/set_X, so the same pair is also
# "a member is already reserved" in C# -- a second, independent collision the
# native ledger cannot see.
truthy("the native symbols are distinct",
       "mb_Collide_get_x\"" in cs and "mb_Collide_get_x_\"" in cs)
truthy("the C# method is renamed off the property's hidden accessor",
       "public int get_x_()" in cs)
truthy("and the property keeps the C++ spelling", "public int x {" in cs)

print("\n=== one P/Invoke transition per call, not two ===")
# Throw used to call back into the library to ask whether an error was pending,
# so every generated call paid for two transitions. Measured at 58.1 -> 49.2
# ns/call under mono, which is 1.18x rather than the 2x it looks like: the
# second transition carries no arguments.
truthy("the shim exposes the error slot's address", "mb_error_slot" in shim_src)
truthy("the managed side caches it per thread", "[ThreadStatic] private static IntPtr _slot;" in cs)
truthy("and tests it with a memory read", "Marshal.ReadInt32(s)" in cs)

print("\n=== containers: a member is the live vector, a return is a copy ===")
truthy("a view class is generated per element type",
       "public sealed class DoubleVector : IReadOnlyList<double>, IDisposable" in cs)
truthy("and for strings too", "public sealed class StringVector" in cs)
truthy("one set of element helpers serves every class that uses that vector type",
       shim_src.count("mb_vec_double_size") == 1)
truthy("a vector member is handed back as the real vector",
       "public DoubleVector samples {" in cs and "(Get_samples(Check()), false, this)" in cs)
truthy("a vector returned by value is a copy the caller owns",
       "new DoubleVector(r, true, null)" in cs)
truthy("the shim copies rather than pointing at the temporary",
       "new std::vector<double>(" in shim_src)
truthy("the view is enumerable, so LINQ works", "IEnumerator<double> GetEnumerator()" in cs)
truthy("and indexed writes exist", "public double this[int i] {" in cs)
truthy("the element helpers range-check", "index out of range" in shim_src)
# vector<bool> packs bits and hands back a proxy, so there is no element to
# address. Binding it wrongly would be worse than not binding it.
truthy("vector<bool> is refused and says why",
       "flags --" in cs and "vector<bool" in cs)
truthy("the run reports it", "flags" in run.stdout and "does not cross" in run.stdout)
# Only an owned copy may be freed; a borrowed member view points into its owner.
truthy("the free helper is documented as owned-only",
       "must never reach this" in shim_src)

print("\n=== a borrowed view cannot outlive its owner ===")
# Holding a reference to the owner stops the GC collecting it and says nothing
# about Dispose. Before this, reading o.inner.x after o.Dispose() returned
# 9.27e-310 and samples.Count returned 37 -- freed memory, no exception.
# Python's equivalent views need only the reference, because Python has no
# explicit dispose. C# does, so the view has to be able to ask.
truthy("the owner-liveness interface is declared", "internal interface IMbOwner" in cs)
truthy("every class answers for itself", "bool IMbOwner.MbAlive {" in cs)
truthy("and the check walks the whole chain, so a view of a view is covered",
       "(_parent as IMbOwner)?.MbAlive != false" in cs)
truthy("a class view checks its owner before use",
       "the object this view belongs to has been disposed" in cs)
truthy("so does a vector view",
       "the object this vector belongs to has been disposed" in cs)

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
    # -warnaserror because a consumer's project very likely has it on, and it
    # is how the _parent field's CS0414 ("assigned but never used") was found.
    build = subprocess.run([MCS, "-warnaserror", "-out:driver.exe", "csb.cs", "Driver.cs"],
                           cwd=drive, capture_output=True, text=True)
    if build.returncode != 0:
        check("the generated C# compiles", build.stdout.strip()[:400]
              + build.stderr.strip()[:400], "")
    else:
        truthy("the generated C# compiles with warnings as errors", True)
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
        truthy("an enum round-trips as the enum, not an int",
               "mode=Fast promote(None)=Fast" in out and "mode set=Careful asint=7" in out)
        truthy("both overloads dispatch to the right C++ function",
               "overloads scaled(3)=15 scaled(3.0)=7.5" in out)
        truthy("statics run without an instance",
               "statics version()=3 describe=careful" in out)
        truthy("parameterized constructors reach the right C++ one",
               "ctor1 value=10" in out and "ctor2 value=13" in out)
        truthy("an inherited field writes through to the base subobject",
               "inherit base_id=20 tag=base base_method()=40 combined()=20.5" in out)
        truthy("a renamed colliding member still reaches the right C++ field",
               "renamed Handle_=1 Lib_=2 Dispose_()=3 New_()=99" in out)
        truthy("std::invalid_argument arrives as ArgumentException",
               "invalid_argument -> ArgumentException" in out)
        truthy("std::out_of_range arrives as ArgumentOutOfRangeException",
               "out_of_range -> ArgumentOutOfRangeException" in out)
        truthy("std::overflow_error arrives as OverflowException",
               "overflow_error -> OverflowException" in out)
        truthy("reading through a disposed wrapper raises rather than crashing",
               "disposed read raises" in out)
        truthy("disposing twice is a no-op", "double dispose is a no-op" in out)
        check("nothing reported a wrong exception type", "BUG " in out, False)
        truthy("writing through a borrowed member view reaches the real subobject",
               "nested view write reaches the owner: inner.x=7.5 scaled(2)=15" in out)
        truthy("a returned object is a copy the caller owns",
               "owned return: x=9.5 n=42" in out)
        truthy("an object passed as a parameter arrives intact",
               "class parameter: consume=107.5" in out)
        truthy("a vector member is live: Add and the indexer reach C++",
               "vector member: Count=2 [0]=9 sum=11.5" in out)
        truthy("a vector returned by value is the caller's copy",
               "vector return: Count=2 [0]=9" in out)
        truthy("a string vector round-trips per element",
               "string vector: [0]=gamma Count=1" in out)
        truthy("an out-of-range index raises rather than reading past the end",
               "vector range check raises" in out)
        truthy("a borrowed object view refuses to read a disposed owner",
               "disposed owner: view raises" in out)
        truthy("a borrowed vector view refuses too",
               "disposed owner: vector raises" in out)
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
