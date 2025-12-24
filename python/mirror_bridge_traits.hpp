#pragma once

// ============================================================================
// Mirror Bridge - Type Traits for Auto-Discovery Customization
// ============================================================================
//
// This header provides a way to customize binding behavior WITHOUT writing
// explicit binding code. Users define traits for their types, and the
// reflection-based auto-discovery respects these traits.
//
// Philosophy: "Declare intent once, in one place"
//
// Usage:
//   struct MyClass {
//       int public_data;
//       void* internal_ptr;  // Want to exclude this
//       int camelCase;       // Want to rename this
//   };
//
//   // Define traits ONCE (can be in the same header as MyClass)
//   template<>
//   struct mirror_bridge::traits<MyClass> {
//       static constexpr auto excluded = std::make_tuple("internal_ptr");
//       static constexpr auto renamed = std::make_tuple(
//           std::pair{"camelCase", "camel_case"}
//       );
//       static constexpr auto readonly = std::make_tuple("computed_value");
//   };
//
//   // Then bind with ONE line - traits are automatically applied:
//   mirror_bridge::bind_class<MyClass>(m, "MyClass");
//
// ============================================================================

#include <tuple>
#include <string_view>
#include <type_traits>
#include <utility>

namespace mirror_bridge {

// ============================================================================
// Default Traits (no customization)
// ============================================================================

template<typename T>
struct traits {
    // By default, nothing is excluded
    static constexpr auto excluded = std::make_tuple();

    // By default, no renames
    static constexpr auto renamed = std::make_tuple();

    // By default, nothing is readonly
    static constexpr auto readonly = std::make_tuple();

    // By default, everything is included
    static constexpr bool include_all = true;
};

// ============================================================================
// Trait Detection Helpers
// ============================================================================

// Check if a member name is in a tuple of names
template<typename Tuple, std::size_t... Is>
constexpr bool is_name_in_tuple_impl(std::string_view name, const Tuple& t,
                                      std::index_sequence<Is...>) {
    return ((std::get<Is>(t) == name) || ...);
}

template<typename Tuple>
constexpr bool is_name_in_tuple(std::string_view name, const Tuple& t) {
    return is_name_in_tuple_impl(name, t,
        std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

// Check if a member should be excluded
template<typename T>
constexpr bool should_exclude(std::string_view member_name) {
    return is_name_in_tuple(member_name, traits<T>::excluded);
}

// Check if a member should be readonly
template<typename T>
constexpr bool should_be_readonly(std::string_view member_name) {
    return is_name_in_tuple(member_name, traits<T>::readonly);
}

// Get renamed name for a member (returns original if not renamed)
template<typename Tuple, std::size_t... Is>
constexpr std::string_view get_renamed_impl(std::string_view name, const Tuple& t,
                                             std::index_sequence<Is...>) {
    std::string_view result = name;
    (([&] {
        const auto& pair = std::get<Is>(t);
        if (pair.first == name) {
            result = pair.second;
        }
    }()), ...);
    return result;
}

template<typename T>
constexpr std::string_view get_python_name(std::string_view cpp_name) {
    if constexpr (std::tuple_size_v<decltype(traits<T>::renamed)> == 0) {
        return cpp_name;
    } else {
        return get_renamed_impl(cpp_name, traits<T>::renamed,
            std::make_index_sequence<std::tuple_size_v<decltype(traits<T>::renamed)>>{});
    }
}

// ============================================================================
// Convenience Macros for Defining Traits
// ============================================================================

// Simple exclusion list
#define MIRROR_BRIDGE_EXCLUDE(...) \
    static constexpr auto excluded = std::make_tuple(__VA_ARGS__)

// Simple readonly list
#define MIRROR_BRIDGE_READONLY(...) \
    static constexpr auto readonly = std::make_tuple(__VA_ARGS__)

// Rename mapping (use with pairs)
#define MIRROR_BRIDGE_RENAME(...) \
    static constexpr auto renamed = std::make_tuple(__VA_ARGS__)

// ============================================================================
// Alternative: In-Class Markers (for users who prefer keeping traits with class)
// ============================================================================

// Marker type for excluded members
struct exclude_from_binding_t {};
inline constexpr exclude_from_binding_t exclude_from_binding{};

// Marker type for readonly members
struct readonly_binding_t {};
inline constexpr readonly_binding_t readonly_binding{};

// Users can use static constexpr markers in their class:
//
//   struct MyClass {
//       static constexpr auto _mirror_bridge_exclude = std::make_tuple("internal_ptr");
//       static constexpr auto _mirror_bridge_readonly = std::make_tuple("computed");
//
//       int public_data;
//       void* internal_ptr;
//       int computed;
//   };

// Detection for in-class markers
template<typename T, typename = void>
struct has_exclude_marker : std::false_type {};

template<typename T>
struct has_exclude_marker<T, std::void_t<decltype(T::_mirror_bridge_exclude)>>
    : std::true_type {};

template<typename T, typename = void>
struct has_readonly_marker : std::false_type {};

template<typename T>
struct has_readonly_marker<T, std::void_t<decltype(T::_mirror_bridge_readonly)>>
    : std::true_type {};

// Combined check: either traits specialization or in-class marker
template<typename T>
constexpr bool is_excluded(std::string_view member_name) {
    // Check traits first
    if (should_exclude<T>(member_name)) {
        return true;
    }
    // Then check in-class marker
    if constexpr (has_exclude_marker<T>::value) {
        return is_name_in_tuple(member_name, T::_mirror_bridge_exclude);
    }
    return false;
}

template<typename T>
constexpr bool is_readonly(std::string_view member_name) {
    // Check traits first
    if (should_be_readonly<T>(member_name)) {
        return true;
    }
    // Then check in-class marker
    if constexpr (has_readonly_marker<T>::value) {
        return is_name_in_tuple(member_name, T::_mirror_bridge_readonly);
    }
    return false;
}

} // namespace mirror_bridge
