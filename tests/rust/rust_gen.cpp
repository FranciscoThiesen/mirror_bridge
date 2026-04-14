// Comprehensive Rust binding generation tests.
//
// Verifies:
// 1. Code generation runs without crashing for all test classes
// 2. Generated strings contain correct Rust/C constructs
// 3. Generated C++ implementation actually compiles (via write_bindings + compile)
// 4. Edge cases: empty class, methods-only, Rust keyword members
// 5. Bool ABI mapping (C int ↔ Rust i32, safe wrapper uses bool)
// 6. Keyword escaping produces r#type syntax
// 7. write_bindings produces files on disk

#include "rust/mirror_bridge_rust.hpp"
#include "rust_class.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>

int pass_count = 0;
int fail_count = 0;

void check(bool cond, const char* msg) {
    if (cond) {
        std::cout << "  PASS: " << msg << "\n";
        pass_count++;
    } else {
        std::cerr << "  FAIL: " << msg << "\n";
        fail_count++;
    }
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

bool not_contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) == std::string::npos;
}

// Read a file into a string
std::string read_file(const std::string& path) {
    std::ifstream f(path);
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

int main() {
    std::cout << "Testing Rust binding generation:\n\n";

    // ================================================================
    // Test: Vec2 (basic numeric class with method)
    // ================================================================
    std::cout << "--- Vec2 ---\n";
    auto vec2 = mirror_bridge::rust::generate_bindings<Vec2>("Vec2");

    check(contains(vec2.rust_source, "f64"),
          "Vec2: Rust source contains f64 for double members");
    check(contains(vec2.rust_source, "pub fn length(&self) -> f64"),
          "Vec2: length() method generated with correct signature");
    check(contains(vec2.rust_source, "pub fn x(&self) -> f64"),
          "Vec2: x getter generated");
    check(contains(vec2.rust_source, "pub fn set_x(&mut self, value: f64)"),
          "Vec2: x setter generated");
    check(contains(vec2.rust_source, "impl Drop for Vec2"),
          "Vec2: Drop implementation generated");
    check(not_contains(vec2.rust_source, "unsafe impl Send"),
          "Vec2: no unconditional Send");
    check(not_contains(vec2.rust_source, "unsafe impl Sync"),
          "Vec2: no unconditional Sync");
    check(contains(vec2.rust_source, "extern \"C\""),
          "Vec2: has extern C block");
    check(contains(vec2.c_header, "Vec2_handle"),
          "Vec2: C header has handle typedef");
    check(contains(vec2.c_header, "Vec2_length"),
          "Vec2: C header has length method");
    check(contains(vec2.c_header, "Vec2_get_x"),
          "Vec2: C header has getter");
    check(contains(vec2.c_header, "Vec2_set_x"),
          "Vec2: C header has setter");

    // ================================================================
    // Test: Config (mixed types including bool and string)
    // ================================================================
    std::cout << "\n--- Config ---\n";
    auto config = mirror_bridge::rust::generate_bindings<Config>("Config");

    check(contains(config.rust_source, "-> String"),
          "Config: String return type for title getter");
    check(contains(config.rust_source, "i32"),
          "Config: i32 for int members");
    check(contains(config.rust_source, "-> bool"),
          "Config: safe wrapper returns bool (not i32)");
    check(contains(config.rust_source, "!= 0"),
          "Config: bool getter uses != 0 conversion");
    check(contains(config.rust_source, "as i32"),
          "Config: bool setter uses 'as i32' conversion");
    check(contains(config.rust_source, ".expect("),
          "Config: CString uses .expect() not .unwrap()");
    check(not_contains(config.rust_source, ".unwrap()"),
          "Config: no .unwrap() anywhere");
    check(contains(config.rust_source, "set_title(&mut self, value: &str)"),
          "Config: string setter takes &str");
    check(contains(config.c_header, "const char*"),
          "Config: C header uses const char* for string");

    // ================================================================
    // Test: Empty class
    // ================================================================
    std::cout << "\n--- Empty ---\n";
    auto empty = mirror_bridge::rust::generate_bindings<Empty>("Empty");

    check(contains(empty.rust_source, "pub struct Empty"),
          "Empty: generates valid struct");
    check(contains(empty.rust_source, "pub fn new() -> Self"),
          "Empty: has new() constructor");
    check(contains(empty.rust_source, "impl Drop for Empty"),
          "Empty: has Drop");
    check(contains(empty.c_header, "Empty_create"),
          "Empty: C header has create");
    check(contains(empty.c_header, "Empty_destroy"),
          "Empty: C header has destroy");
    // Should NOT have any getters/setters
    check(not_contains(empty.c_header, "Empty_get_"),
          "Empty: no getters for empty class");
    check(not_contains(empty.c_header, "Empty_set_"),
          "Empty: no setters for empty class");

    // ================================================================
    // Test: MethodsOnly (no data members, just methods)
    // ================================================================
    std::cout << "\n--- MethodsOnly ---\n";
    auto methods_only = mirror_bridge::rust::generate_bindings<MethodsOnly>("MethodsOnly");

    check(contains(methods_only.rust_source, "pub fn compute(&self) -> f64"),
          "MethodsOnly: compute() method generated");
    check(contains(methods_only.rust_source, "pub fn reset(&self)"),
          "MethodsOnly: void reset() method generated");
    check(not_contains(methods_only.c_header, "MethodsOnly_get_"),
          "MethodsOnly: no getters (no data members)");

    // ================================================================
    // Test: HasKeywordMember (Rust keyword escaping)
    // ================================================================
    std::cout << "\n--- HasKeywordMember (Rust keyword escaping) ---\n";
    auto kw = mirror_bridge::rust::generate_bindings<HasKeywordMember>("HasKeywordMember");

    check(contains(kw.rust_source, "pub fn r#type(&self)"),
          "HasKeywordMember: 'type' escaped to r#type in getter");
    check(contains(kw.rust_source, "pub fn set_r#type(&mut self"),
          "HasKeywordMember: 'type' escaped to r#type in setter");
    // The FFI function name should still use the raw C++ name
    check(contains(kw.rust_source, "HasKeywordMember_get_type"),
          "HasKeywordMember: FFI uses raw name (no r#)");
    // value member should NOT be escaped (not a keyword)
    check(contains(kw.rust_source, "pub fn value(&self)"),
          "HasKeywordMember: non-keyword 'value' not escaped");

    // ================================================================
    // Test: write_bindings produces files on disk
    // ================================================================
    std::cout << "\n--- write_bindings file output ---\n";
    const char* outdir = "/tmp/rust_binding_test";
    std::system("mkdir -p /tmp/rust_binding_test");

    bool wrote = mirror_bridge::rust::write_bindings<Vec2>("Vec2", outdir);
    check(wrote, "write_bindings returns true");

    std::string h_content = read_file("/tmp/rust_binding_test/Vec2_ffi.h");
    std::string cpp_content = read_file("/tmp/rust_binding_test/Vec2_ffi.cpp");
    std::string rs_content = read_file("/tmp/rust_binding_test/Vec2.rs");

    check(!h_content.empty(), "write_bindings: .h file is non-empty");
    check(!cpp_content.empty(), "write_bindings: .cpp file is non-empty");
    check(!rs_content.empty(), "write_bindings: .rs file is non-empty");
    check(h_content == vec2.c_header, "write_bindings: .h content matches in-memory");
    check(rs_content == vec2.rust_source, "write_bindings: .rs content matches in-memory");

    // ================================================================
    // Test: generated C++ implementation compiles
    // ================================================================
    std::cout << "\n--- Generated C++ compilation ---\n";

    // Write Vec2 with class_header so the .cpp includes the original header
    auto vec2_with_header = mirror_bridge::rust::generate_bindings<Vec2>("Vec2", "rust_class.hpp");
    {
        std::ofstream f("/tmp/rust_binding_test/Vec2_ffi_full.cpp");
        f << vec2_with_header.cpp_impl;
    }
    {
        std::ofstream f("/tmp/rust_binding_test/Vec2_ffi.h");
        f << vec2_with_header.c_header;
    }

    int compile_result = std::system(
        "clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ "
        "-I/workspace/tests/rust -I/tmp/rust_binding_test "
        "-fPIC -shared -o /tmp/rust_binding_test/libvec2_ffi.so "
        "/tmp/rust_binding_test/Vec2_ffi_full.cpp 2>&1");
    check(compile_result == 0,
          "Generated C++ compiles successfully as shared library");

    // ================================================================
    // Test: generated C header is valid C
    // ================================================================
    std::cout << "\n--- Generated C header compilation ---\n";

    int c_compile = std::system(
        "echo '#include \"/tmp/rust_binding_test/Vec2_ffi.h\"\nint main(){return 0;}' | "
        "cc -xc -fsyntax-only -I/tmp/rust_binding_test - 2>&1");
    check(c_compile == 0,
          "Generated C header compiles as valid C");

    // ================================================================
    // Summary
    // ================================================================
    std::cout << "\n";
    if (fail_count == 0) {
        std::cout << "All " << pass_count << " Rust binding tests passed!\n";
        return 0;
    } else {
        std::cout << fail_count << " of " << (pass_count + fail_count) << " tests FAILED\n";
        return 1;
    }
}
