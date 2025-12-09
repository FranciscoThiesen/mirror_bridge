#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge Core - Language-Agnostic Reflection Infrastructure
// ═══════════════════════════════════════════════════════════════════════════
//
// This header contains the language-agnostic core of Mirror Bridge:
// • C++26 Reflection (P2996) introspection utilities
// • Type traits and concepts for compile-time type classification
// • Member and method caches for compile-time performance
// • Class metadata registry for change detection
//
// This code is shared across all language bindings (Python, JavaScript, Lua)
//
// ═══════════════════════════════════════════════════════════════════════════

#include <meta>
#include <string>
#include <string_view>
#include <vector>
#include <type_traits>
#include <concepts>
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <shared_mutex>

// ============================================================================
// Feature Detection - Check for P2996 Reflection Support
// ============================================================================

#ifndef __cpp_reflection
  #warning "Compiler does not define __cpp_reflection feature-test macro. " \
           "Reflection support is experimental and may be incomplete."
#elif __cpp_reflection < 202306L
  #error "This library requires C++26 reflection (P2996) from 2023-06 or later"
#endif

// Library version and capabilities
#define MIRROR_BRIDGE_VERSION_MAJOR 0
#define MIRROR_BRIDGE_VERSION_MINOR 2
#define MIRROR_BRIDGE_VERSION_PATCH 0

#define MIRROR_BRIDGE_HAS_REFLECTION 1
#define MIRROR_BRIDGE_HAS_ENUMERATORS_OF 1
#define MIRROR_BRIDGE_HAS_TYPE_SIGNATURES 1

namespace mirror_bridge {
namespace core {

// ============================================================================
// Type Traits and Concepts
// ============================================================================

// Concept to identify arithmetic types (int, float, double, etc.)
template<typename T>
concept Arithmetic = std::is_arithmetic_v<std::remove_cvref_t<T>>;

// Concept to identify string-like types
template<typename T>
concept StringLike =
    std::is_same_v<std::remove_cvref_t<T>, std::string> ||
    std::is_same_v<std::remove_cvref_t<T>, std::string_view> ||
    std::is_same_v<std::remove_cvref_t<T>, const char*> ||
    std::is_same_v<std::remove_cvref_t<T>, char*>;

// Concept to identify smart pointers (unique_ptr, shared_ptr)
template<typename T>
concept SmartPointer = requires {
    typename std::remove_cvref_t<T>::element_type;
} && (std::is_same_v<std::remove_cvref_t<T>, std::unique_ptr<typename std::remove_cvref_t<T>::element_type>> ||
      std::is_same_v<std::remove_cvref_t<T>, std::shared_ptr<typename std::remove_cvref_t<T>::element_type>>);

// Concept to identify C++ standard containers (vector, array, list, etc.)
template<typename T>
concept Container =
    requires {
        { std::declval<T>().begin() } -> std::input_or_output_iterator;
        { std::declval<T>().end() } -> std::input_or_output_iterator;
        { std::declval<T>().size() } -> std::convertible_to<std::size_t>;
        typename std::remove_cvref_t<T>::value_type;
    } &&
    !StringLike<T> &&
    !SmartPointer<T>;

// Concept for enum types
template<typename T>
concept EnumType = std::is_enum_v<std::remove_cvref_t<T>>;

// Concept for types that can be bound (classes with reflectable members)
template<typename T>
concept Bindable = std::is_class_v<std::remove_cvref_t<T>> && requires {
    { std::meta::nonstatic_data_members_of(^^std::remove_cvref_t<T>, std::meta::access_context::current()) };
};

// Concept for nested bindable classes
template<typename T>
concept NestedBindable = Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>;

// ============================================================================
// Class Metadata and Registry
// ============================================================================

struct ClassMetadata {
    std::string name;
    std::string type_signature;
    size_t hash;
    void* language_type_object;  // Language-specific type object (PyTypeObject*, etc.)

    void compute_hash() {
        std::hash<std::string> hasher;
        hash = hasher(type_signature);
    }

    bool needs_recompilation(const std::string& new_signature) const {
        return type_signature != new_signature;
    }
};

class Registry {
private:
    std::unordered_map<std::string, ClassMetadata> classes;
    Registry() = default;

public:
    static Registry& instance() {
        static Registry reg;
        return reg;
    }

    void register_class(const std::string& name, const std::string& signature, void* type_obj = nullptr) {
        ClassMetadata meta{name, signature, 0, type_obj};
        meta.compute_hash();
        classes[name] = meta;
    }

    const ClassMetadata* get_class(const std::string& name) const {
        auto it = classes.find(name);
        return (it != classes.end()) ? &it->second : nullptr;
    }

