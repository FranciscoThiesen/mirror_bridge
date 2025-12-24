#pragma once

// ============================================================================
// Mirror Bridge - Type-Safe Field Configuration (No Type Modification)
// ============================================================================
//
// Configure binding behavior WITHOUT modifying your struct definitions!
// Uses member pointers for compile-time type safety.
//
// Usage:
//   struct MyClass {
//       int public_data;
//       void* internal_ptr;     // Want to exclude
//       int creation_time;      // Want readonly
//   };
//
//   // Configure AFTER the struct - clean and type-safe:
//   MIRROR_FIELD_CONFIG(MyClass,
//       EXCLUDE(&MyClass::internal_ptr),
//       READONLY(&MyClass::creation_time)
//   );
//
//   // Or equivalently, with individual macros:
//   MIRROR_EXCLUDE(MyClass, internal_ptr);
//   MIRROR_READONLY(MyClass, creation_time);
//
// ============================================================================

#include <type_traits>
#include <cstddef>

namespace mirror_bridge {

// ============================================================================
// Member Pointer Utilities
// ============================================================================

// Get the offset of a member from its pointer-to-member
template<typename T, typename M>
constexpr std::size_t member_offset(M T::* ptr) {
    // Use offsetof-like calculation at compile time
    return reinterpret_cast<std::size_t>(
        &(static_cast<T*>(nullptr)->*ptr)
    );
}

// ============================================================================
// Field Configuration Storage
// ============================================================================

// Default: no excluded fields
template<typename T>
struct excluded_fields {
    static constexpr bool has_config = false;

    template<typename M>
    static constexpr bool is_excluded(M T::*) { return false; }
};

// Default: no readonly fields
template<typename T>
struct readonly_fields {
    static constexpr bool has_config = false;

    template<typename M>
    static constexpr bool is_readonly(M T::*) { return false; }
};

// ============================================================================
// Field Pointer List (for storing multiple field pointers)
// ============================================================================

template<typename T, typename... Ms>
struct field_list {
    // Store offsets at compile time
    static constexpr std::size_t offsets[] = { member_offset<T, Ms>(nullptr)... };
    static constexpr std::size_t count = sizeof...(Ms);

    // Check if a given member pointer matches any in our list
    template<typename M>
    static constexpr bool contains(M T::* ptr) {
        std::size_t target = member_offset(ptr);
        for (std::size_t i = 0; i < count; ++i) {
            if (offsets[i] == target) return true;
        }
        return false;
    }
};

// Empty list specialization
template<typename T>
struct field_list<T> {
    static constexpr std::size_t count = 0;

    template<typename M>
    static constexpr bool contains(M T::*) { return false; }
};

// ============================================================================
// Name-Based Lookup (for reflection integration)
// ============================================================================

// Default: empty config
template<typename T>
struct field_config {
    static constexpr bool is_excluded(const char* name) { return false; }
    static constexpr bool is_readonly(const char* name) { return false; }
};

// ============================================================================
// Configuration Macros - Clean syntax for users
// ============================================================================

// Helper to create a field offset entry
#define _MB_FIELD_OFFSET(Class, field) \
    offsetof(Class, field)

// Main configuration macro
#define MIRROR_FIELD_CONFIG(Class, ...) \
    template<> struct mirror_bridge::field_config<Class> { \
        __VA_ARGS__ \
    }

// Exclude a single field (standalone macro)
#define MIRROR_EXCLUDE(Class, field) \
    template<> struct mirror_bridge::excluded_fields<Class> { \
        static constexpr bool has_config = true; \
        static constexpr std::size_t excluded_offsets[] = { offsetof(Class, field) }; \
        static constexpr std::size_t count = 1; \
        static constexpr bool is_excluded_offset(std::size_t off) { \
            for (std::size_t i = 0; i < count; ++i) \
                if (excluded_offsets[i] == off) return true; \
            return false; \
        } \
    }

// Readonly a single field (standalone macro)
#define MIRROR_READONLY(Class, field) \
    template<> struct mirror_bridge::readonly_fields<Class> { \
        static constexpr bool has_config = true; \
        static constexpr std::size_t readonly_offsets[] = { offsetof(Class, field) }; \
        static constexpr std::size_t count = 1; \
        static constexpr bool is_readonly_offset(std::size_t off) { \
            for (std::size_t i = 0; i < count; ++i) \
                if (readonly_offsets[i] == off) return true; \
            return false; \
        } \
    }

// Multi-field versions
#define MIRROR_EXCLUDE_FIELDS(Class, ...) \
    template<> struct mirror_bridge::excluded_fields<Class> { \
        static constexpr bool has_config = true; \
        static constexpr std::size_t excluded_offsets[] = { \
            _MB_EXPAND_OFFSETS(Class, __VA_ARGS__) \
        }; \
        static constexpr std::size_t count = sizeof(excluded_offsets) / sizeof(std::size_t); \
        static constexpr bool is_excluded_offset(std::size_t off) { \
            for (std::size_t i = 0; i < count; ++i) \
                if (excluded_offsets[i] == off) return true; \
            return false; \
        } \
    }

#define MIRROR_READONLY_FIELDS(Class, ...) \
    template<> struct mirror_bridge::readonly_fields<Class> { \
        static constexpr bool has_config = true; \
        static constexpr std::size_t readonly_offsets[] = { \
            _MB_EXPAND_OFFSETS(Class, __VA_ARGS__) \
        }; \
        static constexpr std::size_t count = sizeof(readonly_offsets) / sizeof(std::size_t); \
        static constexpr bool is_readonly_offset(std::size_t off) { \
            for (std::size_t i = 0; i < count; ++i) \
                if (readonly_offsets[i] == off) return true; \
            return false; \
        } \
    }

// Helper to expand field names to offsets
#define _MB_EXPAND_OFFSETS(Class, ...) \
    _MB_EXPAND_OFFSETS_IMPL(Class, __VA_ARGS__)

#define _MB_EXPAND_OFFSETS_IMPL(Class, field, ...) \
    offsetof(Class, field) __VA_OPT__(, _MB_EXPAND_OFFSETS_IMPL(Class, __VA_ARGS__))

} // namespace mirror_bridge
