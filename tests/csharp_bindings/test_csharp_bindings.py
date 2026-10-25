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
using System.Collections.Generic;
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
            d.label = "日本 café 🚀";
            Console.WriteLine($"utf8 round trip: {d.label} len={d.label.Length}");
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
        using (var c2 = new Collide()) {
            Console.WriteLine($"collide x={c2.x} get_x_()={c2.get_x_()}");
        }
        using (var w = new Device()) {
            w.id = 4;
            Console.WriteLine($"widen(7)={w.widen(7L)}");
        }
        using (var mx = new Mixin()) {
            mx.v = 9;
            Console.WriteLine($"mixin v={mx.v} from_left()={mx.from_left()} w={mx.w}");
        }
        using (var sh = new Shadow()) {
            Console.WriteLine($"shadowed member={sh.Shadow_} z={sh.z}");
        }
        using (var nv = new Native()) Console.WriteLine($"class called Native: id={nv.id}");
        using (var dv = new DoubleVector()) Console.WriteLine($"class called DoubleVector: n={dv.n}");
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
        using (var op = new Device()) {
            Console.WriteLine($"opt defaults: limit={(op.limit.HasValue ? op.limit.Value.ToString() : "null")} trim={op.trim.Value} note={op.note}");
            op.limit = 42;
            op.trim = null;
            op.note = null;
            op.fallback = Mode.Careful;
            Console.WriteLine($"opt set: limit={op.limit.Value} trim={(op.trim.HasValue ? "set" : "null")} note={(op.note == null ? "null" : op.note)} fallback={op.fallback.Value}");
        }
        using (var o2 = new Device()) {
            o2.id = 3;
            Console.WriteLine($"opt return: {(o2.maybe(true).HasValue ? o2.maybe(true).Value.ToString() : "null")}/{(o2.maybe(false).HasValue ? "set" : "null")}"
                              + $" name={o2.maybe_name(true)}/{(o2.maybe_name(false) == null ? "null" : "set")}");
            Console.WriteLine($"opt param: absorb(9)={o2.absorb(9)} absorb(null)={o2.absorb(null)} name={o2.absorb_name(null)}");
        }
        using (var sq = new Square()) {
            sq.sides = 4;
            sq.weight = 9.5;
            sq.edge = 3.0;
            using (var j = new Judge()) {
                Console.WriteLine($"upcast implicit: count={j.count(sq)} weigh={j.weigh(sq)}");
            }
            var bv = sq.AsShapeBase();
            bv.sides = 7;
            Console.WriteLine($"base view writes through: sides={sq.sides} describe={sq.describe()} area={sq.area()}");
        }
        using (var vi = new Device()) {
            Console.WriteLine($"array in: sum_of={vi.sum_of(new double[]{1.0, 2.5, 3.5})}");
            vi.replace_samples(new double[]{4.0, 5.0});
            Console.WriteLine($"replaced the C++ vector: Count={vi.samples.Count} [1]={vi.samples[1]}");
            using (var other = new Device()) {
                other.samples.Add(10.0);
                other.samples.Add(20.0);
                Console.WriteLine($"a live view passes straight in: sum_of={vi.sum_of(other.samples)}");
            }
            Console.WriteLine($"empty list: sum_of={vi.sum_of(new double[0])}");
        }
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
            Console.WriteLine($"views cached: {ReferenceEquals(o.inner, o.inner)}");
            o.inner.x = 7.5;
            Console.WriteLine($"nested view write reaches the owner: inner.x={o.inner.x} scaled(2)={o.inner.scaled(2.0)}");
            using (var made = o.make())
                Console.WriteLine($"owned return: x={made.x} n={made.n}");
            using (var arg = new Inner()) {
                arg.x = 100.0;
                Console.WriteLine($"class parameter: consume={o.consume(arg)}");
            }
        }
        using (var e = new Echoer()) {
            Console.WriteLine($"keyword param: echo={e.echo("hi")}");
        }
        using (var lo = new Locals(5)) {
            Console.WriteLine($"locals: add={lo.add(7)}");
        }
        using (var wf = new WideFirst()) {
            Console.WriteLine($"wide-first: pick={wf.pick(5)} spick={WideFirst.spick(5)}");
        }
        using (var nf = new NarrowFirst()) {
            Console.WriteLine($"narrow-first: pick={nf.pick(5)} spick={NarrowFirst.spick(5)}");
        }
        using (var pr = new Pair(true)) {
            Console.WriteLine($"ctor bool: which={pr.which} n={pr.n}");
        }
        using (var pv = new Pair(new double[] { 1.0, 2.0, 3.0 })) {
            Console.WriteLine($"ctor vector: which={pv.which} n={pv.n}");
        }
        Console.WriteLine($"static container: total={Shared.total(new int[] { 1, 2, 3 })}");
        Console.WriteLine($"static optional: some={Shared.fallback(7)} none={Shared.fallback(null)}");
        var _some = Shared.maybe(true);
        var _none = Shared.maybe(false);
        var _noneTxt = _none.HasValue ? _none.Value.ToString() : "null";
        Console.WriteLine($"static opt return: some={_some} none={_noneTxt}");
        using (var _sv = Shared.spread(3)) {
            Console.WriteLine($"static vec return: Count={_sv.Count} [1]={_sv[1]}");
        }
        using (var wa = new WithArray()) {
            Console.WriteLine($"fixed view: Count={wa.@fixed.Count} IsFixedSize={wa.@fixed.IsFixedSize} [1]={wa.@fixed[1]}");
            wa.@fixed[2] = 7.0;
            Console.WriteLine($"fixed write reaches C++: total={wa.total()}");
            wa.labels[1] = "HI";
            Console.WriteLine($"fixed strings: joined={wa.joined()}");
            Console.WriteLine($"fixed ToArray: {string.Join(",", Array.ConvertAll(wa.@fixed.ToArray(), x => ((int)x).ToString()))}");
            Console.WriteLine($"array return: {string.Join(",", Array.ConvertAll(wa.spread(), x => ((int)x).ToString()))}");
            Console.WriteLine($"array param: sum={wa.sum_of_fixed(new double[] { 1.0, 2.0, 3.0 })}");
            Expect<ArgumentException>(() => wa.sum_of_fixed(new double[] { 1.0 }),
                                      "array param wrong length");
            Expect<NotSupportedException>(() => wa.@fixed.Add(1.0), "fixed Add");
            Expect<NotSupportedException>(() => wa.@fixed.Clear(), "fixed Clear");
        }
        using (var vfix = new Device()) {
            Console.WriteLine($"vector IsFixedSize={vfix.samples.IsFixedSize}");
        }
        using (var r = new Registry()) {
            var a0 = r.counts["a"];
            var hasB = r.counts.ContainsKey("b");
            var hasZ = r.counts.ContainsKey("z");
            Console.WriteLine($"map member: Count={r.counts.Count} a={a0} total={r.total()}");
            Console.WriteLine($"map lookup: hasB={hasB} hasZ={hasZ}");

            int miss;
            var found = r.counts.TryGetValue("z", out miss);
            Console.WriteLine($"map absent: found={found} value={miss}");

            r.counts["d"] = 9;
            var removed = r.counts.Remove("b");
            Console.WriteLine($"map write reaches C++: total={r.total()} Count={r.counts.Count} removed={removed}");

            var ks = new List<string>(r.counts.Keys);
            ks.Sort();
            var vls = new List<int>(r.counts.Values);
            vls.Sort();
            Console.WriteLine($"map halves: keys={string.Join(",", ks.ToArray())} values={string.Join(",", vls.ToArray())}");

            var pairs = new List<string>();
            foreach (var kv in r.counts) pairs.Add(kv.Key + ":" + kv.Value);
            pairs.Sort();
            Console.WriteLine($"map pairs: {string.Join(",", pairs.ToArray())}");

            var mode0 = r.labels["mode"];
            r.labels["mode"] = "slow";
            var mode1 = r.labels["mode"];
            Console.WriteLine($"string-valued map: was={mode0} now={mode1}");

            var w1 = r.weights[1];
            Console.WriteLine($"unordered map: w1={w1} Count={r.weights.Count}");

            using (var dbl = r.doubled()) {
                var da = dbl["a"];
                dbl["a"] = 999;
                var cppA = r.counts["a"];
                Console.WriteLine($"map return is an owned copy: doubled_a={da} cpp_a={cppA}");
            }

            Expect<KeyNotFoundException>(() => { var _ = r.counts["nope"]; }, "map missing key");

            r.counts.Clear();
            Console.WriteLine($"map clear reaches C++: total={r.total()} Count={r.counts.Count}");
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
# A string the claim can actually distinguish: three bytes for the kanji, two
# for the e-acute, four for the emoji, and a NUL-free multi-byte mix that
# Latin-1 or a code page would mangle.
UTF8 = "日本 café 🚀"
set_label(d, UTF8.encode("utf-8"))
check("and survives multi-byte characters intact", take(get_label(d)), UTF8)
check("with the byte length C++ actually stored",
      len(UTF8.encode("utf-8")), 3 + 3 + 1 + 3 + 2 + 1 + 4)
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

print("\n=== the library name the runtime will actually probe for ===")
truthy("the C# names the module, letting the runtime apply its own prefix/suffix",
       'internal const string Lib = "csb";' in cs)
import platform as _plat
_want = {"Darwin": "libcsb.dylib", "Windows": "csb.dll"}.get(_plat.system(), "libcsb.so")
truthy(f"and the built file is named {_want} for this platform",
       os.path.isfile(os.path.join(work, _want)))
cli_src = open(CLI).read()
truthy("the CLI picks the name per platform rather than always .so",
       "lib${module_name}.dylib" in cli_src and "${module_name}.dll" in cli_src)

truthy("a class is generated per C++ class",
       "public sealed class Device" in cs and "public sealed class Counter" in cs)
truthy("it is disposable", "public void Dispose()" in cs)
string_getters = re.findall(r"get \{\n\s*var r = Get_\w+\(Check\(\)\);(.*?)\}", cs, re.S)
truthy("there are string property getters to check", len(string_getters) >= 1)
check("every one of them routes the pointer through TakeString, which frees it",
      [g for g in string_getters if "MbNative.TakeString(r)" not in g], [])
truthy("and a string-returning method does too",
       "return MbNative.TakeString(r);" in cs)
truthy("a string parameter is marshalled in",
       "[MarshalAs(UnmanagedType.LPUTF8Str)] string" in cs)
truthy("a bool is converted at the edge, in the property itself",
       "get => Get_online(Check()) != 0;" in cs
       and "set => Set_online(Check(), value ? 1 : 0);" in cs)
truthy("a const member has a getter and no setter",
       "public int serial {" in cs and "Set_serial" not in cs)
truthy("errors are raised, not swallowed", "Native.Throw(" in cs)
check("every string property getter raises rather than returning null",
      [g for g in string_getters if "MbNative.Throw(" not in g], [])

print("\n=== and says what it left out ===")
# A generator that silently binds a subset is worse than one that refuses:
# the caller cannot tell a missing member from an absent one.
truthy("the skipped member is named in the C#", "flags --" in cs)
truthy("with the type that could not cross", "vector<bool" in cs)
truthy("the skipped method too", "widen() --" in cs)
check("a skipped member gets no property", "public  flags" in cs, False)

# The run itself must report them, so nobody has to read the generated file.
truthy("the generate run names the member and its reason on one line",
       re.search(r"Device\.flags: .*vector<bool", run.stdout) is not None)
# And the honest inverse: what used to be skipped is now bound, so a stale
# claim here would be the test lying about its own subject.
check("samples is no longer in the skipped list", "samples --" in cs, False)
check("history() is no longer in the skipped list", "history() --" in cs, False)

print("\n=== a const container or object member is refused, not miscompiled ===")
# The failure this replaces was the worst kind: nothing reported as left out,
# then "static_cast from 'const std::vector<double> *' to 'void *' is not
# allowed" from the C++ compiler on generated code.
truthy("a const vector member is named with its reason",
       re.search(r"Frozen\.baseline: it is const", run.stdout) is not None)
truthy("a const nested object member is too",
       re.search(r"Frozen\.origin: it is const", run.stdout) is not None)
check("the const vector member gets no property",
      "public MbDoubleVector baseline" in cs, False)
check("nor does the const object member", "public Counter origin" in cs, False)
truthy("while the writable member beside them still binds",
       "public double drift {" in cs)
truthy("and a method reading the const member still binds",
       "public double baseline_sum()" in cs)

print("\n=== the C# surface and the C++ shim spell a name separately ===")
# escape_cs_keyword turns `in` into `@in` for C#, which is not a C++
# identifier. One spelling used to reach both and the shim did not parse.
truthy("the C# parameter is escaped", "public string echo(string @in)" in cs)
truthy("and the shim keeps the name C++ declared",
       "mb_Echoer_echo(void* self, const char* in)" in shim_src)
check("no C# escape leaked into the shim", "@in" in shim_src, False)

print("\n=== a parameter cannot collide with a generated local ===")
# The body declares r, p, i, _has and <name>_arr for itself, and C# shares one
# scope between parameters and locals, so these were CS0128 on plain ints.
truthy("a parameter called r is renamed on the C# side",
       "public int add(int r_)" in cs)
truthy("so is one called p", "public Locals(int p_)" in cs)
truthy("while the shim still uses the declared names",
       "mb_Locals_add(void* self, int32_t r)" in shim_src
       and "mb_Locals_new_0(int32_t p)" in shim_src)

print("\n=== a collapsed overload pair calls the overload that was kept ===")
# Without a cast the shim forwards the raw wire variable and C++ re-resolves,
# picking the exact match -- which is the overload the generator REFUSED. The
# generated code is checked here; the behaviour is checked under mono below.
truthy("the forwarded argument is cast to the chosen overload's type",
       "pick(static_cast<long long>(" in shim_src
       and "pick(static_cast<long>(" in shim_src)
truthy("one overload of each pair is refused",
       cs.count("an earlier overload already has the C# signature pick(long)") == 2)
truthy("and of each static pair",
       cs.count("an earlier overload already has the C# signature static spick(long)") == 2)

print("\n=== a forward-declared type is reported, not a compiler error ===")
# std::is_class_v is true for a forward-declared class, so this used to reach
# typeid and fail to compile INSIDE the generator, naming no method. Found by
# pointing the compatibility corpus at C#: yaml-cpp has exactly this shape.
truthy("a method returning an incomplete type says so precisely",
       re.search(r"Forward\.make\(\): returns .*declared but not defined",
                 run.stdout) is not None)
check("and does not blame a deleted copy constructor for it",
      re.search(r"Forward\.make\(\): returns .*copy constructor",
                run.stdout) is not None, False)
truthy("and one taking it", 
       re.search(r"Forward\.take\(\): takes a class this module does not bind",
                 run.stdout) is not None)
truthy("while the rest of the class still binds",
       "public sealed class Forward" in cs and "public int doubled()" in cs)
check("and the generator did not mention its own source",
      "mirror_bridge_csharp.hpp" in run.stdout, False)

print("\n=== one slot description serves all three emitters ===")
# A constructor and a static method used to be able to spell strictly less than
# an instance method, and what they could not spell was emitted wrong rather
# than refused: a vector parameter arrived as a bare void*, which converts to
# bool. Both now build their parameter lists from the same arg_slot, so a
# container parameter costs two wire slots and a prologue wherever it appears.
truthy("a constructor takes a container as IReadOnlyList",
       "public Pair(IReadOnlyList<double> xs)" in cs)
truthy("as two wire slots, not one pointer",
       "mb_Pair_new_0(const double* xs_data, int64_t xs_n)" in shim_src)
truthy("and builds the vector from the range",
       "Pair(std::vector<double>(xs_data, xs_data + xs_n))" in shim_src)
check("no constructor takes a bare pointer", "public Pair(IntPtr" in cs, False)
truthy("the bool constructor still binds beside it", "public Pair(bool b)" in cs)

truthy("a static method takes a container too",
       "public static int total(IReadOnlyList<int> xs)" in cs)
truthy("and an optional, with its has-flag as a second slot",
       "public static int fallback(int? v)" in cs
       and "mb_Shared_s_fallback(int32_t v_has, int32_t v)" in shim_src)
truthy("a static string parameter still works",
       "public static string shout(string s)" in cs)
# Returns needed RetSlot: the has-flag is a trailing out-slot and the container
# is a heap copy, and both lived inside emit_method until it was shared.
truthy("a static method returns an optional",
       "public static int? maybe(bool want)" in cs)
truthy("with its has-flag as a trailing out-slot",
       "mb_Shared_s_maybe(int32_t want, int32_t* has)" in shim_src)
truthy("and a static method returns a container the caller owns",
       "public static MbIntVector spread(int n)" in cs)
check("no static method mentions the unhandled-wire poison",
      "MB_WIRE_UNHANDLED" in shim_src, False)

print("\n=== maps cross as a live view, and a set is not one ===")
truthy("a string-keyed map is an IReadOnlyDictionary",
       "public sealed class MbStringIntMap : IReadOnlyDictionary<string, int>" in cs)
truthy("a string-valued one too",
       "public sealed class MbStringStringMap : IReadOnlyDictionary<string, string>" in cs)
truthy("an unordered_map is named apart from an ordered one",
       "class MbIntDoubleHashMap" in cs)
truthy("the member is the live map", "public MbStringIntMap counts {" in cs)
truthy("and a returned map is that same type, owned by the caller",
       "public MbStringIntMap doubled()" in cs)
# One traversal, two owned vectors: handing keys and values back through
# separate calls would let the map change between them and pair key i with a
# value that is no longer its own, which the caller cannot detect.
truthy("keys and values come from a single snapshot call",
       "_snapshot(void* m, void** out_keys, void** out_vals)" in shim_src)
check("so there is no separate keys entry point", "_keys(void*" in shim_src, False)
truthy("and the halves reuse the vector view classes rather than new ones",
       "new MbStringVector(kp, true, null)" in cs)
truthy("a map parameter is reported rather than emitted wrong",
       re.search(r"Registry\.sum_of\(\): a parameter type", run.stdout) is not None)
# Deliberately not matching the type spelling: display_string_of prints what
# the compiler calls the type, and that differs -- clang says `set<basic_string
# <...>>` where GCC says `std::set<std::__cxx11::basic_string<char> >`. The
# member name and the refusal are ours; the spelling is the compiler's.
truthy("a std::set is reported as not crossing",
       re.search(r"TagSet\.names: type .*\bset<", run.stdout) is not None
       and "does not cross the C ABI yet" in run.stdout)
check("and was not quietly bound as a map", "StringSet" in cs, False)

print("\n=== two C++ types the C# wire spells alike keep their own helpers ===")
# cs_type maps every 64-bit signed type to "long", so the C# spelling cannot
# name the helper set. The first claimant keeps the plain name so that no
# existing generated name changes.
truthy("the first vector keeps the plain class name", "class MbLongVector :" in cs)
truthy("and the second gets a distinct one", "class MbLong2Vector :" in cs)
truthy("each with its own native helper set", "mb_vec_long2_size" in shim_src)
truthy("the two members have the two types",
       "public MbLongVector narrow {" in cs and "public MbLong2Vector wide {" in cs)

print("\n=== an unhandled wire kind cannot stream a null pointer ===")
# c_type and cs_wire_type used to return nullptr for anything they had not
# enumerated, which is undefined behaviour at the ostringstream and has
# segfaulted this generator before. The poison spelling fails loudly instead.
check("no null was streamed into the shim", "MB_WIRE_UNHANDLED" in shim_src, False)
check("nor into the C#", "MB_WIRE_UNHANDLED" in cs, False)

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
# Stated as a property of derived_body alone, not as an `or` with a claim
# already asserted above, which could not fail.
truthy("the split landed on the class body, which declares all three members",
       all(m in derived_body for m in ("public int base_id {", "public string tag {",
                                       "public double extra {")))
truthy("and not on some other class's body", "public double combined()" in derived_body)

print("\n=== the handle is a SafeHandle, not a raw IntPtr ===")
# Not tidiness. With an IntPtr field the last read of `this` is inside the
# argument list, so the JIT may report the object dead and the finalizer may
# run Delete while the native call is still on the stack. A SafeHandle is
# ref-counted across the call, which closes that window; it also makes Dispose
# idempotent and thread-safe and gives the handle a critical finalizer.
truthy("a SafeHandle subclass is generated per class", "DeviceHandle : SafeHandle" in cs)
truthy("it releases through the native delete", "protected override bool ReleaseHandle()" in cs)
# Four legitimately take a raw pointer. Delete, VFree and MFree are called
# from SafeHandle.ReleaseHandle, which is handed the bare handle field and has
# no SafeHandle to pass; FreeString takes a malloc'd char*, which is not a
# handle and is never wrapped in one. Everything else must take the typed
# handle, because an argument crossing as a bare IntPtr can be released by the
# GC mid-call -- the exact hole that typing `self` closed. This list is the
# whole point of the check, so a new release path belongs in it by name and
# nothing else does: adding maps is what first made it four.
instance_externs = re.findall(
    r"private static extern [^\n(]+ (\w+)\(([^)]*)\)", cs)
raw_self = [n for n, a in instance_externs
            if a.startswith("IntPtr ")
            and n not in ("Delete", "VFree", "MFree", "FreeString")]
check("only the release paths take a raw pointer", raw_self, [])
truthy("and the release paths do, because ReleaseHandle has only the field",
       any(n == "Delete" and a == "IntPtr self" for n, a in instance_externs))
truthy("they take the handle instead, so the runtime keeps it alive",
       any(re.match(r"\w+Handle self", a) for _, a in instance_externs)
       and any(a.startswith("H v") for _, a in instance_externs))
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
for sig in ("public double scale(double factor)", "public bool toggle(bool on)",
            "public void rename(string next)", "public int sum(int a, int b)",
            "public Mode promote(Mode m)"):
    truthy(f"signature reads {sig}", sig in cs)
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
dllimports = re.findall(r"\[DllImport\([^\]]*\)\]", cs)
truthy("there are DllImports to check at all", len(dllimports) > 20)
check("every one of them sets ExactSpelling",
      [d for d in dllimports if "ExactSpelling = true" not in d], [])

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
# Anchored to the function that returns by value: "new csb::Inner(" on its own
# is matched by mb_Inner_new's own `return new csb::Inner();`, so the assertion
# held no matter what make() did.
make_body = shim_src.split("mb_Outer_make(")[1].split("\n}")[0]
# The access is qualified with its declaring class now, which is why this
# names Outer::make rather than make.
truthy("the shim heap-copies the returned value",
       "new csb::Inner(static_cast<csb::Outer*>(self)->Outer::make())" in make_body)
check("and does not hand back the address of the temporary",
      "&static_cast<csb::Outer*>(self)->Outer::make()" in make_body, False)
truthy("a class parameter crosses as the wrapper's handle",
       "public double consume(Inner other)" in cs
       and 'Native.NotNull(other, "other").Borrow()' in cs)
truthy("and the extern takes the handle type, so the marshaller ref-counts it",
       "Call_consume(OuterHandle self, Inner.InnerHandle other)" in cs)
truthy("a null class argument names the parameter instead of dereferencing",
       "ArgumentNullException(name)" in cs)
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
       "public sealed class MbDoubleVector : IReadOnlyList<double>, IDisposable" in cs)
