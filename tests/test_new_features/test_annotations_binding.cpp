// Test: P3394 Annotations Integration with Python Bindings
//
// This test verifies that the P3394 annotations work correctly with
// bind_class to exclude fields and make fields read-only.

#include "../../mirror_bridge.hpp"
#include <string>
#include <iostream>

// Import annotation types for clean syntax
using mirror_bridge::exclude;
using mirror_bridge::readonly;

// ============================================================================
// Test Struct with Annotations
// ============================================================================

struct UserProfile {
    int user_id = 0;
    std::string username;
    std::string email;

    // Excluded from Python binding
    [[=exclude{}]] std::string password_hash;
    [[=exclude{}]] int internal_flag = 0;

    // Read-only in Python (getter only, no setter)
    [[=readonly{}]] std::string created_at;
    [[=readonly{}]] int version = 1;

    // Method to set readonly fields from C++
    void set_created_at(const std::string& ts) { created_at = ts; }
    void set_password(const std::string& pw) { password_hash = pw; }
};

// ============================================================================
// Python Module
// ============================================================================

MIRROR_BRIDGE_MODULE(test_annotations_binding,
    mirror_bridge::bind_class<UserProfile>(m, "UserProfile");
)
