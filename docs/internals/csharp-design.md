# C# Backend: Design Decisions and Known Gaps

Written for a reader who knows .NET better than they know this project.

## Why C# is shaped differently from the other three backends

P2996 cannot declare a function whose **name is computed**, and every P/Invoke
target needs its own exported symbol. So unlike Python, Lua and JavaScript —
which each emit one module directly — C# generation is **two stages**:

1. a generator built *with* reflection reads the C++ headers and **emits text**:
   a flat C ABI shim (`<module>_shim.cpp`), the C# source (`<module>.cs`), and a
   C header (`<module>.h`);
2. an ordinary compiler with **no reflection support** compiles the shim into a
   `.so`/`.dll`.

Two consequences follow, and both are load-bearing:

- The generated `.h` is a genuine public artifact. Any C or C++17 caller can use
  the same ABI, which is why issue #3 (bindings for older C++) is answered by a
  by-product rather than a feature.
- **The shim stage needs no reflection.** MSVC has no P2996 support and no
  published ETA (spring 2026), but generated text can in principle be checked in
  and built to a `.dll` by MSVC indefinitely. *This has never been attempted —
  no Windows available. `MB_EXPORT` is currently a GCC/clang visibility
  attribute, and the shim builds with flags that have no direct MSVC
  equivalents.*

## The decisions

### Ownership is fixed when the generator writes the symbol

A **member** is a borrowed view whose writes reach the owning C++ object. A value
**returned by value** is a copy the caller owns and disposes. The same C++ type
gets two different wire shapes depending on position, and the choice is never
made at runtime.

This is the most load-bearing rule in the backend. A wrong return arm is a
*memory* bug rather than a compile error, which is why the parameter half of the
slot refactor landed before the return half.

### Lifetime safety: SafeHandle per class, plus owner pinning

Every bound object is a `SafeHandle`, never a raw `IntPtr`, so the marshaller
ref-counts it across a call and the GC cannot release it mid-call. A test
invariant enforces that no P/Invoke takes a bare `IntPtr` except the release
paths, and that list is checked by name.

A borrowed view calls `DangerousAddRef` on its owner for its **whole lifetime**
(not per call — check-then-call would be a race) and tracks disposal in a
separate flag, so a disposed owner raises `ObjectDisposedException` rather than
reading freed memory.

Verified on .NET 8 CoreCLR: double `Dispose` is safe; use-after-`Dispose` and a
view of a disposed owner both throw.

### C++ names are preserved verbatim

`greet_count` stays `greet_count`. Framework-provided members (`Count`,
`ToArray`, `Dispose`) are PascalCase, and a C# keyword is escaped (`@fixed`)
rather than renamed.

This is a **trade, not a claim about idiom**: one mental model across four
backends, and no second list of names to keep in step — which is this project's
characteristic failure mode. Renaming in C++ renames the property on the next
build. The mapping to PascalCase is derivable, so this is a policy decision
rather than a cost one.

### Refusal is a feature

Anything that cannot cross is reported **by name with a reason**, in the run
output and as a comment in the `.cs` where the member would have been. The
contract is that a missing member can be told from an absent one.

There is also a class-level refusal path, because a refused class is absent from
the `.cs` entirely and otherwise indistinguishable from a class the headers never
contained.

### Fail loud, never silent

Earned the hard way. Poison spellings (`MB_WIRE_UNHANDLED`,
`MB_CLASS_UNREGISTERED`) rather than `nullptr` or an empty string, so a missing
case is a named compile error. `-Wl,--no-undefined` on the shim link, because the
build previously reported success for a `.so` with 125 undefined symbols. The
generated C# is compiled with `-warnaserror` during the build, so a managed-side
codegen bug fails *our* build rather than the consumer's.

### One helper set per element type, keyed on the C++ type

`vector<T>` and `array<T,N>` share one view class and one native helper set; a
map's snapshot halves *are* vector plans. Keyed on the C++ type and not the C#
spelling, because `long` and `long long` both spell `long` in C# and would
otherwise collide into one set of symbols.

### A property's hidden accessors are names in the ledger

A C# property `X` compiles to hidden `get_X` and `set_X` methods. So a C++ class
with members `value` and `set_value` is CS0102, "already contains a definition",
with no collision on the C++ side at all — and the error points at a property
accessor the reader never wrote.

