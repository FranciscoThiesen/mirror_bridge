#pragma once

// ============================================================================
// Mirror Bridge - Field Annotations for Binding Customization
// ============================================================================
//
// Annotate fields directly - no separate configuration needed!
//
// Usage:
//   struct MyClass {
//       int public_data;                              // Normal binding
//       mirror_bridge::exclude<void*> internal_ptr;   // Excluded from Python
//       mirror_bridge::readonly<int> creation_time;   // Read-only in Python
//   };
//
//   // Just bind - annotations are auto-detected:
//   mirror_bridge::bind_class<MyClass>(m, "MyClass");
//
// ============================================================================

#include <type_traits>
#include <utility>

namespace mirror_bridge {

// ============================================================================
// exclude<T> - Member will not appear in Python bindings
// ============================================================================
//
// Use for:
//   - Internal pointers (void*, implementation handles)
//   - Security-sensitive data (password hashes, tokens)
//   - Implementation details not meant for scripting
//
// The wrapper is transparent in C++ - implicit conversions to T work.

template<typename T>
struct exclude {
    T value{};

    // Constructors
    constexpr exclude() = default;
    constexpr exclude(const T& v) : value(v) {}
    constexpr exclude(T&& v) : value(std::move(v)) {}

    // Implicit conversion to T& (transparent access)
    constexpr operator T&() noexcept { return value; }
    constexpr operator const T&() const noexcept { return value; }

    // Pointer-like access for pointer/smart-pointer types
    constexpr auto operator->() noexcept -> T* { return &value; }
    constexpr auto operator->() const noexcept -> const T* { return &value; }

    // Dereference for pointer types
    template<typename U = T>
    constexpr auto operator*() noexcept
        -> std::enable_if_t<std::is_pointer_v<U>, std::remove_pointer_t<U>&> {
        return *value;
    }

    template<typename U = T>
    constexpr auto operator*() const noexcept
        -> std::enable_if_t<std::is_pointer_v<U>, const std::remove_pointer_t<U>&> {
        return *value;
    }

    // Assignment from T
    constexpr exclude& operator=(const T& v) { value = v; return *this; }
    constexpr exclude& operator=(T&& v) { value = std::move(v); return *this; }

    // Direct access
    constexpr T& get() noexcept { return value; }
    constexpr const T& get() const noexcept { return value; }

    // Comparison operators for seamless use (both directions)
    template<typename U>
    friend constexpr bool operator==(const exclude& lhs, const U& rhs) { return lhs.value == rhs; }

    template<typename U>
    friend constexpr bool operator==(const U& lhs, const exclude& rhs) { return lhs == rhs.value; }

    template<typename U>
    friend constexpr bool operator!=(const exclude& lhs, const U& rhs) { return lhs.value != rhs; }

    template<typename U>
    friend constexpr bool operator!=(const U& lhs, const exclude& rhs) { return lhs != rhs.value; }

    template<typename U>
    friend constexpr bool operator<(const exclude& lhs, const U& rhs) { return lhs.value < rhs; }

    template<typename U>
    friend constexpr bool operator<(const U& lhs, const exclude& rhs) { return lhs < rhs.value; }

    template<typename U>
    friend constexpr bool operator<=(const exclude& lhs, const U& rhs) { return lhs.value <= rhs; }

    template<typename U>
    friend constexpr bool operator<=(const U& lhs, const exclude& rhs) { return lhs <= rhs.value; }

    template<typename U>
    friend constexpr bool operator>(const exclude& lhs, const U& rhs) { return lhs.value > rhs; }

    template<typename U>
    friend constexpr bool operator>(const U& lhs, const exclude& rhs) { return lhs > rhs.value; }

    template<typename U>
    friend constexpr bool operator>=(const exclude& lhs, const U& rhs) { return lhs.value >= rhs; }

    template<typename U>
    friend constexpr bool operator>=(const U& lhs, const exclude& rhs) { return lhs >= rhs.value; }
};

// ============================================================================
// readonly<T> - Member is read-only in Python (getter only, no setter)
// ============================================================================
//
// Use for:
//   - Creation timestamps
//   - Computed/cached values
//   - IDs that shouldn't change after construction
//
// C++ code retains full read-write access.

template<typename T>
struct readonly {
    T value{};

