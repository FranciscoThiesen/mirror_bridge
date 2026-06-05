# Error Catalog

Paste your error, find your fix. Each entry below pairs the exact message you see with its cause and the precise command or code change that resolves it.

Most issues come from building **outside** the clang-p2996 container. If something fails, first confirm you are inside the Docker environment (`./start_dev_container.sh`) — that single step eliminates the majority of toolchain and linking problems.

## Compile-time errors

### Reflection not available (`<meta>` missing)

**Symptom**

```
error: no member named 'meta' in namespace 'std'
fatal error: 'meta' file not found
```

**Cause** The compiler does not implement C++26 reflection (P2996), so the `<meta>` header and `std::meta::` API are absent.

**Fix** Build with Bloomberg's clang-p2996 and request reflection explicitly. Inside the container this is the default; manually:

```bash
clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ binding.cpp ...
```

### Reflection feature-test macro version too old

**Symptom**

```
error: "This library requires C++26 reflection (P2996) from 2023-06 or later"
```

**Cause** `__cpp_reflection` is defined but reports a value below `202306L`, meaning the compiler implements a pre-P2996-revision of reflection that mirror_bridge does not support.

**Fix** Update clang-p2996 to a current `p2996` branch build (the container ships one). Rebuild the image with `docker rmi mirror_bridge:latest && ./start_dev_container.sh` if you are on an old image.

### Reflection feature-test macro undefined (warning)

**Symptom**

```
warning: "Compiler does not define __cpp_reflection feature-test macro. Reflection support is experimental and may be incomplete."
```

**Cause** The experimental clang-p2996 build does not always set `__cpp_reflection`. This is expected and harmless — mirror_bridge falls back to detecting reflection via the `<meta>` header.

**Fix** None needed. If `<meta>` compiles, reflection works. Suppress with `-Wno-#warnings` only if the warning is noisy in your build logs.

### Unconvertible member type (`bind_class` / `MIRROR_BRIDGE_VALIDATE`)

**Symptom**

```
error: static assertion failed: bind_class<T>: T contains members with types that mirror_bridge cannot convert. Mark unconvertible members with [[=exclude{}]] or add a custom type converter. Supported: arithmetic, std::string, containers, smart pointers, std::optional, std::expected, enums, and nested bindable classes.
```

**Cause** One of the class's data members has a type mirror_bridge does not know how to marshal (e.g. a raw `void*`, a third-party handle, a `std::mutex`, or a non-bindable nested type).

**Fix** Either exclude the offending member from binding, or provide a custom converter. To exclude:

```cpp
using mirror_bridge::exclude;

struct MyClass {
    int value;
    [[=exclude{}]] std::mutex lock;   // not bound
};
```

To pinpoint *which* member fails before reaching `bind_class`, drop in the validator at the definition site:

```cpp
MIRROR_BRIDGE_VALIDATE(MyClass);   // static_assert with the same message, closer to the type
```

### Trampoline does not derive from the bound type

**Symptom**

```
error: static assertion failed: Trampoline must derive from T (or be T itself)
```

**Cause** `bind_class<T, Trampoline>` was instantiated with a `Trampoline` type that is not `T` and is not derived from `T`, so virtual-override forwarding cannot work.

**Fix** Use the trampoline class emitted by `mirror_bridge generate --trampolines` (it derives from `T` and overrides every virtual), or pass no trampoline at all to bind without Python-side virtual overrides:

```cpp
mirror_bridge::python::bind_class<MyBase>(module, "MyBase");                  // no overrides
mirror_bridge::python::bind_class<MyBase, MyBaseTrampoline>(module, "MyBase"); // with overrides
```

### Unsupported container shape

**Symptom**

```
error: static assertion failed: Container must support indexing, push_back, or insert
```

**Cause** A member is container-like but exposes none of the insertion APIs mirror_bridge writes into (`operator[]`, `push_back`, or `insert`), so values cannot be deserialized back into it from the target language.

**Fix** Use a standard sequence/associative container (`std::vector`, `std::deque`, `std::list`, `std::set`, `std::map`, `std::array`). For a custom container, exclude it with `[[=exclude{}]]` (after `using mirror_bridge::exclude;`) or wrap it behind a getter/setter that uses a supported type.