The ledger reserves those spellings. What matters is that it checks them
**before** claiming a name, not after: reserving the accessors afterwards covers
only the direction where the property comes first.
`cxxopts::values::parser_tool::ArguDesc` declares `set_value` before `value`,
which is the other direction, and it cost cxxopts its whole module — a `CS0102`
the CLI's own `-warnaserror` compile of the generated `.cs` reported directly,
so this one is attributed with certainty rather than inferred from a corpus row.

The first claimant keeps the plain name and the later one gains a `_`, which is
the same tie-break the native symbol ledger and the helper-tag ledger use:
declaration order decides, so a rename is stable across builds.

### A class C# cannot own binds borrow-only

A deleted, private or protected destructor is how a factory-style C++ API says
the caller does not free these. `tinyxml2::XMLElement`, `b2Body`, `b2Joint` —
ten classes in the 11-library corpus, every one central API. Such a class used
to be refused whole, which for tinyxml2 meant refusing most of what the library
is for.

It binds borrow-only now, under one invariant:

> No C# code may ever hold an *owned* handle to a borrow-only class.

Enforced one site per way an owned handle could appear:

| Site | What closes it |
|---|---|
| `mb_T_new` + `public T()` | `is_default_constructible_v` is false |
| a return of `T` **by value** (the shim heap-copies) | `is_copy_constructible_v` is false |
| a reflected parameterised constructor | **a gate** — this is the only one not already closed |
| `mb_T_delete` + `ReleaseHandle` | not emitted at all |

The first two are free, and the reason is worth knowing: each of those traits is
a *variable-definition* check, and a variable definition needs an accessible
destructor. Measured for the private, protected and deleted spellings alike.
There is a guard in `add_class` for the case where a compiler disagrees, because
that is a fact about the language rather than about this code and the failure if
it stopped holding would be a leak, not a compile error.

The third is not closed, because `new T(args...)` is well-formed with an
inaccessible destructor. It would hand C# an owned pointer with nothing able to
free it, so it is refused by name.