    // Constructors
    constexpr readonly() = default;
    constexpr readonly(const T& v) : value(v) {}
    constexpr readonly(T&& v) : value(std::move(v)) {}

    // Implicit conversion (full access in C++)
    constexpr operator T&() noexcept { return value; }
    constexpr operator const T&() const noexcept { return value; }

    // Pointer-like access
    constexpr auto operator->() noexcept -> T* { return &value; }
    constexpr auto operator->() const noexcept -> const T* { return &value; }

    // Assignment (allowed in C++, blocked in Python binding)
    constexpr readonly& operator=(const T& v) { value = v; return *this; }
    constexpr readonly& operator=(T&& v) { value = std::move(v); return *this; }

    // Direct access
    constexpr T& get() noexcept { return value; }
    constexpr const T& get() const noexcept { return value; }

    // Comparison operators for seamless use (both directions)
    template<typename U>
    friend constexpr bool operator==(const readonly& lhs, const U& rhs) { return lhs.value == rhs; }

    template<typename U>
    friend constexpr bool operator==(const U& lhs, const readonly& rhs) { return lhs == rhs.value; }

    template<typename U>
    friend constexpr bool operator!=(const readonly& lhs, const U& rhs) { return lhs.value != rhs; }

    template<typename U>
    friend constexpr bool operator!=(const U& lhs, const readonly& rhs) { return lhs != rhs.value; }

    template<typename U>
    friend constexpr bool operator<(const readonly& lhs, const U& rhs) { return lhs.value < rhs; }

    template<typename U>
    friend constexpr bool operator<(const U& lhs, const readonly& rhs) { return lhs < rhs.value; }

    template<typename U>
    friend constexpr bool operator<=(const readonly& lhs, const U& rhs) { return lhs.value <= rhs; }

    template<typename U>
    friend constexpr bool operator<=(const U& lhs, const readonly& rhs) { return lhs <= rhs.value; }

    template<typename U>
    friend constexpr bool operator>(const readonly& lhs, const U& rhs) { return lhs.value > rhs; }

    template<typename U>
    friend constexpr bool operator>(const U& lhs, const readonly& rhs) { return lhs > rhs.value; }

    template<typename U>
    friend constexpr bool operator>=(const readonly& lhs, const U& rhs) { return lhs.value >= rhs; }

    template<typename U>
    friend constexpr bool operator>=(const U& lhs, const readonly& rhs) { return lhs >= rhs.value; }
};

// ============================================================================
// Type Traits for Detection
// ============================================================================

// Detect exclude<T>
template<typename T>
struct is_excluded : std::false_type {};

template<typename T>
struct is_excluded<exclude<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_excluded_v = is_excluded<std::remove_cvref_t<T>>::value;

// Detect readonly<T>
template<typename T>
struct is_readonly_field : std::false_type {};

template<typename T>
struct is_readonly_field<readonly<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_readonly_field_v = is_readonly_field<std::remove_cvref_t<T>>::value;

// Unwrap the inner type from annotations
template<typename T>
struct unwrap_annotation {
    using type = T;
};

template<typename T>
struct unwrap_annotation<exclude<T>> {
    using type = T;
};

template<typename T>
struct unwrap_annotation<readonly<T>> {
    using type = T;
};

template<typename T>
using unwrap_annotation_t = typename unwrap_annotation<std::remove_cvref_t<T>>::type;

// Check if a type has any annotation
template<typename T>
inline constexpr bool is_annotated_v = is_excluded_v<T> || is_readonly_field_v<T>;

// ============================================================================
// Compile-Time Member Filtering Helpers
// ============================================================================

// Count non-excluded members at compile time
template<typename T, std::size_t... Is>
consteval std::size_t count_visible_members_impl(std::index_sequence<Is...>);

// Check if member at index should be visible (not excluded)
template<typename T, std::size_t Index>
consteval bool is_member_visible();

// Get the Nth visible member index (maps visible index to actual index)
template<typename T, std::size_t VisibleIndex>
consteval std::size_t get_visible_member_index();

} // namespace mirror_bridge