truthy("and for strings too", "public sealed class MbStringVector" in cs)
truthy("one set of element helpers serves every class that uses that vector type",
       shim_src.count("mb_vec_double_size") == 1)
truthy("a vector member is handed back as the real vector",
       "public MbDoubleVector samples {" in cs and "(Get_samples(Check()), false, this)" in cs)
truthy("a vector returned by value is a copy the caller owns",
       "new MbDoubleVector(r, true, null)" in cs)
truthy("the shim copies rather than pointing at the temporary",
       "new std::vector<double>(" in shim_src)
truthy("the view is enumerable, so LINQ works", "IEnumerator<double> GetEnumerator()" in cs)
truthy("and indexed writes exist", "public double this[int i] {" in cs)
truthy("the element helpers range-check", "index out of range" in shim_src)
truthy("vector element access is handle-typed too, not a raw pointer",
       "VGet(H v, long i)" in cs and "VSize(H v)" in cs)
truthy("ToArray does one bulk copy rather than a transition per element",
       "Marshal.Copy(p, a, 0, n)" in cs)
# A refused overload used to leave a live entry point that no generated C#
# calls and that C++ overload resolution would answer from the wrong overload.
check("a refused overload leaves no entry point behind",
      shim_src.count("mb_Device_widen") > 1, False)
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
# Checking the owner and then calling is a check-then-use race: another thread
# can dispose in between and the in-flight call reads freed memory. A ref held
# for the view's whole lifetime defers the owner's release, so the racing call
# reads memory that is still there.
truthy("a borrowed view holds a ref on its owner's handle",
       "_pin.DangerousAddRef(ref _pinned);" in cs)
