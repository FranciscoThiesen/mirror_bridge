# Proof for #23 — class discovery through reflection

All runs inside the clang-p2996 dev container (`mirror_bridge:latest`, clang 21.0.0git
Bloomberg p2996 @ 9ffb96e, aarch64). Third-party sources live in `/proto`, never in the repo.
The GCC section uses `mb-gcc16-ci:latest` (official `gcc:16`, 16.2.0) under qemu/amd64.

`BEFORE` = `main` @ b0a63fa. `AFTER` = this branch.

---

## 1. jsoncpp 1.9.5, unmodified, `include/json`

```
$ cd /proto && git clone --depth 1 --branch 1.9.5 https://github.com/open-source-parsers/jsoncpp.git
```

### BEFORE

```
$ cd /proto/jsoncpp
$ time mirror_bridge generate include/json --module jsoncpp_py --lang python -o /proto/jc_b --keep-generated
...
Scanning for classes...
  ✓ Found: JSON_API in json_features.h
  ✓ Found: JSON_API in reader.h
  ✓ Found: JSON_API::StructuredError in reader.h
  ✓ Found: JSON_API in reader.h
  ✓ Found: JSON_API::JSON_API in reader.h
  ✓ Found: JSON_API in reader.h
  ✓ Found: JSON_API in value.h
  [... 15 more, all JSON_API ...]
  ✓ Found: JSON_API in writer.h

Discovered 23 classes

⚠ 'JSON_API' is the binding name of 20 classes:
      json_features.h:21
      reader.h:36
      ...

real	1m14.859s
BEFORE_EXIT=1
```

Kept binding file (`/proto/jc_b/jsoncpp_py_python_binding.cpp`), 23 lines of it:

```cpp
    mirror_bridge::bind_class_when_bindable<JSON_API>(m, "JSON_API");
    mirror_bridge::bind_class_when_bindable<JSON_API>(m, "JSON_API");
    mirror_bridge::bind_class_when_bindable<JSON_API::StructuredError>(m, "StructuredError");
    mirror_bridge::bind_class_when_bindable<JSON_API>(m, "JSON_API");
```

Compiler verdict: `error: no matching function for call to 'bind_class_when_bindable'` ×20,
`expected unqualified-id` ×1, `too many errors emitted`.

The unbound report named the enums the same way: `JSON_API::TokenType`,
`JSON_API::CZString::DuplicationPolicy`, `JSON_API::Kind`.

### AFTER

```
$ time mirror_bridge generate include/json --module jsoncpp_py --lang python -o /proto/jc_a --keep-generated
...
Scanning for classes...
Planning template instantiations...
  templates: 0 class and 14 function instantiations across 2 templates, 6 free functions,
             66 candidates rejected by the compiler (plan: /proto/jc_a/jsoncpp_py_plan.txt)

  ✓ Found: Json::Exception in value.h
  ✓ Found: Json::RuntimeError in value.h
  ✓ Found: Json::LogicError in value.h
  ✓ Found: Json::StreamWriter in writer.h
  ✓ Found: Json::StreamWriter::Factory in writer.h
  ✓ Found: Json::StreamWriterBuilder in writer.h
  ✓ Found: Json::Writer in writer.h
  ✓ Found: Json::FastWriter in writer.h
  ✓ Found: Json::StyledWriter in writer.h
  ✓ Found: Json::StyledStreamWriter in writer.h
  ✓ Found: Json::Reader in reader.h
  ✓ Found: Json::Reader::StructuredError in reader.h
  ✓ Found: Json::CharReader in reader.h
  ✓ Found: Json::CharReader::Factory in reader.h
  ✓ Found: Json::CharReaderBuilder in reader.h
  ✓ Found: Json::Features in json_features.h
  ✓ Found: Json::StaticString in value.h
  ✓ Found: Json::Path in value.h
  ✓ Found: Json::PathArgument in value.h
  ✓ Found: Json::Value in value.h
  ✓ Found: Json::ValueIteratorBase in value.h
  ✓ Found: Json::ValueIterator in value.h
  ✓ Found: Json::ValueConstIterator in value.h

Discovered 23 classes (reflection)

⚠ 'Factory' is the binding name of 2 classes:
      writer.h:58
      reader.h:266
  A module exposes every class under its unqualified name, so only one
  of these can answer to 'Factory': the last one bound replaces the rest.
  ...

real	1m22.023s
AFTER_EXIT=1
```

