// Test program that generates Rust FFI bindings and writes them to stdout.
// This verifies the code generation without requiring a Rust compiler.

#include "rust/mirror_bridge_rust.hpp"
#include "rust_class.hpp"
#include <iostream>

int main() {
    // Generate bindings for Vec2
    auto vec2_bindings = mirror_bridge::rust::generate_bindings<Vec2>("Vec2");

    std::cout << "=== Vec2 C Header ===\n";
    std::cout << vec2_bindings.c_header << "\n";

    std::cout << "=== Vec2 Rust Source ===\n";
    std::cout << vec2_bindings.rust_source << "\n";

    // Generate bindings for Config (includes string member)
    auto config_bindings = mirror_bridge::rust::generate_bindings<Config>("Config");

    std::cout << "=== Config C Header ===\n";
    std::cout << config_bindings.c_header << "\n";

    std::cout << "=== Config Rust Source ===\n";
    std::cout << config_bindings.rust_source << "\n";

    // Verify key content
    bool ok = true;

    // Vec2 should have f64 getters
    if (vec2_bindings.rust_source.find("f64") == std::string::npos) {
        std::cerr << "FAIL: Vec2 rust source should contain f64\n";
        ok = false;
    }

    // Config should have String getter for title
    if (config_bindings.rust_source.find("String") == std::string::npos) {
        std::cerr << "FAIL: Config rust source should contain String\n";
        ok = false;
    }

    // Config should have i32 getter for width
    if (config_bindings.rust_source.find("i32") == std::string::npos) {
        std::cerr << "FAIL: Config rust source should contain i32\n";
        ok = false;
    }

    // Should have Drop implementation
    if (vec2_bindings.rust_source.find("impl Drop") == std::string::npos) {
        std::cerr << "FAIL: Should have Drop implementation\n";
        ok = false;
    }

    // Should have extern "C" block
    if (vec2_bindings.rust_source.find("extern \"C\"") == std::string::npos) {
        std::cerr << "FAIL: Should have extern C block\n";
        ok = false;
    }

    // C header should have handle typedef
    if (vec2_bindings.c_header.find("Vec2_handle") == std::string::npos) {
        std::cerr << "FAIL: C header should have Vec2_handle\n";
        ok = false;
    }

    if (ok) {
        std::cout << "\nAll Rust binding generation tests passed!\n";
        return 0;
    } else {
        return 1;
    }
}