    bool is_registered(const std::string& name) const {
        return classes.find(name) != classes.end();
    }
};

// ============================================================================
// Global Type Registry - Cross-Module Type Sharing
// ============================================================================
//
// Problem: When C++ types are bound in separate shared libraries (.so files),
// each library gets its own copy of template static variables. This means
// TypeRegistry<Point>::py_type in point.so is different from the same variable
// in rectangle.so - causing cross-module type lookups to fail.
//
// Solution: Use a truly global registry with std::type_index as the key.
// The registry is shared across all shared libraries because:
// 1. inline static variables in header-only code share the same address
// 2. std::type_index provides stable type identity across translation units
//
// Thread Safety: Uses std::shared_mutex for concurrent read access with
// exclusive write access. Most operations are reads (type lookups), so
// shared_mutex provides better concurrency than a regular mutex.
//
// Usage:
//   // In bind_class<T>():
//   GlobalTypeRegistry::register_type<T>(py_type_object);
//
//   // In to_python() for Bindable types:
//   void* py_type = GlobalTypeRegistry::lookup<T>();
//
class GlobalTypeRegistry {
private:
    // The actual storage - inline static ensures single instance across all TUs
    inline static std::unordered_map<std::type_index, void*> registry_;
    inline static std::shared_mutex mutex_;

public:
    // Register a type with its language-specific type object (e.g., PyTypeObject*)
    // Thread-safe: acquires exclusive lock
    template<typename T>
    static void register_type(void* type_object) {
        std::unique_lock lock(mutex_);
        registry_[std::type_index(typeid(T))] = type_object;
    }

    // Look up the type object for a given C++ type
    // Thread-safe: acquires shared lock (allows concurrent reads)
    // Returns nullptr if type is not registered
    template<typename T>
    static void* lookup() {
        std::shared_lock lock(mutex_);
        auto it = registry_.find(std::type_index(typeid(T)));
        return it != registry_.end() ? it->second : nullptr;
    }

    // Check if a type is registered
    // Thread-safe: acquires shared lock
    template<typename T>
    static bool is_registered() {
        std::shared_lock lock(mutex_);
        return registry_.find(std::type_index(typeid(T))) != registry_.end();
    }

    // Unregister a type (useful for cleanup in tests)
    // Thread-safe: acquires exclusive lock
    template<typename T>
    static void unregister() {
        std::unique_lock lock(mutex_);
        registry_.erase(std::type_index(typeid(T)));
    }

    // Get the number of registered types (for debugging)
    static size_t size() {
        std::shared_lock lock(mutex_);
        return registry_.size();
    }

    // Clear all registrations (for testing)
    static void clear() {
        std::unique_lock lock(mutex_);
        registry_.clear();
    }
};

// ============================================================================
// Reflection Utilities - Member Discovery
// ============================================================================

// Data Member utilities
template<typename T, std::size_t I>
consteval auto get_data_member() {
    auto members = std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current());
    return members[I];
}

template<typename T>
consteval std::size_t get_data_member_count() {
    return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()).size();
}

// Member Function Cache - using members_of and filtering (instance methods only)
template<typename T>
struct MemberFunctionCache {
    static consteval bool is_bindable_method(std::meta::info member) {
        return std::meta::is_function(member) &&
               !std::meta::is_static_member(member) &&
               !std::meta::is_constructor(member) &&
               !std::meta::is_special_member_function(member) &&
               !std::meta::is_operator_function(member);
    }

    static consteval std::size_t compute_count() {
        auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
        std::size_t count = 0;
        for (auto member : all_members) {
            if (is_bindable_method(member)) {
                count++;
            }
        }
        return count;
    }

    static consteval auto get_at_index(std::size_t Index) {
        auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
        std::size_t func_index = 0;
        for (auto member : all_members) {
            if (is_bindable_method(member)) {
                if (func_index == Index) {
                    return member;
                }
                func_index++;
            }
        }
        return all_members[0];
    }

    static constexpr std::size_t count = compute_count();
};

// Static Member Function Cache - for class-level static methods
template<typename T>
struct StaticMemberFunctionCache {
    static consteval bool is_bindable_static_method(std::meta::info member) {
        return std::meta::is_function(member) &&
               std::meta::is_static_member(member) &&
               !std::meta::is_constructor(member) &&
               !std::meta::is_special_member_function(member) &&
               !std::meta::is_operator_function(member);
    }

    static consteval std::size_t compute_count() {
        auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
        std::size_t count = 0;
        for (auto member : all_members) {
            if (is_bindable_static_method(member)) {
                count++;
            }
        }
        return count;
    }

    static consteval auto get_at_index(std::size_t Index) {
        auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
        std::size_t func_index = 0;
        for (auto member : all_members) {
            if (is_bindable_static_method(member)) {
                if (func_index == Index) {
                    return member;
                }
                func_index++;
            }
        }
        return all_members[0];
    }