Binding file:

```cpp
    mirror_bridge::bind_class_when_bindable<Json::Value>(m, "Value");
    mirror_bridge::bind_class_when_bindable<Json::Reader>(m, "Reader");
    mirror_bridge::bind_class_when_bindable<Json::CharReader::Factory>(m, "Factory");
    ...
    mirror_bridge::templates::bind_member_template<Json::Value, "as", std::string>();
```

Three nested classes (`Json::StreamWriter::Factory`, `Json::Reader::StructuredError`,
`Json::CharReader::Factory`) that the old path never named, and the private ones
(`Json::Value::CZString`, `Json::Value::Comments`) correctly absent. Enums are now
`Json::ValueType`, `Json::CommentPlacement`, `Json::PrecisionType`.

### jsoncpp still does not finish the build, for a reason that is not #23

```
python/mirror_bridge_python.hpp:2167:27: error: static assertion failed:
    Container must support indexing, push_back, or insert
  note: in instantiation of 'mirror_bridge::from_python<Json::Value>' requested here
  note: in instantiation of 'bind_class<Json::StreamWriterBuilder>' requested here
```

`Json::Value` has `begin()`/`end()`, so it satisfies the `Container` concept, but its
`operator[]` overload set is ambiguous for an integer index and it has no `push_back`
or `insert`. Any method taking or returning `Json::Value` by value therefore trips the
hard `static_assert` in the container converter, which `bind_class_when_bindable`
cannot step around. That failure reaches `Json::Value`, `Json::Reader` and
`Json::Writer`, so no subset of the module avoids it.

This is the already-documented "Unsupported container shape" error
(`docs/reference/errors.md`), it is in the Python conversion layer, and it is out of
scope here. Evidence it is untouched by this change: the template-instantiation lines
the planner emits for tinyxml2 (below) are byte-identical before and after.

So for jsoncpp's full header set, how far it gets: discovery correct, plan correct,
code generation correct, compile blocked by the container converter.

### Timing

| | BEFORE | AFTER |
|---|---|---|
| `generate` on `jsoncpp/include/json` | 1m14.859s (exit 1) | 1m22.023s (exit 1) |

+7.2s, about 10%. The reflection pass already ran in both; the extra cost is the class
and enum walk inside the same consteval unit (the discovery TU alone goes 14.6s → 15.7s)
plus 14 member-template instantiations that are now planned, because the planner can
finally match `Json::Value` to a class the module binds.

The 61s in the issue was on different hardware; 1m14.859s is this machine's BEFORE.

---

## 2. A real third-party module that builds and imports

jsoncpp's own `json_features.h`, `config.h`, `allocator.h`, `forwards.h`, `version.h`,
copied unmodified, linked against jsoncpp built as a static library.

```
$ mirror_bridge generate src --module jcfeat --lang python -o linked --force --stubs \
      --link-args "/proto/jclib/libjsoncpp.a"
...
  ✓ Found: Json::Features in json/json_features.h

Discovered 1 classes (reflection)

Generating python bindings...
Compiling python module...
✓ Built: /proto/jc_subset/linked/jcfeat.so
✓ Stubs: /proto/jc_subset/linked/jcfeat.pyi

✓ Successfully built bindings for: python
```

(BEFORE, the same directory: `✓ Found: JSON_API in json/json_features.h`,
`Discovered 1 classes`, then `error: no matching function for call to
'bind_class_when_bindable'`, exit 1.)

```
$ python3 -c "import jcfeat; ..."
type: <class 'Features'>
strictMode allows comments: False
all() allows comments: True
roundtrip: False
attrs: ['all', 'allowComments_', 'allowDroppedNullPlaceholders_', 'allowNumericKeys_',
        'strictMode', 'strictRoot_']
no JSON_API attr: True
```

Generated stub:

```python
class Features:
    def __init__(self) -> None: ...
    allowComments_: bool
    strictRoot_: bool
    allowDroppedNullPlaceholders_: bool
    allowNumericKeys_: bool
    @staticmethod
    def all() -> Features: ...
    @staticmethod
    def strictMode() -> Features: ...
```

