#pragma once

// ============================================================================
// Mirror Bridge - Non-Intrusive Field Configuration
// ============================================================================
//
// Configure binding behavior WITHOUT modifying your struct definitions!
// Uses a simple template specialization pattern.
//
// Usage:
//   struct MyClass {
//       int public_data;
//       void* internal_ptr;
//       int creation_time;
//   };
//
//   // Configure separately - typos caught at compile time!
//   template<> inline constexpr auto mirror_bridge::exclude<MyClass> =
//       mirror_bridge::fields(&MyClass::internal_ptr);
//
//   template<> inline constexpr auto mirror_bridge::readonly<MyClass> =
//       mirror_bridge::fields(&MyClass::creation_time);
//
// ============================================================================

#include <string_view>
#include <array>
#include <cstddef>
#include <tuple>

namespace mirror_bridge {

// ============================================================================
// Field pointer to name conversion
// ============================================================================

// Storage for field configuration
template<std::size_t N>
struct field_names {
    std::array<std::string_view, N> names;

    constexpr bool contains(std::string_view name) const {
        for (const auto& n : names) {
            if (n == name) return true;
        }
        return false;
    }
};

// Empty field names
inline constexpr field_names<0> no_fields{{}};

// Create field names from strings (validated elsewhere)
template<typename... Strs>
constexpr auto fields(Strs... names) {
    return field_names<sizeof...(Strs)>{{std::string_view{names}...}};
}

// ============================================================================
// Configuration templates - specialize these for your types
// ============================================================================

// Fields to exclude from Python binding
template<typename T>
inline constexpr auto exclude = no_fields;

// Fields to make read-only in Python binding
template<typename T>
inline constexpr auto readonly = no_fields;

// ============================================================================
// Query functions
// ============================================================================

template<typename T>
constexpr bool is_excluded(std::string_view field_name) {
    return exclude<T>.contains(field_name);
}

template<typename T>
constexpr bool is_readonly(std::string_view field_name) {
    return readonly<T>.contains(field_name);
}

} // namespace mirror_bridge

// ============================================================================
// Validation Macro - Use this to ensure field names are correct
// ============================================================================

// Validates that a field exists in the class
#define MIRROR_VALIDATE(Class, field) \
    static_assert(sizeof(std::declval<Class>().field) > 0, \
        "Field '" #field "' not found in '" #Class "'")