    static constexpr std::size_t count = compute_count();
};

template<typename T, std::size_t I>
consteval auto get_member_function() {
    return MemberFunctionCache<T>::get_at_index(I);
}

template<typename T>
consteval std::size_t get_member_function_count() {
    return MemberFunctionCache<T>::count;
}

template<typename T, std::size_t I>
consteval auto get_static_member_function() {
    return StaticMemberFunctionCache<T>::get_at_index(I);
}

template<typename T>
consteval std::size_t get_static_member_function_count() {
    return StaticMemberFunctionCache<T>::count;
}

template<typename T, std::size_t Index>
consteval const char* get_static_member_function_name() {
    constexpr auto func = get_static_member_function<T, Index>();
    return std::meta::identifier_of(func).data();
}

// Static method parameter introspection
template<typename T, std::size_t FuncIndex>
consteval std::size_t get_static_method_param_count() {
    constexpr auto func = get_static_member_function<T, FuncIndex>();
    return std::meta::parameters_of(func).size();
}

template<typename T, std::size_t FuncIndex, std::size_t ParamIndex>
consteval auto get_static_method_param_type() {
    constexpr auto func = get_static_member_function<T, FuncIndex>();
    auto params = std::meta::parameters_of(func);
    return std::meta::type_of(params[ParamIndex]);
}

template<typename T, std::size_t FuncIndex>
consteval auto get_static_method_return_type() {
    constexpr auto func = get_static_member_function<T, FuncIndex>();
    return std::meta::return_type_of(func);
}

// Method parameter introspection
template<typename T, std::size_t FuncIndex>
consteval std::size_t get_method_param_count() {
    constexpr auto func = get_member_function<T, FuncIndex>();
    return std::meta::parameters_of(func).size();
}

template<typename T, std::size_t FuncIndex, std::size_t ParamIndex>
consteval auto get_method_param_type() {
    constexpr auto func = get_member_function<T, FuncIndex>();
    auto params = std::meta::parameters_of(func);
    return std::meta::type_of(params[ParamIndex]);
}

template<typename T, std::size_t FuncIndex>
consteval auto get_method_return_type() {
    constexpr auto func = get_member_function<T, FuncIndex>();
    return std::meta::return_type_of(func);
}

// Nested member utilities (for dict/object conversion)
template<typename T>
consteval std::size_t get_nested_member_count() {
    return get_data_member_count<T>();
}

template<typename T, std::size_t I>
consteval auto get_nested_member() {
    return get_data_member<T, I>();
}

template<typename T, std::size_t I>
consteval const char* get_nested_member_name() {
    constexpr auto member = get_nested_member<T, I>();
    return std::meta::identifier_of(member);
}

template<typename T, std::size_t I>
using NestedMemberType = typename [:std::meta::type_of(get_nested_member<T, I>()):];

// ============================================================================
// Constructor Introspection
// ============================================================================

template<typename T>
consteval std::size_t get_constructor_count() {
    constexpr auto ctors = std::meta::members_of(^^T);
    std::size_t count = 0;

    for (auto ctor : ctors) {
        if (std::meta::is_constructor(ctor)) {
            ++count;
        }
    }

    return count;
}

template<typename T>
consteval bool has_default_constructor() {
    return std::is_default_constructible_v<T>;
}

template<typename T>
consteval bool has_parameterized_constructor() {
    return get_constructor_count<T>() > (has_default_constructor<T>() ? 1 : 0);
}

// ============================================================================
// Type Signature Generation (for change detection)
// ============================================================================

template<Bindable T>
std::string generate_type_signature(const char* file_hash = nullptr) {
    std::string sig;

    // Add file hash if provided
    if (file_hash) {
        sig += "hash:";
        sig += file_hash;
        sig += "|";
    }

    // Add class name
    sig += "class:";
    sig += std::meta::identifier_of(^^T);
    sig += "|members:";

    // Add member signatures
    constexpr std::size_t member_count = get_data_member_count<T>();
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr auto member = get_data_member<T, Is>();
            sig += std::meta::identifier_of(member);
            sig += ":";
            sig += std::meta::identifier_of(std::meta::type_of(member));
            if (Is + 1 < member_count) sig += ",";
        }(), ...);
    }(std::make_index_sequence<member_count>{});

    sig += "|methods:";

    // Add method signatures
    constexpr std::size_t method_count = get_member_function_count<T>();
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr auto func = get_member_function<T, Is>();
            sig += std::meta::identifier_of(func);
            if (Is + 1 < method_count) sig += ",";
        }(), ...);
    }(std::make_index_sequence<method_count>{});

    return sig;
}

} // namespace core
} // namespace mirror_bridge