---

## 3. A second library: tinyxml2 (`TINYXML2_LIB`)

BEFORE (19.7s, exit 1):

```
  ✓ Found: TINYXML2_LIB in tinyxml2.h
  ✓ Found: MemPool in tinyxml2.h
  ✓ Found: TINYXML2_LIB in tinyxml2.h
  [... 13 more TINYXML2_LIB ...]
```

AFTER (24.8s, exit 1):

```
  ✓ Found: tinyxml2::XMLDocument in tinyxml2.h
  ✓ Found: tinyxml2::XMLElement in tinyxml2.h
  ✓ Found: tinyxml2::XMLAttribute in tinyxml2.h
  ✓ Found: tinyxml2::XMLComment in tinyxml2.h
  ✓ Found: tinyxml2::XMLText in tinyxml2.h
  ✓ Found: tinyxml2::XMLDeclaration in tinyxml2.h
  ✓ Found: tinyxml2::XMLUnknown in tinyxml2.h
  ✓ Found: tinyxml2::XMLPrinter in tinyxml2.h
  ✓ Found: tinyxml2::StrPair in tinyxml2.h
  ✓ Found: tinyxml2::MemPool in tinyxml2.h
  ✓ Found: tinyxml2::XMLVisitor in tinyxml2.h
  ✓ Found: tinyxml2::XMLUtil in tinyxml2.h
  ✓ Found: tinyxml2::XMLNode in tinyxml2.h
  ✓ Found: tinyxml2::XMLHandle in tinyxml2.h
  ✓ Found: tinyxml2::XMLConstHandle in tinyxml2.h

Discovered 15 classes (reflection)
```

tinyxml2's module does not build either, and again not because of discovery:
`'Block' is a private member of 'tinyxml2::MemPoolT<120>'` from the **template planner**,
and `calling a protected destructor of class 'tinyxml2::XMLElement'` from the Python
backend. The template-planner lines are identical before and after:

```
$ diff <(grep bind_instance before/tx_py_python_binding.cpp) \
       <(grep bind_instance after/tx_py_python_binding.cpp)
(no output)
```

---

## 4. The two non-macro cases the coordinator reported

No preprocessor involved at all. BEFORE, on `/proto/align/src/wide.hpp`:

```cpp
struct alignas(64) Wide { double first = 1.5; double last = 2.5; };
struct [[nodiscard]] Tagged { int tag = 11; };
struct Base { int b = 1; };
struct Sealed final : Base { int s = 2; };
```

```
Scanning for classes...
  ✓ Found: alignas in wide.hpp
  ✓ Found: Base in wide.hpp
  ✓ Found: Sealed in wide.hpp

Discovered 3 classes
...
    mirror_bridge::bind_class_when_bindable<alignas>(m, "alignas");
...
error: expected expression
✗ Failed languages: python
```

`alignas` bound as a class, and `Tagged` silently gone. AFTER:

```
  ✓ Found: demo::Wide in wide.hpp
  ✓ Found: demo::Tagged in wide.hpp
  ✓ Found: demo::Base in wide.hpp
  ✓ Found: demo::Sealed in wide.hpp

Discovered 4 classes (reflection)
✓ Built: /proto/align/build2/al.so
```

(`struct Sealed final : Base` was already fine before: `final` comes *after* the name,
so the old regex never mistook it. It is in the fixture as a guard, not as a fix.)

---

## 5. The fallback announces itself

A header that is not self-contained, so the reflection unit cannot compile:

```cpp
#pragma once
// Deliberately does not include <unordered_map>.
namespace demo {
class DEMO_EXPORT_UNUSED_PLACEHOLDER_DUMMY {};
struct Registry {
    std::unordered_map<int, int> entries;
    int size_of() const { return static_cast<int>(entries.size()); }
};
}
```