truthy("and releases it on dispose", "_pin.DangerousRelease();" in cs)
# With a ref outstanding the handle is deliberately not closed, so it cannot
# answer "was the owner disposed" -- only "is the memory still there".
truthy("disposal is tracked separately from the handle",
       "private volatile bool _disposed;" in cs and "_disposed = true;" in cs)
truthy("and every guard consults it first",
       "if (_disposed || _h.IsInvalid || _h.IsClosed)" in cs)
# A borrowed view's pointer is a fixed offset into the owner, so a fresh
# wrapper -- and a fresh finalizable SafeHandle -- on every read was only GC
# pressure. Count is still read from C++ each time, so growth is still seen.
truthy("a member view is cached rather than rebuilt per read",
       "private Inner _view_inner;" in cs)
truthy("and so is a vector view", "private MbDoubleVector _view_samples;" in cs)

print("\n=== inherited access is qualified with the class that declares it ===")
# Unqualified worked until inherited members started crossing. Two bases with
# one member name gave "member 'v' found in multiple base classes", and a
# member named after the derived class lost to the injected-class-name. Both
# took the whole module down rather than one member.
truthy("the shim names the declaring base", "->LeftBase::v" in shim_src)
truthy("and qualifies the shadowed member too", "->HasShadow::Shadow" in shim_src)
check("no unqualified inherited access is left", "self)->v" in shim_src, False)
# C# separately forbids a member named after its enclosing type.
truthy("a member named after its class is renamed", "public int Shadow_ {" in cs)
truthy("and the rename is reported", "Shadow.Shadow renamed to Shadow_" in run.stdout)