### `uint8_t` / `unsigned char` size mismatch

**Symptom**

```
error: static assertion failed: uint8_t and unsigned char must be same size for byte conversion
```

**Cause** mirror_bridge's fast `vector<unsigned char>` → Python `bytes` path assumes `sizeof(uint8_t) == sizeof(unsigned char)`. This only fails on exotic platforms where that invariant is broken.

**Fix** Use a supported platform/toolchain (the container's x86-64/aarch64 Linux build satisfies this). There is no per-project workaround; the byte-conversion fast path is unavailable on a platform that fails this assert.

### `fmt` 8.1 `FMT_STRING` consteval failure

**Symptom**

```
error: call to consteval function 'fmt::...' is not a constant expression
.../fmt/format-inl.h: in bigint formatting: FMT_STRING("{:x}")
```

**Cause** fmt 8.1's `FMT_STRING` compile-time format checks do not survive consteval evaluation under clang-p2996. The failing calls are only reachable from fmt's debug bigint-printing path, but they break header-only compilation.

**Fix** The provided Docker image already patches this. If you build fmt yourself, either upgrade to fmt ≥ 9, or apply the same replacement that the Dockerfile uses:

```bash
sed -i \
  -e 's|format_to(out, FMT_STRING("{:x}"), value)|format_to(out, fmt::string_view("{:x}"), value)|' \
  -e 's|format_to(out, FMT_STRING("{:08x}"), value)|format_to(out, fmt::string_view("{:08x}"), value)|' \
  -e 's|format_to(out, FMT_STRING("p{}"),|format_to(out, fmt::string_view("p{}"),|' \
  /usr/include/fmt/format-inl.h
```

## Environment & toolchain

### `libc++.so.1` not found

**Symptom**

```
error while loading shared libraries: libc++.so.1: cannot open shared object file: No such file or directory
```

**Cause** The reflection-enabled libc++ is not on the dynamic linker's search path.

**Fix** Add the libc++ directory to `LD_LIBRARY_PATH`. The container and test scripts set this automatically; manually:

```bash
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
# On some images the path is arch-qualified:
export LD_LIBRARY_PATH=/usr/local/lib/aarch64-unknown-linux-gnu:/usr/local/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH
```

### No C++ compiler found when running tests

**Symptom**

```
Error: No C++ compiler found (clang++ or g++)
This script must run inside a mirror_bridge Docker container.
```

**Cause** `tests/run_all_tests.sh` could not find a reflection-capable `clang++` (or `g++`) on `PATH`, which almost always means you are running on the host rather than inside the container.

**Fix** Enter the container first:

```bash
./start_dev_container.sh
cd /workspace
./tests/run_all_tests.sh
```

### Compiler may not support reflection (warning)

**Symptom**

```
Warning: Compiler may not support C++26 reflection
This script requires a reflection-enabled compiler.
Continue anyway? [y/N]
```

**Cause** The test runner found a `clang++` whose `--version` string does not mention `reflection`, `p2996`, or `bloomberg`, so it cannot confirm reflection support.

**Fix** Run inside the mirror_bridge container where `clang++` is the clang-p2996 build. If you intentionally point `CXX` at a custom clang-p2996 build whose version string is non-standard, answer `y` to proceed.

### `pip install --break-system-packages` rejected (Ubuntu 22.04)

**Symptom**

```
Usage:
  pip install [options] <requirement specifier> [package-index-options] ...
no such option: --break-system-packages
```

**Cause** `--break-system-packages` (PEP 668) only exists on pip ≥ 23. Ubuntu 22.04 ships pip 22.0.2, which does not recognize the flag.

**Fix** Drop the flag — on pip 22 it is unnecessary because the environment is not externally managed:

```bash
pip3 install --no-cache-dir <packages>
```

If you are on a newer pip that *does* require it (PEP 668 environments), prefer a virtualenv over the flag.

### Missing `wayland-scanner` / Xorg headers building GUI deps from source

**Symptom**

```
Could NOT find Wayland (missing: Wayland_scanner)
fatal error: X11/Xlib.h: No such file or directory
```

**Cause** Building GUI-adjacent dependencies from source (e.g. Open3D's visualization layer) needs the Wayland scanner and Xorg development headers, which are not present in a minimal base image.

**Fix** Install the development packages (already baked into the mirror_bridge image):

```bash
apt-get install -y xorg-dev libwayland-dev libwayland-bin libxkbcommon-dev libglu1-mesa-dev libosmesa6-dev
```

## Linking & runtime

### Unresolved `std::__cxx11::` symbols on import

**Symptom**

```
ImportError: .../real_open3d.so: undefined symbol: _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE...
undefined symbol: std::__cxx11::basic_string<...>
```

**Cause** You linked your libc++ binding against an external library (e.g. Open3D) whose vendored 3rdparty deps were compiled with **libstdc++**. The resulting `.so` carries unresolved `std::__cxx11::*` symbols. libc++ and libstdc++ use *different* symbol namespaces (`std::__1::` vs `std::__cxx11::`), so they coexist safely in one process — the loader just needs somewhere to find the libstdc++ symbols.

**Fix** Preload libstdc++ so the ELF loader can satisfy the missing symbols at import time, without changing your binding's ABI:

```bash
LD_PRELOAD=libstdc++.so.6 python3 your_script.py
```

This is exactly what `examples/open3d-runtime/build_and_test.sh` does. The cleaner long-term fix is to forward `-stdlib=libc++` into the external library's 3rdparty builds (see the Open3D fork's `CMAKE_CXX_FLAGS` patch) so the whole stack uses a single stdlib.

### Module imports but a method/field is missing after editing the header

**Symptom**

```
AttributeError: 'MyClass' object has no attribute 'new_method'
```

**Cause** mirror_bridge's change detection skipped recompilation because it judged the header unchanged, so the stale `.so` is still on disk.

**Fix** Force a rebuild:

```bash
mirror_bridge generate src/ --module my_module --lang python --force
```

## Build-system

### CMake 4.x rejects pre-3.5 policy compatibility

**Symptom**

```
CMake Error: Compatibility with CMake < 3.5 has been removed from CMake.
Update the VERSION argument <min> value. Or, use the <min>...<max> syntax
to tell CMake that the project requires at least <min> but has been updated
to work with policies introduced by <max> or earlier.
```

**Cause** pip's `cmake` 4.x removed compatibility with `cmake_minimum_required` values below 3.5, which breaks older third-party `CMakeLists.txt` (e.g. VTK 9.1 inside the Open3D dependency tree).

**Fix** Pin CMake to the 3.x line that mirror_bridge's image uses:

```bash
pip3 install --no-cache-dir 'cmake>=3.24,<4'
```

For a single problematic subproject you cannot pin globally, pass the bypass policy on the configure line:

```bash
cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ...
```

### No header files / no classes found

**Symptom**

```
Error: No header files found in src/
Error: No classes found in src/
```

**Cause** `mirror_bridge generate` scanned the directory but found no `.hpp`/`.h` files, or none of the files contained a bindable `struct`/`class` (or every class was opted out via `// MIRROR_BRIDGE_SKIP` / `// MIRROR_BRIDGE_SKIP_FILE`).

**Fix** Point at the directory that actually contains your headers and confirm they define a `struct`/`class`:

```bash
mirror_bridge generate path/to/headers/ --module my_module --lang python
```

Check that you did not leave a stray `// MIRROR_BRIDGE_SKIP_FILE` at the top of the header.

### Missing required CLI arguments

**Symptom**

```
Error: No module name specified (use --module)
Error: No source directory specified
Error: Directory not found: src/
```

**Cause** A required `generate` argument is missing or points at a nonexistent path.

**Fix** Supply both the source directory and `--module`, and verify the path exists:

```bash
mirror_bridge generate src/ --module my_module --lang python
```

### Config-file generation not available

**Symptom**

```
Error: Config file generation not available
```

**Cause** The `mirror_bridge config` command could not locate the config-driven generator backend (the supporting script/binary is absent from the install).

**Fix** Use `mirror_bridge generate` with explicit flags instead of a `.mirror` file, or reinstall/rebuild from a complete checkout so the config backend is present.