```
$ mirror_bridge generate src --module nsc --lang python -o build --force
...
Scanning for classes...
Planning template instantiations...
  templates: discovery failed:
  templates: skipped (details: .../plan/nsc/planner.log)
  Templates and free functions not bound (see .../plan/nsc/planner.log); binding classes only.

⚠ Reflection could not read these headers; falling back to a scan of the header text.
  A text scan does not see the preprocessor, so a class declared through an
  export macro (class JSON_API Value) is discovered under the macro's name.
  Why the reflection unit failed: /proto/notselfcontained/build/.mirror_bridge/plan/nsc/planner.log

  ✓ Found: DEMO_EXPORT_UNUSED_PLACEHOLDER_DUMMY in registry.hpp
  ✓ Found: Registry in registry.hpp

Discovered 2 classes (text scan)

Generating python bindings...
Compiling python module...
✓ Built: /proto/notselfcontained/build/nsc.so

✓ Successfully built bindings for: python
```

The text scan still works, the module still builds, and the user is told which list
produced it — `(text scan)` vs `(reflection)` on the Discovered line.

---

## 6. Regression fixture: `tests/export_macro/`

`src/exportlib_config.hpp` defines `EXPORTLIB_API` the way jsoncpp defines `JSON_API`.
`src/exportlib.hpp` declares classes behind it, a public and a private nested class, two
classes whose member aliases point at each other, the `alignas`/`[[nodiscard]]`/`final`
cases, an enum, and one class marked `// MIRROR_BRIDGE_SKIP`.

On `main`, the fixture reproduces the bug:

```
  ✓ Found: EXPORTLIB_API in exportlib.hpp
  ✓ Found: EXPORTLIB_API::EXPORTLIB_API in exportlib.hpp
  ✓ Found: EXPORTLIB_API in exportlib.hpp
  ✓ Found: EXPORTLIB_API in exportlib.hpp
  ✓ Found: alignas in exportlib.hpp
  ✓ Found: EXPORTLIB_API in exportlib.hpp

Discovered 6 classes
...
    mirror_bridge::bind_class_when_bindable<EXPORTLIB_API>(m, "EXPORTLIB_API");
    mirror_bridge::bind_class_when_bindable<EXPORTLIB_API::EXPORTLIB_API>(m, "EXPORTLIB_API");
    mirror_bridge::bind_class_when_bindable<alignas>(m, "alignas");
...
1 warning and 6 errors generated.
✗ Failed languages: python
```

On this branch:

```
$ bash tests/export_macro/build_and_test.sh
  ✓ Found: exportlib::Widget in exportlib.hpp
  ✓ Found: exportlib::Widget::Handle in exportlib.hpp
  ✓ Found: exportlib::WidgetCursor in exportlib.hpp
  ✓ Found: exportlib::WidgetList in exportlib.hpp
  ✓ Found: exportlib::Wide in exportlib.hpp
  ✓ Found: exportlib::Tagged in exportlib.hpp
  ✓ Found: exportlib::Sealed in exportlib.hpp

Discovered 7 classes (reflection)
✓ Built: .../build/exportlib_py.so

  ✓ the class is found under its own name
  ✓ the export macro is not taken for a class
  ✓ a public nested class is found
  ✓ a private nested class is not
  ✓ mutually aliasing classes both appear
  ✓ alignas is not taken for a class name
  ✓ and no class called alignas is bound
  ✓ an attributed class is not lost
  ✓ a final class is found
  ✓ MIRROR_BRIDGE_SKIP is honoured
  ✓ the enum is reported rather than bound
  ✓ the list came from the compiler
✓ export-macro discovery test passed
```

Registered in both harnesses: `add_test(NAME cli_export_macro ...)` in
`tests/CMakeLists.txt`, and `test_export_macro_cli.sh` which `tests/run_all_tests.sh`
picks up in its step-3 glob.

Without the `is_type_alias` guard, `WidgetList::iterator` → `WidgetCursor` →
`WidgetCursor::container` → `WidgetList` is an unbounded recursion: each alias is a
distinct reflection, so the "already seen this class" guard does not catch it. That is
the jsoncpp "constexpr evaluation hit maximum step limit" the issue describes, reduced
to six lines.

---

## 7. Test suites

### clang-p2996, `tests/run_all_tests.sh`, clean tree

This branch:

```
Bindings:  Total: 61   Built: 61
Tests:     Total: 68   Passed: 64
V8 Tests (non-blocking):  Total: 3  Passed: 0  Failed: 3
✓ ALL TESTS PASSED!
EXIT=0
```

