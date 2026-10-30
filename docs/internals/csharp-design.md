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
- **A class whose destructor is not public is refused wholesale.** That is safe
  but costly: `tinyxml2::XMLElement` and box2d's bodies are freed by an owning
  pool, and `XMLElement` is most of what tinyxml2 is for. Binding such a class
  **borrow-only** — no constructor, no deleter, reachable only as a member or
  return view — is what these libraries need. It is an ownership-model change
  where a mistake is a double free rather than a compile error.
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