print("\n=== the generator's own namespace-scope names cannot be taken ===")
# A C++ struct called Native, or DoubleVector beside a vector<double> member,
# was CS0101 "already contains a definition for". The generator's own types are
# prefixed so no C++ name reaching here can collide.
truthy("the helper class is prefixed", "internal static class MbNative" in cs)
truthy("so are the vector views", "public sealed class MbDoubleVector" in cs)
truthy("a C++ class really called Native still binds",
       "public sealed class Native :" in cs)
truthy("and one called DoubleVector does too",
       "public sealed class DoubleVector :" in cs)

print("\n=== what used to break the generator itself ===")
# std::array has value_type and .data() but no allocator_type. Naming
# allocator_type outside the detection guard made this a hard compile error in
# the generator rather than a member reported as not crossing.
print("\n=== a std::array member is a live fixed view ===")
# The helpers take a raw E* and the length as an argument, so std::array<E,3>
# and std::array<E,16> share one set and one view class with std::vector<E>.
# Generating a second MbDoubleFixed alongside MbDoubleVector would have
# duplicated the indexer, ToArray, the SafeHandle and the owner pin.
truthy("it binds as the element type's own view class",
       "public MbDoubleVector @fixed {" in cs)
truthy("and there is still exactly one view class for that element type",
       cs.count("public sealed class MbDoubleVector") == 1)
