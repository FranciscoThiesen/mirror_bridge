#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge Annotations - P3394 Field Annotations for Binding Control
// ═══════════════════════════════════════════════════════════════════════════
//
// This header provides annotation types for controlling binding behavior using
// C++26 P3394 (Annotations for Reflection), accepted at the Sofia meeting.
//
// Usage (recommended - import annotation types for clean syntax):
//
//   using mirror_bridge::exclude;
//   using mirror_bridge::readonly;
//
//   struct UserProfile {
//       int user_id;                              // Bound normally
//       [[=exclude{}]] std::string password_hash; // Not bound to Python
//       [[=readonly{}]] std::string created_at;   // Read-only in Python
//   };
//
// Requires: -freflection-latest with Bloomberg's clang-p2996
//
// For compilers without P3394 support, annotations are ignored and all
// members are bound normally.
//
// ═══════════════════════════════════════════════════════════════════════════

#include <meta>

// ============================================================================
// P3394 Feature Detection
// ============================================================================
//
// P3394 (Annotations for Reflection) adds std::meta::annotation_of_type<T>().
// Currently only Bloomberg's clang-p2996 with -freflection-latest supports it.
// GCC's reflection implementation does not yet support P3394.
//
// We detect P3394 by checking if annotation_of_type is available via SFINAE.

namespace mirror_bridge {
namespace detail {

// Helper to detect P3394 support at compile time
template<typename T, typename = void>
struct has_p3394_support : std::false_type {};

// This specialization will only be valid if annotation_of_type exists
// We use a simple struct for testing
struct p3394_test_marker {
    constexpr bool operator==(const p3394_test_marker&) const = default;
};

#if defined(__clang__) && __has_builtin(__builtin_reflection)
// Clang with reflection support - check if annotation_of_type exists
// by attempting to use it in a requires clause
template<typename T>
struct has_p3394_support<T, std::void_t<
    decltype(std::meta::annotation_of_type<p3394_test_marker>(std::meta::info{}))
>> : std::true_type {};
#endif

// For simplicity, we use a compile-time constant based on compiler detection
// Bloomberg's clang-p2996 with -freflection-latest supports P3394
#if defined(__clang__) && defined(__has_feature)
  #if __has_feature(reflection)
    // Check if this is clang-p2996 with P3394 support
    // The annotation_of_type function is only available with -freflection-latest
    // We detect this by checking if the function exists in std::meta
    inline constexpr bool p3394_available = requires {
        std::meta::annotation_of_type<p3394_test_marker>(std::meta::info{});
    };
  #else
    inline constexpr bool p3394_available = false;
  #endif
#else
    inline constexpr bool p3394_available = false;
#endif

} // namespace detail

// ============================================================================
// Annotation Types
// ============================================================================

// Exclude a field from the binding entirely
// The field will not appear in the Python/Lua/JavaScript binding
struct exclude {
    constexpr bool operator==(const exclude&) const = default;
};

// Make a field read-only in the binding
// The field will have a getter but no setter
struct readonly {
    constexpr bool operator==(const readonly&) const = default;
};

// ============================================================================
// Annotation Detection Helpers
// ============================================================================

namespace annotations {

#if defined(__clang__)
// Clang with P3394 support - use annotation_of_type

// Check if a member has the 'exclude' annotation
template<std::meta::info Member>
consteval bool is_excluded() {
    return std::meta::annotation_of_type<exclude>(Member).has_value();
}

// Check if a member has the 'readonly' annotation
template<std::meta::info Member>
consteval bool is_readonly() {
    return std::meta::annotation_of_type<readonly>(Member).has_value();
}

// Count the number of non-excluded data members for a type
template<typename T>
consteval std::size_t count_visible_members() {
    auto members = std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current());
    std::size_t count = 0;
    for (auto member : members) {
        if (!std::meta::annotation_of_type<exclude>(member).has_value()) {
            ++count;
        }
    }
    return count;
}

// Get the Nth visible (non-excluded) member
template<typename T>
consteval std::meta::info get_visible_member(std::size_t visible_index) {
    auto members = std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current());
    std::size_t visible_count = 0;
    for (auto member : members) {
        if (!std::meta::annotation_of_type<exclude>(member).has_value()) {
            if (visible_count == visible_index) {
                return member;
            }
            ++visible_count;
        }
    }
    // Should not reach here if visible_index is valid
    return members[0];
}

// Check if the Nth visible member is readonly
template<typename T, std::size_t VisibleIndex>
consteval bool is_visible_member_readonly() {
    constexpr auto member = get_visible_member<T>(VisibleIndex);
    return std::meta::annotation_of_type<readonly>(member).has_value();
}

// Get the name of the Nth visible member
template<typename T, std::size_t VisibleIndex>
consteval const char* get_visible_member_name() {
    constexpr auto member = get_visible_member<T>(VisibleIndex);
    return std::meta::identifier_of(member).data();
}

#else
// GCC or other compilers without P3394 - all members are visible, none readonly

// Check if a member has the 'exclude' annotation - always false without P3394
template<std::meta::info Member>
consteval bool is_excluded() {
    return false;  // No P3394 support, can't check annotations
}

// Check if a member has the 'readonly' annotation - always false without P3394
template<std::meta::info Member>
consteval bool is_readonly() {
    return false;  // No P3394 support, can't check annotations
}

// Count the number of non-excluded data members - all members are visible
template<typename T>
consteval std::size_t count_visible_members() {
    return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()).size();
}

// Get the Nth visible member - just return the Nth member directly
template<typename T>
consteval std::meta::info get_visible_member(std::size_t index) {
    return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current())[index];
}

// Check if the Nth visible member is readonly - always false without P3394
template<typename T, std::size_t VisibleIndex>
consteval bool is_visible_member_readonly() {
    return false;  // No P3394 support, can't check annotations
}

// Get the name of the Nth visible member
template<typename T, std::size_t VisibleIndex>
consteval const char* get_visible_member_name() {
    constexpr auto member = get_visible_member<T>(VisibleIndex);
    return std::meta::identifier_of(member).data();
}

#endif // __clang__

} // namespace annotations
} // namespace mirror_bridge
