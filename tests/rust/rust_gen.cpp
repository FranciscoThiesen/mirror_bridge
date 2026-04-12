// Test program that generates Rust FFI bindings and verifies correctness.

#include "rust/mirror_bridge_rust.hpp"
#include "rust_class.hpp"
#include <iostream>

int main() {
    auto vec2_bindings = mirror_bridge::rust::generate_bindings<Vec2>("Vec2");
    auto config_bindings = mirror_bridge::rust::generate_bindings<Config>("Config");

    std::cout << "=== Vec2 Rust Source ===\n" << vec2_bindings.rust_source << "\n";
    std::cout << "=== Config Rust Source ===\n" << config_bindings.rust_source << "\n";

    bool ok = true;
    auto check = [&](bool cond, const char* msg) {
        if (!cond) { std::cerr << "FAIL: " << msg << "\n"; ok = false; }
    };

    // Vec2 should have f64 getters
    check(vec2_bindings.rust_source.find("f64") != std::string::npos,
          "Vec2 should contain f64");

    // Vec2 should have length() method binding
    check(vec2_bindings.rust_source.find("pub fn length") != std::string::npos,
          "Vec2 should have length() method");

    // Config should have String getter for title
    check(config_bindings.rust_source.find("-> String") != std::string::npos,
          "Config should have String return type");

    // Config should have i32 getter for width
    check(config_bindings.rust_source.find("i32") != std::string::npos,
          "Config should contain i32");

    // Bool: FFI should use i32, safe wrapper should use bool with != 0
    check(config_bindings.rust_source.find("-> bool") != std::string::npos,
          "Config safe wrapper should return bool");
    check(config_bindings.rust_source.find("!= 0") != std::string::npos,
          "Bool getter should use != 0 conversion");
    check(config_bindings.rust_source.find("as i32") != std::string::npos,
          "Bool setter should use 'as i32' conversion");

    // Should have Drop but NOT unconditional Send+Sync
    check(vec2_bindings.rust_source.find("impl Drop") != std::string::npos,
          "Should have Drop implementation");
    check(vec2_bindings.rust_source.find("unsafe impl Send") == std::string::npos,
          "Should NOT have unconditional Send impl");
    check(vec2_bindings.rust_source.find("unsafe impl Sync") == std::string::npos,
          "Should NOT have unconditional Sync impl");

    // CString should use .expect() not .unwrap()
    check(config_bindings.rust_source.find(".expect(") != std::string::npos,
          "CString should use .expect() not .unwrap()");
    check(config_bindings.rust_source.find(".unwrap()") == std::string::npos,
          "Should not contain .unwrap()");

    // Should have extern "C" block
    check(vec2_bindings.rust_source.find("extern \"C\"") != std::string::npos,
          "Should have extern C block");

    // C header should have handle typedef
    check(vec2_bindings.c_header.find("Vec2_handle") != std::string::npos,
          "C header should have Vec2_handle");

    // Method should appear in C header too
    check(vec2_bindings.c_header.find("Vec2_length") != std::string::npos,
          "C header should have Vec2_length method");

    if (ok) {
        std::cout << "\nAll Rust binding generation tests passed!\n";
        return 0;
    }
    return 1;
}