truthy("one helper set serves every N, keyed on the element type alone",
       shim_src.count("mb_fix_double_get") == 1)
truthy("the length comes from the C++ type rather than a native call",
       "new MbDoubleVector(Get_fixed(Check()), 3L, this)" in cs)
truthy("the getter hands out the array's own storage",
       "fixed.data());" in shim_src)
truthy("a C# keyword member name is escaped, not renamed", "@fixed" in cs)
truthy("an array of strings works the same way",
       "public MbStringVector labels {" in cs)
truthy("the class around it still binds", "public sealed class WithArray" in cs)
check("and no fixed helper is given a free, because a view owns nothing",
      "mb_fix_double_free" in shim_src, False)

# Two refusals, each for its own reason and each named.
truthy("a const array member is refused like a const vector",
       re.search(r"WithArray\.pinned: it is const", run.stdout) is not None)
# An array has no identity apart from its storage, so it comes back as a
# copied double[] rather than a view of a temporary -- no handle, no free.
truthy("an array returned by value is a copied array",
       "public double[] spread()" in cs)
truthy("filled through a buffer the caller owns",
       "mb_WithArray_spread(void* self, double* out, int64_t out_n)" in shim_src)
truthy("and the count it returns is checked rather than discarded",
       "if (r != 3) throw MbNative.LastError" in cs)
