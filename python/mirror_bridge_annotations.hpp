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
// Both supported toolchains implement P3394: Bloomberg's clang-p2996 with
// -freflection-latest, and stock GCC 16+ with -freflection. They spell the
// lookup differently, which read_annotation below is the single place that
// knows about.
//
// ═══════════════════════════════════════════════════════════════════════════

#include <meta>
#include <vector>
#include <type_traits>

// ---------------------------------------------------------------------------
// Which spelling of P3394 this standard library has
// ---------------------------------------------------------------------------
//
// P3394 reached the two implementations under different names and nothing else
// about it differs:
//
//   libc++  (clang-p2996)  std::meta::annotation_of_type<T>(info)
//                              -> std::optional<T>
//   libstdc++ (GCC 16+)    std::meta::annotations_of_with_type(info, info)
//                              -> a range of reflections
//
// The name belongs to the standard library rather than to the compiler, so the
// test keys on the library macro. Neither name can be probed with a requires
// expression: `std::meta::` is not dependent, so a qualified name that does
// not exist is a hard error at parse time rather than a substitution failure.
// That leaves the preprocessor, and overriding it from the command line for a
// toolchain that pairs differently:
//
//   -DMIRROR_BRIDGE_P3394_OPTIONAL_SPELLING=0
//
#if !defined(MIRROR_BRIDGE_P3394_OPTIONAL_SPELLING)
  #if defined(_LIBCPP_VERSION)
    #define MIRROR_BRIDGE_P3394_OPTIONAL_SPELLING 1
  #else
    #define MIRROR_BRIDGE_P3394_OPTIONAL_SPELLING 0
  #endif
#endif

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

namespace detail {

// Both supported toolchains have P3394. Kept as a named constant because it is
// what a diagnostic or a test should ask, rather than re-deriving the macro.
inline constexpr bool p3394_available = true;

} // namespace detail

// ============================================================================
// Annotation Detection Helpers
// ============================================================================

namespace annotations {

// The one place the two spellings differ.
template<typename Annotation>
consteval bool has_annotation(std::meta::info member) {
#if MIRROR_BRIDGE_P3394_OPTIONAL_SPELLING
    return std::meta::annotation_of_type<Annotation>(member).has_value();
#else
    // The range libstdc++ returns owns a heap allocation, so it has to be
    // consumed inside this evaluation. Returning it, or binding it to a
    // constexpr variable, makes the result "a result of operator new" and it
    // stops being a constant expression.
    return !std::meta::annotations_of_with_type(member, ^^Annotation).empty();
#endif
}

// An anonymous union is a nonstatic data member with no name. identifier_of
// has a precondition rather than a fallback, so letting one through made
// get_visible_member_name ill-formed and the enclosing consteval call stopped
// being a constant expression -- the module failed to compile over
// ImGuiStoragePair's `union { int val_i; float val_f; void* val_p; }`.
// Everything below filters through here so the index space, the count and the
// names cannot disagree.
template<typename T>
consteval std::vector<std::meta::info> named_members_of() {
    std::vector<std::meta::info> out;
    for (auto m : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current())) {
        if (std::meta::has_identifier(m)) out.push_back(m);
    }
    return out;
}

// Check if a member has the 'exclude' annotation
template<std::meta::info Member>
consteval bool is_excluded() {
    return has_annotation<exclude>(Member);
}

// Check if a member has the 'readonly' annotation
template<std::meta::info Member>
consteval bool is_readonly() {
    return has_annotation<readonly>(Member);
}

// Count the number of non-excluded data members for a type
template<typename T>
consteval std::size_t count_visible_members() {
    std::size_t count = 0;
    for (auto member : named_members_of<T>()) {
        if (!has_annotation<exclude>(member)) ++count;
    }
    return count;
}

// Get the Nth visible (non-excluded) member
template<typename T>
consteval std::meta::info get_visible_member(std::size_t visible_index) {
    auto members = named_members_of<T>();
    std::size_t visible_count = 0;
    for (auto member : members) {
        if (!has_annotation<exclude>(member)) {
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
    return has_annotation<readonly>(member);
}

// Get the name of the Nth visible member
template<typename T, std::size_t VisibleIndex>
consteval const char* get_visible_member_name() {
    constexpr auto member = get_visible_member<T>(VisibleIndex);
    return std::meta::identifier_of(member).data();
}

} // namespace annotations
} // namespace mirror_bridge
