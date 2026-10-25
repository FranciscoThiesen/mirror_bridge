# V8 Direct API Tests

This directory contains tests for Mirror Bridge's **direct V8 bindings**, which are
**separate from Node.js N-API bindings**.

## When to Use V8 Direct API

Use `mirror_bridge_v8.hpp` when:
- Embedding V8 in a custom application (not Node.js)
- Building Chrome extensions or Chromium-based applications
- Creating custom JavaScript runtimes
- Using V8 in game engines or other embedded contexts

For Node.js addons, use `mirror_bridge_javascript.hpp` instead.

## Test Structure

Each V8 test consists of:

```
test_name/
├── main.cpp      # C++ executable that embeds V8 and runs tests
├── binding.cpp   # Mirror Bridge V8 bindings for the C++ class
├── *.hpp         # The C++ class to bind (e.g., vec3.hpp)
└── test.js       # JavaScript tests executed by the embedded V8 engine
```

## Test Parity with Node.js

These tests mirror the Node.js tests in `tests/js/` to ensure both JavaScript
binding implementations have equivalent functionality:

| V8 Test | Node.js Test | Feature Coverage |
|---------|--------------|------------------|
| `basic/point2d/` | `js/basic/point2d/` | Basic struct binding, properties |
| `static_method_test/` | `js/static_method_test/` | Static methods, const ref params |
| `comprehensive/calculator/` | `js/comprehensive/calculator/` | All method types, multiple params |

## Running Tests

**These tests do not currently build.** They and `v8_test_harness.hpp` target the
V8 8.x+ API, while the dev image ships V8 7.8 by way of `libnode-dev`
(`/usr/include/node/v8.h`). `./tests/run_all_tests.sh` attempts them, because it
gates on that path, and reports the failures without failing the run. The
`test-v8` CI job gates on `/usr/include/v8.h`, which no image provides, so it
skips instead.

Manual compilation:
```bash
cd tests/v8/basic/point2d
clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ \
    -I../../../.. -I. \
    main.cpp -o test_point2d \
    -lv8 -lv8_libplatform
./test_point2d test.js
```

## Requirements

- V8 development files (`libv8-dev` on Ubuntu/Debian)
- Reflection-enabled C++ compiler (clang-p2996 or GCC with reflection)