truthy("an array OF STRINGS returned by value is still refused",
       re.search(r"WithArray\.spread_labels\(\): returns", run.stdout) is not None)
# An array parameter is the vector parameter's two wire slots plus a length
# check. The count is already known on both sides from the C++ type, so a
# mismatch throws rather than binding a short array.
truthy("an array parameter takes the same two wire slots a vector does",
       "mb_WithArray_sum_of_fixed(void* self, const double* a_data, int64_t a_n)"
       in shim_src)
truthy("and the same C# surface",
       "public double sum_of_fixed(IReadOnlyList<double> a)" in cs)
truthy("the shim checks the length rather than clamping it",
       "mb_to_array<double, 3>(a_data, a_n)" in shim_src
       and "throw std::length_error" in shim_src)
# The shim re-parses the project's headers. Pinned at C++17 it rejected any
# header using a C++20 feature, on a project whose subject is modern C++.
truthy("a header using a C++20 feature still builds through the shim stage",
       "public sealed class Modern" in cs)

print("\n=== containers crossing inward ===")
# The shape a real caller writes, and the direction this backend refused
# longest. Two wire parameters for one C# argument -- a pointer and a length --
# so the shim builds the vector from the range and nothing is allocated or
# freed on the managed side.
truthy("a vector parameter takes a read-only list",
       "public double sum_of(IReadOnlyList<double> values)" in cs)
