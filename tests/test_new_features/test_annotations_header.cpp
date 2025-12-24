// Test: mirror_bridge_annotations.hpp compilation
//
// Verifies the annotations header compiles and detects annotations correctly.

#include "../../python/mirror_bridge_annotations.hpp"
#include <iostream>

// ============================================================================
// Test Struct with Annotations
// ============================================================================

struct TestUser {
    int id;
    std::string name;

    [[=mirror_bridge::exclude{}]] std::string password_hash;
    [[=mirror_bridge::exclude{}]] int internal_flag;

    [[=mirror_bridge::readonly{}]] std::string created_at;
    [[=mirror_bridge::readonly{}]] int version;
};

// ============================================================================
// Main
// ============================================================================

int main() {
    using namespace mirror_bridge::annotations;

    std::cout << "=== Mirror Bridge Annotations Header Test ===\n\n";

    // Test visibility count
    constexpr std::size_t total_members = 6;
    constexpr std::size_t visible_members = count_visible_members<TestUser>();

    std::cout << "TestUser:\n";
    std::cout << "  Total members: " << total_members << "\n";
    std::cout << "  Visible members: " << visible_members << "\n\n";

    // Check expected values
    static_assert(visible_members == 4, "Expected 4 visible members (6 total - 2 excluded)");

    // Test individual member annotations using visible index
    std::cout << "Visible members (by visible index):\n";

    // Index 0: id
    std::cout << "  [0] " << get_visible_member_name<TestUser, 0>()
              << (is_visible_member_readonly<TestUser, 0>() ? " [READONLY]" : "")
              << "\n";

    // Index 1: name
    std::cout << "  [1] " << get_visible_member_name<TestUser, 1>()
              << (is_visible_member_readonly<TestUser, 1>() ? " [READONLY]" : "")
              << "\n";

    // Index 2: created_at (should be readonly)
    std::cout << "  [2] " << get_visible_member_name<TestUser, 2>()
              << (is_visible_member_readonly<TestUser, 2>() ? " [READONLY]" : "")
              << "\n";

    // Index 3: version (should be readonly)
    std::cout << "  [3] " << get_visible_member_name<TestUser, 3>()
              << (is_visible_member_readonly<TestUser, 3>() ? " [READONLY]" : "")
              << "\n";

    // Verify readonly detection
    static_assert(!is_visible_member_readonly<TestUser, 0>(), "id should not be readonly");
    static_assert(!is_visible_member_readonly<TestUser, 1>(), "name should not be readonly");
    static_assert(is_visible_member_readonly<TestUser, 2>(), "created_at should be readonly");
    static_assert(is_visible_member_readonly<TestUser, 3>(), "version should be readonly");

    std::cout << "\n=== All Tests Passed ===\n";

    return 0;
}