The fourth is the one C# gets for free where Python did not. Python's blocker
(issue #48) was `py_dealloc<T>` spelling the free as a runtime `if`, which
instantiates `delete T*` for every bound `T` — a compile hazard that forced a
class-level gate. Here the deleter is generated *text*: the generator either
writes `mb_T_delete` or does not. And `SafeHandle` with `ownsHandle: false`
never calls `ReleaseHandle`, so a borrowed handle is already a thing that cannot
free. **The absence of the deleter is the invariant** — not an empty one, not a
guarded one, none — because a deleter that exists is a deleter something can
call.

### A pointer return is a borrow pinned to its receiver

Half one alone would be a lie. A by-value member of a borrow-only class would
make its *owner* non-destructible too, so the owner would become borrow-only in
turn and nothing would be reachable. A pointer return is the only route to one —
which is also how the real libraries are shaped: `XMLDocument::RootElement()`,
`FirstChildElement()`, `b2World::GetBodyList()`.

So an **instance method** returning `T*`, where `T` is a bound class, hands back
a borrowed view: no copy, `ownsHandle: false`, and `MbPin` takes a ref on the
receiver's handle for the view's whole lifetime. `const T*` crosses the same way,
const-cast at the boundary, because C# has no const to carry the distinction into
— the same information this backend already drops for a `const T&` member.

A null pointer is a **value** here, not a failure: that is how
`FirstChildElement()` says there is none, so it becomes `null`. The owned-return
arm reads a null as an error, and conflating the two would turn "no such child"
into an exception.

**Instance methods only.** A static method, a free function or an operator has
no receiver, so a borrow minted there would be unowned by construction. The
emitters do not each decide this; `ret_type_ok<Ret, HasReceiver>` takes it as a
parameter, because the asymmetry is the design and a predicate in one of three
emitters is how the string-container gate became a live defect.

**The hazard, stated plainly and also in the generated API.** The pin covers
"the C# view outlives its owner". It does *not* cover "the owner is alive and
C++ destroyed the pointee" — `XMLDocument::Clear()`, `b2World::DestroyBody()`.
After that, reading the view is undefined. Closing it needs a generation counter
on the owner, which is a larger design. A borrow is a borrow, with C++ lifetime
rules, and the generated `<remarks>` on every such method says so, because the
`.cs` is the only place a caller will look.

Related: a `T*` that *transfers* ownership (`T* clone()`) is bound as a borrow
and leaks. The policy is right for the corpus and wrong for that shape, and the
type alone cannot tell them apart.

### A container of strings crosses inward as a blob plus byte counts

`vector<string>` and `array<string,N>` take **three** wire slots for one C#
argument: a contiguous UTF-8 `const char*`, a `const int64_t*` of one byte count
per element, and the count. Not `char**`, which is the obvious encoding and was
the one named in this file as the missing piece.

Two reasons, and both are about what the managed side can do without help. The
blob and the counts are blittable arrays, so the P/Invoke marshaller pins them
for the duration of the call and the generated C# holds no `GCHandle`, needs no
`finally`, and compiles without `AllowUnsafeBlocks` — where `string[]` as
`char**` would mean either an `ArraySubType` the built-in marshaller does not
accept for UTF-8, or hand-pinning one buffer per element. And an explicit length
carries an embedded NUL, which a NUL-terminated `char**` cannot; this is the one
place in the backend where a `std::string` does **not** truncate.

The gate this replaced lived on methods only, so the same C++ signature on a
constructor or a static method was emitted as `const std::string*` against a C#
`string[]` — a `char**` read as `std::string` objects. It is the clearest case
in the backend for deleting a refusal rather than copying it: a gate in one of
three places is worse than no gate at all.

### Errors cross as a kind plus a message

C++ exceptions are caught at the ABI boundary; the kind and message go into a
thread-local slot and the managed side raises a typed .NET exception
(`invalid_argument` → `ArgumentException`, `out_of_range` →
`ArgumentOutOfRangeException`, and so on).

There is no first-party mechanism for this: `SetLastError` carries one `int` with
no type or message, and HRESULT is COM-shaped. This is the only available shape,
not a workaround for a facility we ignored.

## Known gaps, in rough priority order

### Not idiomatic .NET yet

- **315 `DllImport`, zero `LibraryImport`.** The diff is known and small:
  containing types become `partial` (ours are not, including the nested handle
  classes), externs become `static partial`, `ExactSpelling` is dropped (no
  equivalent), `CallingConvention.Cdecl` becomes `[UnmanagedCallConv]`, and the
  scaffold needs `AllowUnsafeBlocks`. SafeHandle parameters survive —
  `SafeHandleMarshaller<T>` exists in net8.0+ and the parameterless-constructor
  requirement applies only to `ref`/`out`/return.

  What blocks it is the build, not the design. `LibraryImport` is a Roslyn
  **source generator**: it writes the P/Invoke stub at compile time. mcs 6.8
  does not run source generators, so switching would leave every `static
  partial` extern without a body under the only C# compiler this repo has.
  Same root cause as the nullability item below, and the same fix — a real .NET
  SDK in the build.
- **A default `dotnet new` project does not build the output.** Both templates
  write `Nullable=enable`; the generated `.cs` has no `#nullable` directive and
  inherits it, producing **184 warnings** (81 CS8618, 46 CS8625, 2 CS8603) — and
  184 hard errors with `TreatWarningsAsErrors`. The `init` scaffold hides this
  only because its hand-written csproj omits `<Nullable>`, making the scaffold
  *less* idiomatic than the template a .NET developer would use.
- **`std::optional<std::string>` surfaces as `string`, not `string?`**, and the
  getter body returns `null`. Scalars are correct (`int?`, `double?`, `Mode?`).
  The annotation must land together with replacing the C# gate: `#nullable enable`
  is a fatal CS1024 on mcs 6.8, which is currently the only compiler in the
  build. These two items and the `LibraryImport` one are one piece of work:
  install a .NET SDK, then generate for it.

### Correctness and resource gaps

- **`std::string` truncates at an embedded NUL on the way OUT**, because the
  outbound wire is a NUL-terminated `const char*`. Lua and JavaScript carry the
  length and do not lose data. A *container* of strings inbound does carry
  lengths, so the asymmetry is now within one backend: the fix for a scalar
  string is the same shape, a pointer plus a byte count.
- **The C++ runtime is not statically linked.** A clang-built shim needs
  `libc++.so.1`, `libc++abi.so.1` and `libunwind.so.1`, and the CLI has no
  static-link option. A GCC 16 generator stage would link `libstdc++.so.6`
  instead, which is far more commonly present — *plausible, unverified*.

### Type coverage

No `std::set`, `std::variant`, `std::expected`, smart pointers or
`vector<BoundClass>`. A `std::map` **parameter** does not cross inward, though
every other container now does, including a container of strings — see the
string-blob wire above. A map parameter is now mostly vocabulary rather than
design: its halves are already `VectorPlan`s, and a string half has a wire.

## What the corpus says, as of 2026-10-30

Pointing `compatibility/run_compatibility.py --lang csharp` at the **seven**
libraries that bind under the Python backend: **6 of 7 bind usefully**, every one
of them with the `must_export` names the manifest demands.

| library | verdict | classes | API the manifest requires |
|---|---|---|---|
| jsoncpp | **ok** | 23 | `Value`, `Reader`, `StreamWriterBuilder` |
| pugixml | **ok** | 19 | `xml_document`, `xml_node` |
| clipper2 | **ok** | 20 | `Clipper64`, `ClipperOffset` |
| cxxopts | **ok** | 27 | `Options`, `ParseResult` |
| SQLiteCpp | **ok** | 9 | `Database`, `Statement`, `Column` |
| tinyobjloader | **ok** | 26 | `ObjReader`, `attrib_t`, `shape_t` |
| pmp-library | `reflection-tu-failed` | 32 discovered | — |

Which of these six were genuinely broken, and how certainly:

- **cxxopts, pugixml and tinyobjloader** were real failures this branch fixed,
  attributed with certainty because the compiler error was read directly — a
  `CS0102`, a mutable `char*` passed to a `char*`, and an unqualified class name
  in two translation units.
- **jsoncpp and SQLiteCpp** both printed `✗ the generated shim did not build`
  before the `char*` fix and build after it. That makes it the likely cause, but
  it was not isolated and is not claimed as certain.
- **clipper2** was never broken. It built the whole time; the harness could not
  see its module, and the *escalation notice* for the constexpr budget — a
  notice, not an error — was read as the cause. That is the case that shows what
  the harness bug was doing.

**tinyobjloader was a real bug, two translation units deep.** Its row read
`reflection-tu-failed`, which was accurate as far as it went: reflection
discovery fails on those headers and the CLI falls back to scanning header text.
What the truncated `detail` field hid is what happens next.

A text scan reports **unqualified** names, because it cannot see the
preprocessor or resolve a scope. The CLI has had a fix for that since
nlohmann-json — it emits `using namespace tinyobj;` ahead of the generated
binding so those names resolve — and **only the Python, Lua and JavaScript
binding file got it.** So:

```
Discovered 26 classes (text scan)
..._csharp_gen.cpp:9:23: error: unknown type name 'texture_option_t'
```

Adding the directives to the C# generator source moved the error exactly one
stage later, which is the useful kind of failure:

```
..._shim.cpp:616:22: error: unknown type name 'texture_option_t'
```

The shim is a **translation unit of its own**, compiled without those
directives, and the class names it contains are the same unqualified ones. Only
the emitter can write the shim's preamble, so the namespaces have to travel
there: `Emitter::add_using`, called once per namespace by the CLI and only on
the text-scan path, since reflection reports names in full and there would be
nothing to open.

With both stages fixed, tinyobjloader **binds**: 26 classes, with `ObjReader`,
`attrib_t` and `shape_t` — the API its manifest demands — all present.

Worth noting what that does and does not mean. A hand run that *succeeds* at
reflection discovery finds 17 classes and fails at the **link** with
`undefined reference to tinyobj::ObjReader::ParseFromFile`, because the library
is not header-only and its manifest gives no `link_args`. The text-scan path
binds 26 and links, because the methods that would have needed those symbols
are declined for other reasons. Two paths, two different subsets, both over the
manifest's bar — and a reminder that "binds" is a claim about a class list, not
about a library.

The investigation also surfaced a classifier gap, fixed on its own merits: there
was no rule for `undefined reference`, so clang's "linker command failed with
exit code 1" matched the generic `\berror\b` rule, and the
`text-scan → reflection-tu-failed` override then replaced even that. A link
failure *proves* discovery and codegen both worked, so the override no longer
applies to it. This matters for C# specifically, because `--no-undefined` makes
the case reachable at **build** time rather than at import.

**pmp-library is a genuine generator-stage limit, and it is the one real thing
left on this list.** It exhausts the constexpr budget and still fails after the
escalation to `-fconstexpr-steps=2000000000`, with the namespace fix above in
place. The C# generator TU is heavier than a Python binding TU — it instantiates
an `Emitter` method per class rather than a binding per class — so it is the
stage most exposed to this, and 32 classes over Eigen is what exposes it.

### How this number was wrong, and in which direction

The first run of this said **0 of 7**, and that was a statement about one line
in the harness rather than about the backend:

```python
module = os.path.join(out, lib["module"] + ".so")   # C# writes lib<module>.so
```

`module_built` was therefore False for every C# library whatever the build did,
and every row then went down `classify_failure`, which read *warnings* out of
the output and published them as the cause. Two of the four "compile errors"
that produced were real, diagnosed and fixed. The other two were libraries that
had already built.

The lesson is not "measure"; it is that a measurement harness is code and needs
its own test. `tests/compatibility_matrix/test_classify.py` now pins the
artifact name for all four backends, including the property that made the bug
invisible: no other backend shares C#'s spelling.

Two real bugs did come out of it, both of which took a whole module down:

### A mutable `char*` is an out-buffer, not a string

`core::StringLike` admits `char*` as well as `const char*`. A mutable `char*` is
not a string, though — it is somewhere to write — and nothing in this backend
can marshal one. pugixml spells its character type `char_t`, which is `char`,
and declares

```cpp
size_t evaluate_string(char_t* buffer, size_t capacity, const xpath_node&) const;
```

so the generator read `buffer` as a string parameter, emitted `const char*` for
it on the wire, and passed that to a `char*`. The shim did not compile, which
means pugixml lost *every* method, not this one.

The C# classifier asks `WireString` now, which is `StringLike` minus mutable
`char*`, at all eight sites that used to ask `StringLike` — including the
container element and map-half predicates, where a `vector<char*>` would
otherwise have reached the wire as a poison spelling.

The const has to be tested **before** `remove_cv`, because `remove_cv` erases
the very thing being asked about: `const char*` keeps its const under
`remove_cvref_t` (the const is on the pointee), while `char* const` correctly
loses it and is refused. The Python backend learned this from the same library;
this is the C# half of the same lesson, which is the characteristic shape of
this project's bugs — one thing classified in two places.

Run it against a **copy** of the tree, not the live mount: the dev container
sees the working directory, so an edit made while a corpus run is in flight is
a result about neither version.

## What has never been measured

State these as unknowns, not as positions.

- **Trimming and NativeAOT behaviour.** Neither was run; this machine has no
  .NET SDK. The *verified* claim is narrower and still useful: the generated C#
  contains no `typeof`, `Activator`, `GetType`, `System.Reflection`,
  `Enum.Parse` or `dynamic`, so nothing in it is `RequiresUnreferencedCode` or
  `RequiresDynamicCode` and it cannot produce IL2026 or IL3050 on its own.
- **Whether `DllImport` works under NativeAOT.** Microsoft's own pages conflict:
  the P/Invoke source-generation page says the runtime IL stub is not available
  for AOT or trimming; the NativeAOT limitations list omits `DllImport` entirely
  and the AOT interop page documents it binding lazily with `DirectPInvoke` as
  the eager opt-in. No primary source says which marshalling shapes ILC
  pre-generates versus rejects.
- **Whether a managed thread stays on one OS thread.** The error slot caches a
  native thread-local address in a `[ThreadStatic] IntPtr` and then does a plain
  `Marshal.ReadInt32` per call, which is sound only if that holds. The sources
  found cut the other way. If it ever breaks, the failure is a silent
  stale-pointer read.
- **Any performance number on CoreCLR.** The only figures in the repo are under
  Mono.
- **Whether the MSVC shim compile works.** Never attempted.

## Open questions worth asking a .NET insider

1. Is managed-thread-to-OS-thread affinity a guarantee on CoreCLR today, and
   what does runtime async do to it? Would `AsyncLocal` or a per-call
   out-parameter be the safer construction?
2. Which page is authoritative on `DllImport` under NativeAOT? Concretely: does a
   blittable `DllImport` with a SafeHandle parameter and `LPUTF8Str` strings
   publish clean under `PublishAot` on .NET 10, or is `LibraryImport` genuinely
   load-bearing there?
3. Is forcing `AllowUnsafeBlocks` on consumers acceptable for a *generated*
   binding? It is the difference between `ReadOnlySpan<T>` over the native buffer
   and a per-element loop.
4. Would an opt-in PascalCase mapping change your view of the binding, or is
   verbatim C++ spelling acceptable for generated code? The mapping is derivable,
   so this is policy, not cost.
5. For shipping: one meta-package over per-RID native packages under
   `runtimes/{rid}/native` — still the right shape, given the .NET 8 change to
   how compatible RIDs are computed from `deps.json`?
6. Is there any first-party path for carrying native error *type and message*
   across a C ABI boundary that we have missed?