truthy("and crosses as a pointer plus a length",
       "Call_sum_of(DeviceHandle self, [In] double[] values_data, long values_n)" in cs)
truthy("the shim builds the vector from the range",
       "std::vector<double>(values_data, values_data + values_n)" in shim_src)
truthy("a list that is already an array is passed without a second copy",
       "values as double[]" in cs)
# A vector of strings would need an array of char* with its own marshalling.
# Refusing half the feature by name beats refusing both.
truthy("a container of strings is refused and says which half is missing",
       "count_names() --" in cs and "array of char*" in cs)

print("\n=== a Derived can be passed where a Base is expected ===")
# A generated class is sealed with no C# base type, so without this a class
# hierarchy could not be bound at all -- and most real C++ libraries are one.
truthy("an upcast view is generated per bound base",
       "public ShapeBase AsShapeBase()" in cs and "public Tagged AsTagged()" in cs)
truthy("with an implicit conversion so the call site reads like C#",
       "public static implicit operator ShapeBase(Square value)" in cs)
truthy("a null conversion names the argument",
       "throw new ArgumentNullException(nameof(value))" in cs)
# ShapeBase is Square's SECOND base, so its offset is not zero. Assuming a
# Derived* is already a Base* would read weight's bytes as sides.
truthy("the pointer is adjusted by C++, not assumed",
       "static_cast<csb::ShapeBase*>(static_cast<csb::Square*>(self))" in shim_src)
truthy("the upcast is a borrowed view, so it carries the owner pin",
       "AsShapeBaseRaw(Check()), false, this)" in cs)

print("\n=== std::optional is C# nullable, both ways ===")
truthy("a scalar optional is a nullable", "public int? limit {" in cs
       and "public double? trim {" in cs)
truthy("an optional enum keeps the enum type", "public Mode? fallback {" in cs)
# A string is already nullable in C#, so a null pointer is the whole signal
# and no separate has-value flag is on the wire for it.
truthy("an optional string is just a string", "public string note {" in cs)
truthy("a scalar optional carries a has-value flag",
       "out int has" in cs and "int32_t* has" in shim_src)
check("a string optional does not need one",
      "mb_Device_get_note(void* self, int32_t* has)" in shim_src, False)
truthy("assigning null calls reset on the C++ side", ".reset();" in shim_src)
# optional<int> is a reflectable class that is not string-like, not a
# container, not arithmetic and not a smart pointer, so it satisfies
# NestedBindable and was classified as an opaque handle before the optional
# branch was reached.
check("an optional is not reported as an unbound class",
      "limit --" in cs or "note --" in cs, False)

print("\n=== an optional in all three positions, not just as a member ===")
# Claiming the matrix row on members alone was an overclaim, and the generator
# segfaulted on an optional return: c_type has no spelling for Wire::Opt and
# the emitter streamed its nullptr.
truthy("an optional return is nullable", "public int? maybe(bool want)" in cs)
truthy("an optional string return is a string", "public string maybe_name(bool want)" in cs)
truthy("an optional parameter is nullable", "public int absorb(int? v)" in cs)
truthy("and an optional string parameter is a string",
       "public string absorb_name(string v)" in cs)
truthy("a scalar return carries the flag as an out-param",
       "out int has" in cs and "int32_t* has" in shim_src)
