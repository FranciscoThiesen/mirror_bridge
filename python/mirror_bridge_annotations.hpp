#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge Annotations - P3394 Field Annotations for Binding Control
// ═══════════════════════════════════════════════════════════════════════════
//
// This header provides annotation types for controlling binding behavior using
// C++26 P3394 (Annotations for Reflection), accepted at the Sofia meeting.
//
// Usage:
//   struct UserProfile {
//       int user_id;                                      // Bound normally
//       [[=mirror_bridge::exclude{}]] std::string password_hash;  // Not bound
//       [[=mirror_bridge::readonly{}]] std::string created_at;    // Read-only
//   };
//
// Requires: -freflection-latest with Bloomberg's clang-p2996
//
// ═══════════════════════════════════════════════════════════════════════════

#include <meta>

namespace mirror_bridge {

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

} // namespace annotations
} // namespace mirror_bridge
