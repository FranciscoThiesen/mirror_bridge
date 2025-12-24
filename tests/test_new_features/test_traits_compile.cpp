// Test: Compile-time verification of traits detection
// This test verifies that the traits system correctly detects in-class markers
// at compile time WITHOUT modifying bind_class.

#include "../../python/mirror_bridge_traits.hpp"
#include "test_traits_demo.hpp"
#include <iostream>
#include <cassert>

// ============================================================================
// Compile-Time Tests: These are static_asserts that verify traits detection
// ============================================================================

// Test 1: UserProfile has in-class exclude marker
static_assert(mirror_bridge::has_exclude_marker<UserProfile>::value,
    "UserProfile should have _mirror_bridge_exclude marker");

// Test 2: UserProfile has in-class readonly marker
static_assert(mirror_bridge::has_readonly_marker<UserProfile>::value,
    "UserProfile should have _mirror_bridge_readonly marker");

// Test 3: ImageData has exclude marker
static_assert(mirror_bridge::has_exclude_marker<ImageData>::value,
    "ImageData should have _mirror_bridge_exclude marker");

// Test 4: ThirdPartyVector3 does NOT have in-class markers (third-party)
static_assert(!mirror_bridge::has_exclude_marker<ThirdPartyVector3>::value,
    "ThirdPartyVector3 should NOT have _mirror_bridge_exclude (it's third-party)");

// Test 5: is_excluded returns true for marked members
static_assert(mirror_bridge::is_excluded<UserProfile>("password_hash"),
    "password_hash should be excluded from UserProfile");

static_assert(mirror_bridge::is_excluded<UserProfile>("internal_id"),
    "internal_id should be excluded from UserProfile");

// Test 6: is_excluded returns false for non-marked members
static_assert(!mirror_bridge::is_excluded<UserProfile>("user_id"),
    "user_id should NOT be excluded from UserProfile");

static_assert(!mirror_bridge::is_excluded<UserProfile>("username"),
    "username should NOT be excluded from UserProfile");

// Test 7: is_readonly returns true for marked members
static_assert(mirror_bridge::is_readonly<UserProfile>("created_at"),
    "created_at should be readonly in UserProfile");

static_assert(mirror_bridge::is_readonly<UserProfile>("user_id"),
    "user_id should be readonly in UserProfile");

// Test 8: is_readonly returns false for non-marked members
static_assert(!mirror_bridge::is_readonly<UserProfile>("username"),
    "username should NOT be readonly in UserProfile");

static_assert(!mirror_bridge::is_readonly<UserProfile>("email"),
    "email should NOT be readonly in UserProfile");

// Test 9: ImageData exclusion
static_assert(mirror_bridge::is_excluded<ImageData>("gpu_handle"),
    "gpu_handle should be excluded from ImageData");

static_assert(!mirror_bridge::is_excluded<ImageData>("width"),
    "width should NOT be excluded from ImageData");


// ============================================================================
// External Traits Test: For third-party classes you can't modify
// ============================================================================

// Define traits externally for ThirdPartyVector3
template<>
struct mirror_bridge::traits<ThirdPartyVector3> {
    static constexpr auto excluded = std::make_tuple("_reserved", "_flags");
    static constexpr auto readonly = std::make_tuple();
    static constexpr auto renamed = std::make_tuple();
};

// Now the external traits should work
static_assert(mirror_bridge::is_excluded<ThirdPartyVector3>("_reserved"),
    "_reserved should be excluded from ThirdPartyVector3 via external traits");

static_assert(mirror_bridge::is_excluded<ThirdPartyVector3>("_flags"),
    "_flags should be excluded from ThirdPartyVector3 via external traits");

static_assert(!mirror_bridge::is_excluded<ThirdPartyVector3>("x"),
    "x should NOT be excluded from ThirdPartyVector3");


// ============================================================================
// Runtime Test Output
// ============================================================================

int main() {
    std::cout << "=== Traits System Compile-Time Tests ===\n\n";

    std::cout << "All static_assert tests passed at compile time!\n\n";

    std::cout << "UserProfile member analysis:\n";
    std::cout << "  user_id:       excluded=" << mirror_bridge::is_excluded<UserProfile>("user_id")
              << ", readonly=" << mirror_bridge::is_readonly<UserProfile>("user_id") << "\n";
    std::cout << "  username:      excluded=" << mirror_bridge::is_excluded<UserProfile>("username")
              << ", readonly=" << mirror_bridge::is_readonly<UserProfile>("username") << "\n";
    std::cout << "  email:         excluded=" << mirror_bridge::is_excluded<UserProfile>("email")
              << ", readonly=" << mirror_bridge::is_readonly<UserProfile>("email") << "\n";
    std::cout << "  password_hash: excluded=" << mirror_bridge::is_excluded<UserProfile>("password_hash")
              << ", readonly=" << mirror_bridge::is_readonly<UserProfile>("password_hash") << "\n";
    std::cout << "  internal_id:   excluded=" << mirror_bridge::is_excluded<UserProfile>("internal_id")
              << ", readonly=" << mirror_bridge::is_readonly<UserProfile>("internal_id") << "\n";
    std::cout << "  created_at:    excluded=" << mirror_bridge::is_excluded<UserProfile>("created_at")
              << ", readonly=" << mirror_bridge::is_readonly<UserProfile>("created_at") << "\n";

    std::cout << "\nThirdPartyVector3 member analysis (external traits):\n";
    std::cout << "  x:         excluded=" << mirror_bridge::is_excluded<ThirdPartyVector3>("x") << "\n";
    std::cout << "  y:         excluded=" << mirror_bridge::is_excluded<ThirdPartyVector3>("y") << "\n";
    std::cout << "  z:         excluded=" << mirror_bridge::is_excluded<ThirdPartyVector3>("z") << "\n";
    std::cout << "  _reserved: excluded=" << mirror_bridge::is_excluded<ThirdPartyVector3>("_reserved") << "\n";
    std::cout << "  _flags:    excluded=" << mirror_bridge::is_excluded<ThirdPartyVector3>("_flags") << "\n";

    std::cout << "\nImageData member analysis:\n";
    std::cout << "  width:      excluded=" << mirror_bridge::is_excluded<ImageData>("width") << "\n";
    std::cout << "  height:     excluded=" << mirror_bridge::is_excluded<ImageData>("height") << "\n";
    std::cout << "  gpu_handle: excluded=" << mirror_bridge::is_excluded<ImageData>("gpu_handle") << "\n";

    std::cout << "\n=== All Tests Passed! ===\n";

    return 0;
}