check("a string return does not, because null already says it",
      "mb_Device_maybe_name(void* self, int32_t want, int32_t* has)" in shim_src, False)

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
        truthy("multi-byte UTF-8 survives the round trip through C#",
               "utf8 round trip: 日本 café 🚀 len=10" in out)
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
        truthy("an array crosses inward and C++ sums it",
               "array in: sum_of=7" in out)
        truthy("and can replace the C++ vector wholesale",
               "replaced the C++ vector: Count=2 [1]=5" in out)
        # The generated view implements IReadOnlyList<T>, so one object's
        # member reaches another object's method with no copy on this side.
        truthy("another object's live view passes straight in",
               "a live view passes straight in: sum_of=30" in out)
        truthy("an empty list is a valid empty vector, not a null pointer",
               "empty list: sum_of=0" in out)
        # Tagged is the first base (offset 0) and ShapeBase the second, so
        # reading sides correctly proves the non-zero offset was applied.
        truthy("a Derived converts implicitly to either base, with the right offset",
               "upcast implicit: count=4 weigh=9.5" in out)
        truthy("and the base view writes through to the derived object",
               "base view writes through: sides=7 describe=700 area=9" in out)
        truthy("an optional reads its C++ value or null",
               "opt defaults: limit=null trim=2.5 note=hi" in out)
        truthy("and writing null reaches reset() on the C++ side",
               "opt set: limit=42 trim=null note=null fallback=Careful" in out)
        truthy("an optional return carries both a value and its absence",
               "opt return: 6/null name=probe/null" in out)
        truthy("and an optional parameter carries null through to nullopt",
               "opt param: absorb(9)=9 absorb(null)=-1 name=none" in out)
        truthy("a borrowed object view refuses to read a disposed owner",
               "disposed owner: view raises" in out)
        truthy("a borrowed vector view refuses too",
               "disposed owner: vector raises" in out)
        truthy("the same wrapper comes back on every read", "views cached: True" in out)
        # The ledger exists so a member x and a method get_x reach different
        # C++ things. Asserted behaviourally: swapping the two entry points
        # used to leave every assertion byte-identical.
        truthy("the symbol ledger wires x and get_x to different C++ members",
               "collide x=1 get_x_()=10" in out)
        truthy("the bound overload of a collapsed pair is the first one",
               "widen(7)=8" in out)
        truthy("two bases with one member name resolve to the declaring base",
               "mixin v=9 from_left()=90 w=0.5" in out)
        truthy("a member named after its class reaches the renamed property",
               "shadowed member=7 z=1" in out)
        truthy("a C++ class called Native binds and works",
               "class called Native: id=3" in out)
        truthy("so does one called DoubleVector",
               "class called DoubleVector: n=4" in out)
        truthy("a map member reads its C++ contents",
               "map member: Count=3 a=1 total=6" in out)
        truthy("ContainsKey answers from C++",
               "map lookup: hasB=True hasZ=False" in out)
        truthy("an absent key is false, not an exception",
               "map absent: found=False value=0" in out)
        # total() is recomputed in C++, so this is the proof that the write
        # landed in the real map rather than in a copy of it.
        truthy("writing and removing through the view reach C++",
               "map write reaches C++: total=13 Count=3 removed=True" in out)
        truthy("keys and values come back as the two halves",
               "map halves: keys=a,c,d values=1,3,9" in out)
        truthy("enumeration pairs each key with its own value",
               "map pairs: a:1,c:3,d:9" in out)
        truthy("a string-valued map round-trips both ways",
               "string-valued map: was=fast now=slow" in out)
        truthy("an unordered_map with int keys works too",
               "unordered map: w1=0.5 Count=2" in out)
        truthy("a returned map is a copy, so writing it leaves C++ alone",
               "map return is an owned copy: doubled_a=2 cpp_a=1" in out)
        truthy("a missing key raises KeyNotFoundException",
               "map missing key -> KeyNotFoundException" in out)
        truthy("Clear reaches C++",
               "map clear reaches C++: total=0 Count=0" in out)
        truthy("a fixed view reports the length from the C++ type",
               "fixed view: Count=3 IsFixedSize=True [1]=2" in out)
        truthy("a write through it reaches the owner's own storage",
               "fixed write reaches C++: total=10" in out)
        truthy("a fixed array of strings reads and writes too",
               "fixed strings: joined=lo/HI" in out)
        truthy("ToArray copies the fixed storage out",
               "fixed ToArray: 1,2,7" in out)
        truthy("growing it is refused rather than silently ignored",
               "fixed Add -> NotSupportedException" in out)
        truthy("and so is clearing it",
               "fixed Clear -> NotSupportedException" in out)
        truthy("a plain vector view is still not fixed",
               "vector IsFixedSize=False" in out)
        # The expected value comes from each fixture's DECLARATION ORDER, not
        # from a literal: the first-declared overload is the one bound, and a
        # single-order test passes by luck because on LP64 the accidental
        # resolution agrees with the choice in exactly one of the two orders.
        truthy("the wide-first pair calls its wide overload",
               "wide-first: pick=2 spick=2" in out)
        truthy("the narrow-first pair calls its narrow overload",
               "narrow-first: pick=1 spick=1" in out)
        truthy("a parameter named after a generated local still works",
               "locals: add=12" in out)
        truthy("a C# keyword parameter name still works",
               "keyword param: echo=p:hi" in out)
        truthy("the bool constructor runs when it is the one chosen",
               "ctor bool: which=2 n=1" in out)
        truthy("and the container constructor runs when it is",
               "ctor vector: which=1 n=3" in out)
        truthy("a static method receives a container",
               "static container: total=6" in out)
        truthy("and an optional, present and absent",
               "static optional: some=7 none=-1" in out)
        truthy("an array comes back by value as a copied array",
               "array return: 1,2,7" in out)
        truthy("a static method returns an optional, present and absent",
               "static opt return: some=9 none=null" in out)
        truthy("and a container the caller owns",
               "static vec return: Count=3 [1]=2" in out)
        truthy("a std::array parameter crosses inward and C++ sums it",
               "array param: sum=6" in out)
        truthy("and the wrong number of elements raises rather than binding short",
               "array param wrong length -> ArgumentException" in out)
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
