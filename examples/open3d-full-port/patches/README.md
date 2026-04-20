# Open3D fork patches

Two one-file CMake patches that let upstream Open3D build and `dlopen`
cleanly when the top-level build uses clang-p2996 and libc++. None of
the changes touch Open3D's C++ sources or public API.

## `001-cmake-cxx-flags-and-boringssl.patch`

One patch, one file (`3rdparty/find_dependencies.cmake`), two fixes:

1. **Forward `CMAKE_CXX_FLAGS` to `ExternalProject_Add`.** VTK, embree,
   zmq, and the other vendored 3rdparty libraries default to libstdc++
   on Linux even when the top-level build requested `-stdlib=libc++`.
   The resulting `libOpen3D.so` mixes `std::__1::*` (libc++) and
   `std::__cxx11::*` (libstdc++) symbols and fails `dlopen`. Forwarding
   top-level CXX flags fixes this. Linker flags are deliberately *not*
   forwarded because OpenBLAS's Fortran components reject
   `-stdlib=libc++` at link time.

2. **`-Wl,--whole-archive` around BoringSSL static libs.** Curl and
   zmq reference SSL entry points that aren't directly reachable from
   Open3D's own `.o` files. The linker drops them, and `dlopen` later
   fails on `X509_INFO_free` and friends. Forcing whole-archive keeps
   every symbol; Open3D's existing version script still hides SSL
   from the `.so`'s dynamic exports.

Both fixes are guarded by `UNIX AND NOT APPLE`, so macOS and Windows
builds are unchanged (they use system SSL and their own linker
semantics).

## Applying

```bash
git clone https://github.com/isl-org/Open3D.git
cd Open3D
git apply /path/to/patches/001-cmake-cxx-flags-and-boringssl.patch
```

Then build Open3D with the mirror_bridge Docker image's clang-p2996
toolchain (`-stdlib=libc++`). `examples/open3d-runtime/` loads the
resulting `libOpen3D.so` and runs tests against it.