`main` @ b0a63fa, same container, same clean-tree procedure:

```
Bindings:  Total: 61   Built: 61
Tests:     Total: 66   Passed: 63
V8 Tests (non-blocking):  Total: 3  Passed: 0  Failed: 3
✓ ALL TESTS PASSED!
EXIT=0
```

The +2 total / +1 passed is the new fixture: `test_export_macro_cli.sh` (runs and passes)
and `test_export_macro.py` (counted, then skipped — the CLI test runs it). The three V8
failures are pre-existing on `main` and non-blocking by design (segfaults in the
embedded-V8 harness, unrelated to discovery).

Individually re-run after the final edits:

```
tests/test_cli_diagnostics.sh      passed: 30   failed: 0
tests/test_cli_api.sh              passed: 33   failed: 0
tests/test_cli_diff.sh             All 10 diff tests passed!
tests/test_cli_watch.sh            All 9 watch tests passed!
tests/test_doctor.sh               All doctor tests passed!
tests/inheritance/build_and_test.sh          All inherited-method tests passed.
tests/template_instantiation/build_and_test.sh   All template instantiation tests passed
```

Note on the harness: running `run_all_tests.sh` twice without deleting `build/` reports
spurious binding failures, because step 1 globs `*.cpp` and the planner's scratch
`discover.cpp` / `probe.cpp` are left behind in `<output>/.mirror_bridge/plan/`. That
behaviour predates this branch (it happens with `tests/template_instantiation` and
`tests/inheritance` on `main` too); the new fixture adds one more such directory.

### Stock GCC 16

`mb-gcc16-ci:latest` = `Dockerfile.gcc-reflection` = official `gcc:16` (16.2.0, Debian
trixie), amd64 under qemu, run on an rsync'd copy of the worktree.

```
$ docker run --rm --platform linux/amd64 -v $PWD:/workspace -w /workspace mb-gcc16-ci \
      bash -lc 'cd tests/export_macro && bash build_and_test.sh'
  ✓ Found: exportlib::Widget in exportlib.hpp
  ✓ Found: exportlib::Widget::Handle in exportlib.hpp
  ✓ Found: exportlib::WidgetCursor in exportlib.hpp
  ✓ Found: exportlib::WidgetList in exportlib.hpp
  ✓ Found: exportlib::Wide in exportlib.hpp
  ✓ Found: exportlib::Tagged in exportlib.hpp
  ✓ Found: exportlib::Sealed in exportlib.hpp

Discovered 7 classes (reflection)
✓ Built: /workspace/tests/export_macro/build/exportlib_py.so
  [all 12 assertions ✓]
✓ export-macro discovery test passed
EXIT=0
```

Same class list, same order, as clang. Full harness on GCC:

```
$ docker run --rm --platform linux/amd64 -v $PWD:/workspace -w /workspace mb-gcc16-ci \
      ./tests/run_all_tests.sh
Bindings:  Total: 59   Built: 59
Tests:     Total: 68   Passed: 64
V8 Tests (non-blocking):  Total: 3  Passed: 0  Failed: 3
✓ ALL TESTS PASSED!
EXIT=0
```

(59 rather than 61 bindings: the two P3394 annotation tests are skipped on GCC, as the
harness already does.)

---

## What is not covered

- **jsoncpp's full module still does not compile**, for the container-converter
  `static_assert` in section 1. Separate bug in the Python conversion layer; not
  attempted here.
- **tinyxml2's module still does not compile**, for a template-planner bug
  (`MemPoolT<N>::Block` is private) and protected destructors. Both predate this branch
  and are byte-identically present before and after.
- **`--lang js` was not run end to end.** `--lang lua` was (it takes the same
  `--discover-only` path and built `exportlib_lua.so` from the fixture); the JS path
  differs only in `compile_binding`, which this branch does not touch.
- **The unbound report says "Python raises AttributeError" even for a Lua module.**
  Pre-existing wording, left alone.
- The `Discovered N classes` line now carries `(reflection)` or `(text scan)`. Anything
  parsing that line for an exact match would need updating; nothing in the repo does.
