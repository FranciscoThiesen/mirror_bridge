#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge - JavaScript Single Header
// ═══════════════════════════════════════════════════════════════════════════
//
// Auto-generated single-header version for JavaScript (Node.js N-API) bindings
// Generated: $(date)
//
// This file contains:
// - Core reflection infrastructure (language-agnostic)
// - JavaScript N-API bindings
//
// Usage:
//   #include "mirror_bridge_javascript.hpp"
//   MIRROR_BRIDGE_JS_MODULE(my_module,
//       mirror_bridge::javascript::bind_class<MyClass>(env, m, "MyClass");
//   )
//
// ═══════════════════════════════════════════════════════════════════════════

// ============================================================================
// CORE - Language-Agnostic Reflection Infrastructure
// ============================================================================


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
#include <expected>
#include <version>
#include <mutex>
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
// Global Type Registry - Cross-Module Type Sharing (RTTI Required)
// ============================================================================
//
// NOTE: This C++ registry requires RTTI (typeid) and is NOT used for actual
// cross-module type sharing in Python bindings. Python bindings use a
// Python-based registry instead (stored in sys.modules) because C++ static
// variables are per-shared-library.
//
// This class is kept for potential future use cases where RTTI is available.
// It is guarded by __cpp_rtti to avoid compilation errors in environments
// where RTTI is disabled (e.g., Node.js N-API addons use -fno-rtti).
//
#if defined(__cpp_rtti) || defined(__GXX_RTTI) || defined(_CPPRTTI)

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

#endif // RTTI check

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

// Member Function Cache — includes methods inherited from base classes.
//
// P2996R13's members_of only returns methods DIRECTLY declared in T. To match
// pybind11's behavior of exposing inherited methods, we walk the inheritance
// chain breadth-first via bases_of, collecting every non-hidden method.
//
// Name hiding follows C++ rules: if T directly declares a name, all overloads
// at THAT level hide base overloads of the same name. Overloads at the same
// inheritance level are all kept. Virtual dispatch is handled by the compiler
// when we splice the base method onto a derived object (P2996 allows
// `derived.[:base_method_info:](args)` — we verified this compiles and
// respects overriding).
template<typename T>
struct MemberFunctionCache {
    static consteval bool is_bindable_method(std::meta::info member) {
        return std::meta::is_function(member) &&
               !std::meta::is_static_member(member) &&
               !std::meta::is_constructor(member) &&
               !std::meta::is_special_member_function(member) &&
               !std::meta::is_operator_function(member);
    }

    // Walk T and its bases breadth-first, accumulating bindable methods with
    // C++-style name hiding. Returns methods in order: T's direct methods
    // first, then each base's non-hidden methods, level by level.
    static consteval std::vector<std::meta::info> collect_methods() {
        constexpr auto ctx = std::meta::access_context::current();
        std::vector<std::meta::info> result;
        std::vector<std::string_view> seen_names;

        std::vector<std::meta::info> layer = { ^^T };
        while (!layer.empty()) {
            std::vector<std::meta::info> next_layer;
            std::vector<std::string_view> layer_new_names;

            for (auto cls : layer) {
                for (auto m : std::meta::members_of(cls, ctx)) {
                    if (!is_bindable_method(m)) continue;
                    auto name = std::meta::identifier_of(m);

                    // Name hiding: skip if a closer scope already had this name.
                    bool hidden = false;
                    for (auto seen : seen_names) {
                        if (seen == name) { hidden = true; break; }
                    }
                    if (hidden) continue;

                    result.push_back(m);
                    // Record for hiding deeper bases, but don't add to seen_names
                    // yet — sibling overloads at the same level shouldn't hide
                    // each other.
                    layer_new_names.push_back(name);
                }
                for (auto b : std::meta::bases_of(cls, ctx)) {
                    next_layer.push_back(std::meta::type_of(b));
                }
            }

            for (auto n : layer_new_names) seen_names.push_back(n);
            layer = std::move(next_layer);
        }

        return result;
    }

    // Materialize BFS result into a constexpr static span (define_static_array).
    // Subsequent per-index lookups are O(1) span access rather than re-running
    // the full BFS — critical for keeping compile-time under the step limit on
    // large classes like Open3D's PointCloud (44 methods).
    static constexpr auto methods = std::define_static_array(collect_methods());
    static constexpr std::size_t count = methods.size();

    static consteval auto get_at_index(std::size_t Index) {
        return methods[Index];
    }
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

// Alias-template forms of the parameter-type getters. Pack expansions must
// use these instead of splicing inline: GCC's reflection implementation
// does not treat a pack as expandable when it appears only inside a splice
// ("expansion pattern contains no parameter packs" / "operand of fold
// expression has no unexpanded parameter packs"), while an alias template
// makes the pack an ordinary template argument that both compilers accept.
template<typename T, std::size_t FuncIndex, std::size_t ParamIndex>
using method_param_t = typename [:get_method_param_type<T, FuncIndex, ParamIndex>():];

template<typename T, std::size_t FuncIndex, std::size_t ParamIndex>
using static_method_param_t = typename [:get_static_method_param_type<T, FuncIndex, ParamIndex>():];

// ============================================================================
// Method Bindability — Physical Feasibility Check
// ============================================================================
//
// A method CANNOT be bound if it physically requires operations that don't
// compile: passing an abstract class by value, for example. This isn't a
// mirror_bridge limitation — no C++ binding library can do it without a
// different calling convention (e.g., pybind11's shared_ptr holder).
//
// We detect these cases at compile time and skip the specific methods, while
// keeping the rest of the class binding functional. The user gets all the
// methods that CAN be bound; methods that can't are emitted as an error if
// called from the target language (not silently missing).

// A type is "value-bindable" (can be held by value in std::tuple<T>) if:
//   - It's default-constructible (tuple needs this for its own default ctor)
//   - It's not abstract (no pure virtuals)
//   - It's complete
//   - It's copy-assignable (for tuple element assignment)
// Primitives, enums, and void are always considered value-bindable.
// Raw pointers: allowed only if pointee is primitive — pointers to user types
// are ambiguous (ownership? nullable output? iterator?) and not safely bindable.
// Check if T has any user-declared virtual (ignoring the virtual destructor).
// Value-copy of such a T slices derived state — including a vtable that was
// swapped in by bind_class_auto — so we must pass by pointer to preserve
// polymorphic dispatch through the original instance.
template<typename T>
consteval bool has_user_virtual() {
    if constexpr (!std::is_class_v<T>) return false;
    else if constexpr (!requires { sizeof(T); }) return false;
    else {
        auto ctx = std::meta::access_context::unchecked();
        for (auto m : std::meta::members_of(^^T, ctx)) {
            if (!std::meta::is_function(m)) continue;
            if (!std::meta::is_virtual(m)) continue;
            if (std::meta::is_special_member_function(m)) continue;
            return true;
        }
        return false;
    }
}

template<typename T>
consteval bool is_value_bindable() {
    using U = std::remove_cvref_t<T>;
    if constexpr (std::is_void_v<U>) return true;
    if constexpr (std::is_arithmetic_v<U>) return true;
    if constexpr (std::is_enum_v<U>) return true;
    // Raw pointers only if pointing at simple types (char* for C-strings,
    // void* opaque handle). Pointers to containers/classes in parameter
    // positions are typically output parameters and not safely auto-bindable.
    if constexpr (std::is_pointer_v<U>) {
        using Pointee = std::remove_cv_t<std::remove_pointer_t<U>>;
        return std::is_same_v<Pointee, char> ||
               std::is_same_v<Pointee, void> ||
               std::is_arithmetic_v<Pointee>;
    }
    if constexpr (requires { sizeof(U); }) {
        // Classes with user-declared virtuals must NOT be value-bindable:
        // copying slices derived overrides and any custom vtable (which is
        // how bind_class_auto implements Python subclass dispatch). Force
        // pointer-holder storage instead so method calls dispatch through
        // the caller's actual instance.
        if constexpr (std::is_class_v<U> && has_user_virtual<U>()) return false;
        // Type is complete. Now check all the properties tuple needs.
        return !std::is_abstract_v<U> &&
               std::is_default_constructible_v<U> &&
               std::is_copy_assignable_v<U>;
    }
    return false;  // incomplete type — can't bind by value
}

// Check if all params of a method are physically bindable by value.
// Abstract types in parameters would fail std::tuple construction.
template<typename T, std::size_t FuncIndex>
consteval bool method_params_are_value_bindable() {
    constexpr std::size_t param_count = get_method_param_count<T, FuncIndex>();
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (is_value_bindable<std::remove_cvref_t<method_param_t<T, FuncIndex, Is>>>() && ...);
    }(std::make_index_sequence<param_count>{});
}

template<typename T, std::size_t FuncIndex>
consteval bool static_method_params_are_value_bindable() {
    constexpr std::size_t param_count = get_static_method_param_count<T, FuncIndex>();
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (is_value_bindable<std::remove_cvref_t<static_method_param_t<T, FuncIndex, Is>>>() && ...);
    }(std::make_index_sequence<param_count>{});
}

// ============================================================================
// Parameter Storage Strategy — Pointer Holders for Non-Value Types
// ============================================================================
//
// Strategy per parameter type:
//   - value-bindable type (primitive, concrete class, vector, etc.)
//       → store by value in std::tuple
//   - complete class type that isn't value-bindable (abstract, non-copyable,
//     has protected ctor, etc.)
//       → store as pointer (extracted from the Python wrapper's cpp_object)
//   - raw pointer to user type
//       → method is NOT bindable (ambiguous semantics); skip it
//
// This is analogous to pybind11's shared_ptr holder approach, but lighter.

// Can this parameter type participate in a bound method signature at all?
// True iff we can either store it by value OR via pointer-holder.
template<typename T>
consteval bool is_param_bindable() {
    using U = std::remove_cvref_t<T>;
    if constexpr (is_value_bindable<U>()) return true;
    // Complete, non-pointer class type → use pointer-holder
    if constexpr (requires { sizeof(U); } && std::is_class_v<U> && !std::is_pointer_v<U>) {
        return true;
    }
    return false;
}

template<typename ParamType>
struct param_storage {
    using Clean = std::remove_cvref_t<ParamType>;
    // Smart pointers fail is_value_bindable (unique_ptr isn't copy-
    // assignable) but must still be stored by value: the SmartPointer
    // from_python overload materializes a fresh pointee from the Python
    // value, and forward_arg then moves the smart pointer into the call.
    // Pointer storage would dereference to an lvalue and try to copy a
    // move-only type at the call site, which doesn't compile.
    using type = std::conditional_t<
        is_value_bindable<Clean>() || SmartPointer<Clean>,
        Clean,     // value storage
        Clean*     // pointer storage (class types extracted from Python wrapper)
    >;
};

template<typename ParamType>
using param_storage_t = typename param_storage<ParamType>::type;

// Check if every param of a method is bindable (either value or pointer-holder).
// Methods with raw-pointer-to-user-type params are NOT bindable.
template<typename T, std::size_t FuncIndex>
consteval bool method_params_all_bindable() {
    constexpr std::size_t param_count = get_method_param_count<T, FuncIndex>();
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (is_param_bindable<method_param_t<T, FuncIndex, Is>>() && ...);
    }(std::make_index_sequence<param_count>{});
}

template<typename T, std::size_t FuncIndex>
consteval bool static_method_params_all_bindable() {
    constexpr std::size_t param_count = get_static_method_param_count<T, FuncIndex>();
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (is_param_bindable<static_method_param_t<T, FuncIndex, Is>>() && ...);
    }(std::make_index_sequence<param_count>{});
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
    auto ctors = std::meta::members_of(^^T, std::meta::access_context::unchecked());
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

// ============================================================================
// Compile-Time Binding Validation
// ============================================================================
//
// Validates that all data member types in a class are convertible before
// binding is generated. Produces clear diagnostic messages instead of
// cryptic template instantiation errors.
//
// Supported types (any nesting depth):
//   - Arithmetic (int, float, double, bool, etc.)
//   - String-like (std::string, std::string_view, const char*)
//   - Containers (std::vector, std::array, std::map, std::set, etc.)
//   - Smart pointers (std::unique_ptr, std::shared_ptr)
//   - std::optional<T>, std::expected<T, E>
//   - Enums (enum, enum class)
//   - Nested Bindable classes (structs with reflectable members)

// Trait-based detection for smart pointers (avoids requires-in-consteval issues)
template<typename T>
struct is_smart_pointer : std::false_type {};

template<typename T>
struct is_smart_pointer<std::unique_ptr<T>> : std::true_type {};

template<typename T>
struct is_smart_pointer<std::shared_ptr<T>> : std::true_type {};

// Trait-based detection for containers
template<typename T, typename = void>
struct is_container : std::false_type {};

template<typename T>
struct is_container<T, std::void_t<
    typename T::value_type,
    decltype(std::declval<T>().begin()),
    decltype(std::declval<T>().size())
>> : std::bool_constant<!std::is_same_v<T, std::string> &&
                         !std::is_same_v<T, std::string_view>> {};

// Checks if a type is convertible by mirror_bridge
template<typename T>
consteval bool is_convertible_type() {
    using U = std::remove_cvref_t<T>;

    if constexpr (std::is_arithmetic_v<U>) {
        return true;
    } else if constexpr (std::is_same_v<U, std::string> ||
                         std::is_same_v<U, std::string_view> ||
                         std::is_same_v<U, const char*> ||
                         std::is_same_v<U, char*>) {
        return true;
    } else if constexpr (std::is_enum_v<U>) {
        return true;
    } else if constexpr (std::is_array_v<U>) {
        // C-style arrays (e.g., float[4], int[16]) — we emit a to_python/
        // from_python overload that produces a Python list. The element type
        // still has to be convertible.
        return is_convertible_type<std::remove_extent_t<U>>();
    } else if constexpr (is_smart_pointer<U>::value) {
        return true;
    } else if constexpr (is_container<U>::value) {
        return true;
    } else if constexpr (Bindable<U>) {
        return true;
    } else {
        return false;
    }
}

// Validates a single data member at index I of class T
template<Bindable T, std::size_t I>
consteval bool validate_member_at_index() {
    constexpr auto member = get_data_member<T, I>();
    using MemberType = std::remove_cvref_t<typename [:std::meta::type_of(member):]>;
    return is_convertible_type<MemberType>();
}

// Validates all data members of a Bindable class using index_sequence.
// Returns true if every data member type is convertible by mirror_bridge.
template<Bindable T, std::size_t... Is>
consteval bool validate_members_impl(std::index_sequence<Is...>) {
    return (validate_member_at_index<T, Is>() && ...);
}

template<Bindable T>
consteval bool validate_bindable_members() {
    constexpr std::size_t count = get_data_member_count<T>();
    return validate_members_impl<T>(std::make_index_sequence<count>{});
}

// Convenience macro — place in bind_class or module definition to get a
// clear error when a class has unconvertible members.
//
// Usage:
//   MIRROR_BRIDGE_VALIDATE(MyClass);
//
// On failure, produces:
//   error: static assertion failed: "MyClass contains members with types that
//   mirror_bridge cannot convert. Mark them with @exclude or add a custom converter."
#define MIRROR_BRIDGE_VALIDATE(T) \
    static_assert(::mirror_bridge::core::validate_bindable_members<T>(), \
        #T " contains members with types that mirror_bridge cannot convert. " \
        "Mark unconvertible members with [[=exclude{}]] or add a custom type converter. " \
        "Supported types: arithmetic, std::string, containers, smart pointers, " \
        "std::optional, std::expected, enums, and nested bindable classes.")

} // namespace core
} // namespace mirror_bridge

// ============================================================================
// JAVASCRIPT - Node.js N-API Bindings
// ============================================================================


// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge JavaScript - JavaScript Bindings for C++ Code via C++26 Reflection
// ═══════════════════════════════════════════════════════════════════════════
// Generates Node.js bindings that expose C++ classes to JavaScript.

#include <node_api.h>
#include <cstdio>
#include <cstring>
#include <optional>
#include <expected>
#include <future>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <tuple>
#include <utility>
#include <variant>

namespace mirror_bridge {
namespace javascript {

// Import core concepts for convenience
using namespace core;

// ============================================================================
// JavaScript Wrapper for C++ Objects
// ============================================================================

template<typename T>
struct JsWrapper {
    T* cpp_object;
    bool owns_memory;
    napi_ref js_ref;  // JavaScript reference for GC management
};

// ============================================================================
// Type Conversion: C++ → JavaScript
// ============================================================================

// Arithmetic types
template<Arithmetic T>
napi_value to_javascript(napi_env env, const T& value) {
    napi_value result;
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
        napi_get_boolean(env, value, &result);
    } else if constexpr (std::is_floating_point_v<T>) {
        napi_create_double(env, static_cast<double>(value), &result);
    } else if constexpr (std::is_signed_v<T>) {
        napi_create_int64(env, static_cast<int64_t>(value), &result);
    } else {
        napi_create_uint32(env, static_cast<uint32_t>(value), &result);
    }
    return result;
}

// Enum types
template<EnumType T>
napi_value to_javascript(napi_env env, const T& value) {
    napi_value result;
    napi_create_int32(env, static_cast<int32_t>(value), &result);
    return result;
}

// String types
template<StringLike T>
napi_value to_javascript(napi_env env, const T& value) {
    napi_value result;
    using BaseType = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<BaseType, std::string> || std::is_same_v<BaseType, std::string_view>) {
        napi_create_string_utf8(env, value.data(), value.size(), &result);
    } else {
        napi_create_string_utf8(env, value, NAPI_AUTO_LENGTH, &result);
    }
    return result;
}

// ============================================================================
// Byte Container Detection - for optimized bulk transfer
// ============================================================================

// Concept for containers of byte-like types (uint8_t, int8_t, char, std::byte)
template<typename T>
concept ByteContainer = Container<T> && requires {
    requires sizeof(typename std::remove_cvref_t<T>::value_type) == 1;
    requires std::is_trivially_copyable_v<typename std::remove_cvref_t<T>::value_type>;
} && requires(T t) {
    { t.data() } -> std::convertible_to<const void*>;  // Must have contiguous storage
};

// Optimized: Byte containers → JavaScript Uint8Array (bulk memcpy)
template<ByteContainer T>
napi_value to_javascript(napi_env env, const T& container) {
    // Create an ArrayBuffer with a copy of the data
    napi_value array_buffer;
    void* buffer_data;
    napi_create_arraybuffer(env, container.size(), &buffer_data, &array_buffer);

    // Bulk copy - single memcpy instead of 1.44M individual calls
    std::memcpy(buffer_data, container.data(), container.size());

    // Create a Uint8Array view over the ArrayBuffer
    napi_value uint8_array;
    napi_create_typedarray(env, napi_uint8_array, container.size(), array_buffer, 0, &uint8_array);

    return uint8_array;
}

// Generic containers → JavaScript arrays (element-by-element for non-byte types)
template<Container T>
    requires (!ByteContainer<T>)
napi_value to_javascript(napi_env env, const T& container) {
    napi_value array;
    napi_create_array_with_length(env, container.size(), &array);

    size_t index = 0;
    for (const auto& item : container) {
        napi_value js_item = to_javascript(env, item);
        napi_set_element(env, array, index++, js_item);
    }
    return array;
}

// Forward declaration for Bindable types. Must precede the SmartPointer
// overload: the pointee is typically a user class in the global namespace,
// so the dependent to_javascript(env, *ptr) call below finds this only
// through the declaration context, never through ADL.
template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    napi_value
>
to_javascript(napi_env env, const T& obj);

// Smart pointers
template<SmartPointer T>
napi_value to_javascript(napi_env env, const T& ptr) {
    if (!ptr) {
        napi_value result;
        napi_get_null(env, &result);
        return result;
    }
    return to_javascript(env, *ptr);
}

// ============================================================================
// Type Conversion: JavaScript → C++
// ============================================================================

// Arithmetic types
template<Arithmetic T>
bool from_javascript(napi_env env, napi_value value, T& out) {
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
        bool temp;
        if (napi_get_value_bool(env, value, &temp) != napi_ok) return false;
        out = temp;
    } else if constexpr (std::is_floating_point_v<T>) {
        double temp;
        if (napi_get_value_double(env, value, &temp) != napi_ok) return false;
        out = static_cast<T>(temp);
    } else if constexpr (std::is_signed_v<T>) {
        int64_t temp;
        if (napi_get_value_int64(env, value, &temp) != napi_ok) return false;
        out = static_cast<T>(temp);
    } else {
        uint32_t temp;
        if (napi_get_value_uint32(env, value, &temp) != napi_ok) return false;
        out = static_cast<T>(temp);
    }
    return true;
}

// Enum types
template<EnumType T>
bool from_javascript(napi_env env, napi_value value, T& out) {
    int32_t temp;
    if (napi_get_value_int32(env, value, &temp) != napi_ok) return false;
    out = static_cast<T>(temp);
    return true;
}

// String types
template<StringLike T>
bool from_javascript(napi_env env, napi_value value, T& out) {
    size_t str_size;
    if (napi_get_value_string_utf8(env, value, nullptr, 0, &str_size) != napi_ok) return false;

    using BaseType = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<BaseType, std::string>) {
        std::string temp(str_size, '\0');
        napi_get_value_string_utf8(env, value, &temp[0], str_size + 1, &str_size);
        out = std::move(temp);
    } else {
        // For char* and string_view, this is unsafe but we support for compatibility
        static thread_local std::string temp_storage;
        temp_storage.resize(str_size);
        napi_get_value_string_utf8(env, value, &temp_storage[0], str_size + 1, &str_size);
        out = temp_storage.c_str();
    }
    return true;
}

// Containers from JavaScript arrays
template<Container T>
bool from_javascript(napi_env env, napi_value value, T& container) {
    bool is_array;
    if (napi_is_array(env, value, &is_array) != napi_ok || !is_array) return false;

    uint32_t length;
    if (napi_get_array_length(env, value, &length) != napi_ok) return false;

    using ValueType = typename std::remove_cvref_t<T>::value_type;

    if constexpr (requires { container.clear(); }) {
        container.clear();
    }
    if constexpr (requires { container.reserve(length); }) {
        container.reserve(length);
    }

    for (uint32_t i = 0; i < length; ++i) {
        napi_value js_item;
        if (napi_get_element(env, value, i, &js_item) != napi_ok) return false;

        ValueType cpp_item;
        if (!from_javascript(env, js_item, cpp_item)) return false;

        if constexpr (requires { container.push_back(cpp_item); }) {
            container.push_back(std::move(cpp_item));
        } else if constexpr (requires { container.insert(cpp_item); }) {
            container.insert(std::move(cpp_item));
        }
    }
    return true;
}

// Forward declaration for Bindable types. Must precede the SmartPointer
// overload for the same lookup reason as to_javascript above: the pointee
// class lives in the global namespace, so ADL can't find a later declaration.
template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    bool
>
from_javascript(napi_env env, napi_value value, T& out);

// Smart pointers from JavaScript
template<SmartPointer T>
bool from_javascript(napi_env env, napi_value value, T& out) {
    using ElementType = typename std::remove_cvref_t<T>::element_type;

    napi_valuetype type;
    if (napi_typeof(env, value, &type) != napi_ok) return false;

    if (type == napi_null || type == napi_undefined) {
        out.reset();
        return true;
    }

    // Mirrors the Python backend: an abstract or non-default-constructible
    // pointee can't be materialized from a JS object by value, and trying
    // would be a compile error. Only null/undefined→reset is supported.
    if constexpr (std::is_abstract_v<ElementType> ||
                  !std::is_default_constructible_v<ElementType> ||
                  !std::is_copy_assignable_v<ElementType>) {
        return true;
    } else {
        ElementType cpp_value;
        if (!from_javascript(env, value, cpp_value)) return false;

        if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::unique_ptr<ElementType>>) {
            out = std::make_unique<ElementType>(std::move(cpp_value));
        } else {
            out = std::make_shared<ElementType>(std::move(cpp_value));
        }
        return true;
    }
}

// ============================================================================
// std::map / std::unordered_map Support - Associative Containers
// ============================================================================

// Convert std::map to JavaScript Object
template<typename K, typename V, typename... Args>
napi_value to_javascript(napi_env env, const std::map<K, V, Args...>& map) {
    napi_value obj;
    napi_create_object(env, &obj);

    for (const auto& [key, value] : map) {
        napi_value js_key = to_javascript(env, key);
        napi_value js_value = to_javascript(env, value);

        // Convert key to string for object property
        napi_value key_str;
        napi_coerce_to_string(env, js_key, &key_str);

        napi_set_property(env, obj, key_str, js_value);
    }
    return obj;
}

// Convert std::unordered_map to JavaScript Object
template<typename K, typename V, typename... Args>
napi_value to_javascript(napi_env env, const std::unordered_map<K, V, Args...>& map) {
    napi_value obj;
    napi_create_object(env, &obj);

    for (const auto& [key, value] : map) {
        napi_value js_key = to_javascript(env, key);
        napi_value js_value = to_javascript(env, value);

        napi_value key_str;
        napi_coerce_to_string(env, js_key, &key_str);

        napi_set_property(env, obj, key_str, js_value);
    }
    return obj;
}

// Convert JavaScript Object to std::map
template<typename K, typename V, typename... Args>
bool from_javascript(napi_env env, napi_value value, std::map<K, V, Args...>& map) {
    napi_valuetype type;
    napi_typeof(env, value, &type);
    if (type != napi_object) return false;

    map.clear();

    // Get property names
    napi_value prop_names;
    if (napi_get_property_names(env, value, &prop_names) != napi_ok) return false;

    uint32_t length;
    napi_get_array_length(env, prop_names, &length);

    for (uint32_t i = 0; i < length; ++i) {
        napi_value js_key;
        napi_get_element(env, prop_names, i, &js_key);

        napi_value js_value;
        napi_get_property(env, value, js_key, &js_value);

        K cpp_key;
        V cpp_value;

        if (!from_javascript(env, js_key, cpp_key)) return false;
        if (!from_javascript(env, js_value, cpp_value)) return false;

        map[std::move(cpp_key)] = std::move(cpp_value);
    }
    return true;
}

// Convert JavaScript Object to std::unordered_map
template<typename K, typename V, typename... Args>
bool from_javascript(napi_env env, napi_value value, std::unordered_map<K, V, Args...>& map) {
    napi_valuetype type;
    napi_typeof(env, value, &type);
    if (type != napi_object) return false;

    map.clear();

    napi_value prop_names;
    if (napi_get_property_names(env, value, &prop_names) != napi_ok) return false;

    uint32_t length;
    napi_get_array_length(env, prop_names, &length);

    for (uint32_t i = 0; i < length; ++i) {
        napi_value js_key;
        napi_get_element(env, prop_names, i, &js_key);

        napi_value js_value;
        napi_get_property(env, value, js_key, &js_value);

        K cpp_key;
        V cpp_value;

        if (!from_javascript(env, js_key, cpp_key)) return false;
        if (!from_javascript(env, js_value, cpp_value)) return false;

        map[std::move(cpp_key)] = std::move(cpp_value);
    }
    return true;
}

// ============================================================================
// std::set / std::unordered_set Support - Set Containers
// ============================================================================

// Convert std::set to JavaScript Array
template<typename V, typename... Args>
napi_value to_javascript(napi_env env, const std::set<V, Args...>& set) {
    napi_value array;
    napi_create_array_with_length(env, set.size(), &array);

    size_t index = 0;
    for (const auto& value : set) {
        napi_value js_value = to_javascript(env, value);
        napi_set_element(env, array, index++, js_value);
    }
    return array;
}

// Convert std::unordered_set to JavaScript Array
template<typename V, typename... Args>
napi_value to_javascript(napi_env env, const std::unordered_set<V, Args...>& set) {
    napi_value array;
    napi_create_array_with_length(env, set.size(), &array);

    size_t index = 0;
    for (const auto& value : set) {
        napi_value js_value = to_javascript(env, value);
        napi_set_element(env, array, index++, js_value);
    }
    return array;
}

// Convert JavaScript Array to std::set
template<typename V, typename... Args>
bool from_javascript(napi_env env, napi_value value, std::set<V, Args...>& set) {
    bool is_array;
    napi_is_array(env, value, &is_array);
    if (!is_array) return false;

    set.clear();

    uint32_t length;
    napi_get_array_length(env, value, &length);

    for (uint32_t i = 0; i < length; ++i) {
        napi_value js_value;
        napi_get_element(env, value, i, &js_value);

        V cpp_value;
        if (!from_javascript(env, js_value, cpp_value)) return false;

        set.insert(std::move(cpp_value));
    }
    return true;
}

// Convert JavaScript Array to std::unordered_set
template<typename V, typename... Args>
bool from_javascript(napi_env env, napi_value value, std::unordered_set<V, Args...>& set) {
    bool is_array;
    napi_is_array(env, value, &is_array);
    if (!is_array) return false;

    set.clear();

    uint32_t length;
    napi_get_array_length(env, value, &length);

    for (uint32_t i = 0; i < length; ++i) {
        napi_value js_value;
        napi_get_element(env, value, i, &js_value);

        V cpp_value;
        if (!from_javascript(env, js_value, cpp_value)) return false;

        set.insert(std::move(cpp_value));
    }
    return true;
}

// ============================================================================
// std::tuple Support - Heterogeneous Fixed-Size Containers
// ============================================================================

// Helper to convert tuple elements to JavaScript array
template<typename Tuple, std::size_t... Is>
napi_value tuple_to_javascript_impl(napi_env env, const Tuple& t, std::index_sequence<Is...>) {
    napi_value array;
    napi_create_array_with_length(env, sizeof...(Is), &array);

    (void)std::initializer_list<int>{(
        napi_set_element(env, array, Is, to_javascript(env, std::get<Is>(t))),
        0
    )...};

    return array;
}

// Convert std::tuple to JavaScript array
template<typename... Ts>
napi_value to_javascript(napi_env env, const std::tuple<Ts...>& t) {
    return tuple_to_javascript_impl(env, t, std::index_sequence_for<Ts...>{});
}

// Helper to convert JavaScript array to tuple
template<typename Tuple, std::size_t... Is>
bool tuple_from_javascript_impl(napi_env env, napi_value value, Tuple& t, std::index_sequence<Is...>) {
    bool is_array;
    napi_is_array(env, value, &is_array);
    if (!is_array) return false;

    uint32_t length;
    napi_get_array_length(env, value, &length);
    if (length != sizeof...(Is)) return false;

    bool success = true;
    (void)std::initializer_list<int>{(
        [&]() {
            if (!success) return;
            napi_value elem;
            napi_get_element(env, value, Is, &elem);
            if (!from_javascript(env, elem, std::get<Is>(t))) {
                success = false;
            }
        }(), 0
    )...};

    return success;
}

// Convert JavaScript array to std::tuple
template<typename... Ts>
bool from_javascript(napi_env env, napi_value value, std::tuple<Ts...>& t) {
    return tuple_from_javascript_impl(env, value, t, std::index_sequence_for<Ts...>{});
}

// ============================================================================
// std::pair Support - Two-Element Tuple
// ============================================================================

// Convert std::pair to JavaScript array
template<typename T1, typename T2>
napi_value to_javascript(napi_env env, const std::pair<T1, T2>& p) {
    napi_value array;
    napi_create_array_with_length(env, 2, &array);

    napi_set_element(env, array, 0, to_javascript(env, p.first));
    napi_set_element(env, array, 1, to_javascript(env, p.second));

    return array;
}

// Convert JavaScript array to std::pair
template<typename T1, typename T2>
bool from_javascript(napi_env env, napi_value value, std::pair<T1, T2>& p) {
    bool is_array;
    napi_is_array(env, value, &is_array);
    if (!is_array) return false;

    uint32_t length;
    napi_get_array_length(env, value, &length);
    if (length != 2) return false;

    napi_value first, second;
    napi_get_element(env, value, 0, &first);
    napi_get_element(env, value, 1, &second);

    return from_javascript(env, first, p.first) && from_javascript(env, second, p.second);
}

// ============================================================================
// std::variant Support - Type-Safe Unions
// ============================================================================

// Convert std::variant to JavaScript (converts the active alternative)
template<typename... Ts>
napi_value to_javascript(napi_env env, const std::variant<Ts...>& v) {
    return std::visit([env](const auto& val) -> napi_value {
        return to_javascript(env, val);
    }, v);
}

// Helper to try converting JavaScript value to each variant alternative
template<typename Variant, typename T, typename... Rest>
bool try_js_variant_alternatives(napi_env env, napi_value value, Variant& v) {
    T cpp_value;
    if (from_javascript(env, value, cpp_value)) {
        v = std::move(cpp_value);
        return true;
    }

    if constexpr (sizeof...(Rest) > 0) {
        return try_js_variant_alternatives<Variant, Rest...>(env, value, v);
    }
    return false;
}

// Convert JavaScript to std::variant (tries each alternative in order)
template<typename... Ts>
bool from_javascript(napi_env env, napi_value value, std::variant<Ts...>& v) {
    return try_js_variant_alternatives<std::variant<Ts...>, Ts...>(env, value, v);
}

// ============================================================================
// std::optional Type Conversion
// ============================================================================

// std::optional to JavaScript (null if empty, otherwise convert value)
template<typename T>
napi_value to_javascript(napi_env env, const std::optional<T>& opt) {
    if (!opt.has_value()) {
        napi_value result;
        napi_get_null(env, &result);
        return result;
    }
    return to_javascript(env, *opt);
}

// JavaScript to std::optional (null/undefined → nullopt, otherwise convert value)
template<typename T>
bool from_javascript(napi_env env, napi_value value, std::optional<T>& out) {
    napi_valuetype type;
    if (napi_typeof(env, value, &type) != napi_ok) return false;

    if (type == napi_null || type == napi_undefined) {
        out = std::nullopt;
        return true;
    }

    T cpp_value;
    if (!from_javascript(env, value, cpp_value)) {
        return false;
    }
    out = std::move(cpp_value);
    return true;
}

// ============================================================================
// std::expected Type Conversion
// ============================================================================
//
// Enables conversion between C++ std::expected<T, E> and JavaScript values.
// On success: returns the converted T value.
// On error: throws a JavaScript Error with the error message.
//
// Example C++ code:
//   std::expected<double, std::string> safe_divide(double a, double b) {
//       if (b == 0.0) return std::unexpected("division by zero");
//       return a / b;
//   }
//
// JavaScript usage:
//   const result = obj.safe_divide(10.0, 2.0);  // Returns 5.0
//   try {
//       obj.safe_divide(10.0, 0.0);  // Throws Error("division by zero")
//   } catch (e) { console.error(e.message); }

// Helper trait to detect std::expected
template<typename T>
struct is_js_std_expected : std::false_type {};

template<typename T, typename E>
struct is_js_std_expected<std::expected<T, E>> : std::true_type {};

// std::expected to JavaScript (value on success, throw on error)
template<typename T, typename E>
napi_value to_javascript(napi_env env, const std::expected<T, E>& exp) {
    if (exp.has_value()) {
        if constexpr (std::is_void_v<T>) {
            napi_value result;
            napi_get_undefined(env, &result);
            return result;
        } else {
            return to_javascript(env, *exp);
        }
    }

    // Error case: throw a JavaScript Error, then return nullptr.
    // After napi_throw_error, the env is in a pending-exception state —
    // no further NAPI calls should be made before returning.
    if constexpr (std::is_same_v<std::remove_cvref_t<E>, std::string>) {
        napi_throw_error(env, nullptr, exp.error().c_str());
    } else if constexpr (std::is_arithmetic_v<std::remove_cvref_t<E>>) {
        std::string msg = "error code: " + std::to_string(exp.error());
        napi_throw_error(env, nullptr, msg.c_str());
    } else {
        napi_throw_error(env, nullptr, "expected contained an error");
    }
    return nullptr;
}

// JavaScript to std::expected (always produces success value)
template<typename T, typename E>
bool from_javascript(napi_env env, napi_value value, std::expected<T, E>& out) {
    if constexpr (std::is_void_v<T>) {
        out = std::expected<T, E>{};
        return true;
    } else {
        T cpp_value;
        if (!from_javascript(env, value, cpp_value)) {
            return false;
        }
        out = std::move(cpp_value);
        return true;
    }
}

// ============================================================================
// Custom Type Converter Extension Point
// ============================================================================
//
// Users can add conversion support for custom types by defining
// to_javascript/from_javascript overloads or specializing CustomJsConverter.
//
// Example:
//   namespace mirror_bridge::javascript {
//       napi_value to_javascript(napi_env env, const MyType& v) { ... }
//       bool from_javascript(napi_env env, napi_value val, MyType& v) { ... }
//   }

template<typename T, typename Enable = void>
struct CustomJsConverter {
    static constexpr bool has_custom_conversion = false;
};

template<typename T>
concept HasCustomJsConverter = requires {
    { CustomJsConverter<T>::has_custom_conversion } -> std::convertible_to<bool>;
} && CustomJsConverter<T>::has_custom_conversion;

template<typename T>
    requires HasCustomJsConverter<T>
napi_value to_javascript(napi_env env, const T& value) {
    return CustomJsConverter<T>::to_javascript(env, value);
}

template<typename T>
    requires HasCustomJsConverter<T>
bool from_javascript(napi_env env, napi_value val, T& value) {
    return CustomJsConverter<T>::from_javascript(env, val, value);
}

// ============================================================================
// Async/Await Support - std::future<T> to JavaScript Promise
// ============================================================================
//
// Enables C++ methods returning std::future<T> to return JavaScript Promises.
//
// Example C++ code:
//   std::future<int> compute_async(int x) {
//       return std::async(std::launch::async, [x]() {
//           // Simulate async work
//           return x * 2;
//       });
//   }
//
// JavaScript usage:
//   const result = await obj.compute_async(21);
//   console.log(result);  // 42
//
// The Promise wrapper uses N-API async work to poll the future without
// blocking the main JavaScript thread.

// Helper trait to detect std::future (if not already defined)
template<typename T>
struct is_js_std_future : std::false_type {};

template<typename T>
struct is_js_std_future<std::future<T>> : std::true_type {};

template<typename T>
struct is_js_std_future<std::shared_future<T>> : std::true_type {};

// Async work context for future polling
template<typename T>
struct FutureAsyncContext {
    napi_env env;
    napi_deferred deferred;
    napi_async_work work;
    std::shared_future<T>* future;
    T result;
    bool has_error;
    std::string error_message;
};

// Execute callback - runs on worker thread, waits for future
template<typename T>
void future_execute_callback(napi_env env, void* data) {
    auto* ctx = reinterpret_cast<FutureAsyncContext<T>*>(data);
    try {
        if constexpr (!std::is_void_v<T>) {
            ctx->result = ctx->future->get();
        } else {
            ctx->future->get();
        }
        ctx->has_error = false;
    } catch (const std::exception& e) {
        ctx->has_error = true;
        ctx->error_message = e.what();
    }
}

// Complete callback - runs on main thread, resolves/rejects promise
template<typename T>
void future_complete_callback(napi_env env, napi_status status, void* data) {
    auto* ctx = reinterpret_cast<FutureAsyncContext<T>*>(data);

    if (ctx->has_error) {
        // Reject with error
        napi_value error;
        napi_create_string_utf8(env, ctx->error_message.c_str(), NAPI_AUTO_LENGTH, &error);
        napi_reject_deferred(env, ctx->deferred, error);
    } else {
        // Resolve with result
        napi_value js_result;
        if constexpr (std::is_void_v<T>) {
            napi_get_undefined(env, &js_result);
        } else {
            js_result = to_javascript(env, ctx->result);
        }
        napi_resolve_deferred(env, ctx->deferred, js_result);
    }

    // Cleanup
    napi_delete_async_work(env, ctx->work);
    delete ctx->future;
    delete ctx;
}

// Convert std::future<T> to JavaScript Promise
template<typename T>
napi_value to_javascript(napi_env env, std::future<T>&& fut) {
    // Create Promise
    napi_value promise;
    napi_deferred deferred;
    napi_create_promise(env, &deferred, &promise);

    // Create async context
    auto* ctx = new FutureAsyncContext<T>();
    ctx->env = env;
    ctx->deferred = deferred;
    ctx->future = new std::shared_future<T>(std::move(fut));
    ctx->has_error = false;

    // Create async work
    napi_value resource_name;
    napi_create_string_utf8(env, "FutureAwait", NAPI_AUTO_LENGTH, &resource_name);

    napi_create_async_work(
        env,
        nullptr,
        resource_name,
        future_execute_callback<T>,
        future_complete_callback<T>,
        ctx,
        &ctx->work
    );

    // Queue the work
    napi_queue_async_work(env, ctx->work);

    return promise;
}

// Convert std::shared_future<T> to JavaScript Promise
template<typename T>
napi_value to_javascript(napi_env env, const std::shared_future<T>& fut) {
    // Create Promise
    napi_value promise;
    napi_deferred deferred;
    napi_create_promise(env, &deferred, &promise);

    // Create async context
    auto* ctx = new FutureAsyncContext<T>();
    ctx->env = env;
    ctx->deferred = deferred;
    ctx->future = new std::shared_future<T>(fut);
    ctx->has_error = false;

    // Create async work
    napi_value resource_name;
    napi_create_string_utf8(env, "FutureAwait", NAPI_AUTO_LENGTH, &resource_name);

    napi_create_async_work(
        env,
        nullptr,
        resource_name,
        future_execute_callback<T>,
        future_complete_callback<T>,
        ctx,
        &ctx->work
    );

    // Queue the work
    napi_queue_async_work(env, ctx->work);

    return promise;
}

// Forward declaration for JsWrapper (needed for from_javascript with wrapped objects)
template<typename T> struct JsWrapper;

// Type-based registry for looking up napi constructor by C++ type
template<typename T>
struct JsTypeRegistry {
    static inline napi_ref constructor_ref = nullptr;
    static inline napi_env cached_env = nullptr;
};

// Convert JavaScript wrapped objects to C++ types
// Handles const reference parameters like dot(const Vec3& other)
template<typename T>
    requires (std::is_class_v<std::remove_cvref_t<T>> &&
              !Arithmetic<T> && !StringLike<T> && !SmartPointer<T> && !Container<T>)
bool from_javascript(napi_env env, napi_value value, T& out) {
    using CleanT = std::remove_cvref_t<T>;

    // Try to unwrap as a JsWrapper
    JsWrapper<CleanT>* wrapper = nullptr;
    napi_status status = napi_unwrap(env, value, reinterpret_cast<void**>(&wrapper));

    if (status == napi_ok && wrapper && wrapper->cpp_object) {
        out = *wrapper->cpp_object;
        return true;
    }

    return false;
}

// ============================================================================
// Property Accessor (Getter)
// ============================================================================

template<typename T, std::size_t Index>
napi_value js_getter(napi_env env, napi_callback_info info) {
    napi_value this_arg;
    napi_get_cb_info(env, info, nullptr, nullptr, &this_arg, nullptr);

    JsWrapper<T>* wrapper;
    napi_unwrap(env, this_arg, reinterpret_cast<void**>(&wrapper));

    if (!wrapper || !wrapper->cpp_object) {
        napi_throw_error(env, nullptr, "Invalid C++ object");
        return nullptr;
    }

    constexpr auto member = get_data_member<T, Index>();
    using MemberType = typename [:std::meta::type_of(member):];

    try {
        auto& value = (*wrapper->cpp_object).[:member:];
        return to_javascript(env, value);
    } catch (const std::exception& e) {
        napi_throw_error(env, nullptr, e.what());
        return nullptr;
    } catch (...) {
        napi_throw_error(env, nullptr, "Unknown C++ exception in property getter");
        return nullptr;
    }
}

// ============================================================================
// Property Accessor (Setter)
// ============================================================================

template<typename T, std::size_t Index>
napi_value js_setter(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_value this_arg;
    napi_get_cb_info(env, info, &argc, args, &this_arg, nullptr);

    JsWrapper<T>* wrapper;
    napi_unwrap(env, this_arg, reinterpret_cast<void**>(&wrapper));

    if (!wrapper || !wrapper->cpp_object) {
        napi_throw_error(env, nullptr, "Invalid C++ object");
        return nullptr;
    }

    constexpr auto member = get_data_member<T, Index>();
    using MemberType = typename [:std::meta::type_of(member):];

    MemberType cpp_value;
    if (!from_javascript(env, args[0], cpp_value)) {
        napi_throw_error(env, nullptr, "Type conversion failed");
        return nullptr;
    }

    try {
        (*wrapper->cpp_object).[:member:] = std::move(cpp_value);
    } catch (const std::exception& e) {
        napi_throw_error(env, nullptr, e.what());
        return nullptr;
    } catch (...) {
        napi_throw_error(env, nullptr, "Unknown C++ exception in property setter");
        return nullptr;
    }

    napi_value undefined;
    napi_get_undefined(env, &undefined);
    return undefined;
}

// ============================================================================
// Method Binding
// ============================================================================

template<typename T, std::size_t FuncIndex, std::size_t... Is>
napi_value call_method_impl(napi_env env, JsWrapper<T>* wrapper, napi_value* args, std::index_sequence<Is...>) {
    constexpr auto member_func = get_member_function<T, FuncIndex>();
    constexpr auto return_type = get_method_return_type<T, FuncIndex>();
    using ReturnType = typename [:return_type:];

    std::tuple<std::remove_cvref_t<method_param_t<T, FuncIndex, Is>>...> cpp_args;

    bool success = true;
    ([&] {
        if (!success) return;
        if (!from_javascript(env, args[Is], std::get<Is>(cpp_args))) {
            success = false;
        }
    }(), ...);

    if (!success) {
        napi_throw_error(env, nullptr, "Argument type conversion failed");
        return nullptr;
    }

    try {
        if constexpr (std::is_void_v<ReturnType>) {
            ((*wrapper->cpp_object).[:member_func:])(std::move(std::get<Is>(cpp_args))...);
            napi_value undefined;
            napi_get_undefined(env, &undefined);
            return undefined;
        } else {
            ReturnType result = ((*wrapper->cpp_object).[:member_func:])(std::move(std::get<Is>(cpp_args))...);
            return to_javascript(env, result);
        }
    } catch (const std::exception& e) {
        napi_throw_error(env, nullptr, e.what());
        return nullptr;
    } catch (...) {
        napi_throw_error(env, nullptr, "Unknown C++ exception");
        return nullptr;
    }
}

template<typename T, std::size_t Index>
napi_value js_method(napi_env env, napi_callback_info info) {
    constexpr std::size_t param_count = get_method_param_count<T, Index>();

    size_t argc = param_count;
    napi_value args[param_count > 0 ? param_count : 1];
    napi_value this_arg;
    napi_get_cb_info(env, info, &argc, args, &this_arg, nullptr);

    JsWrapper<T>* wrapper;
    napi_unwrap(env, this_arg, reinterpret_cast<void**>(&wrapper));

    if (!wrapper || !wrapper->cpp_object) {
        napi_throw_error(env, nullptr, "Invalid C++ object");
        return nullptr;
    }

    if (argc != param_count) {
        napi_throw_error(env, nullptr, "Incorrect number of arguments");
        return nullptr;
    }

    return call_method_impl<T, Index>(env, wrapper, args, std::make_index_sequence<param_count>{});
}

// ============================================================================
// Static Method Binding
// ============================================================================

template<typename T, std::size_t FuncIndex, std::size_t... Is>
napi_value call_static_method_impl(napi_env env, napi_value* args, std::index_sequence<Is...>) {
    constexpr auto member_func = get_static_member_function<T, FuncIndex>();
    constexpr auto return_type = get_static_method_return_type<T, FuncIndex>();
    using ReturnType = typename [:return_type:];

    std::tuple<std::remove_cvref_t<static_method_param_t<T, FuncIndex, Is>>...> cpp_args;

    bool success = true;
    ([&] {
        if (!success) return;
        if (!from_javascript(env, args[Is], std::get<Is>(cpp_args))) {
            success = false;
        }
    }(), ...);

    if (!success) {
        napi_throw_error(env, nullptr, "Argument type conversion failed");
        return nullptr;
    }

    try {
        if constexpr (std::is_void_v<ReturnType>) {
            [:member_func:](std::move(std::get<Is>(cpp_args))...);
            napi_value undefined;
            napi_get_undefined(env, &undefined);
            return undefined;
        } else {
            ReturnType result = [:member_func:](std::move(std::get<Is>(cpp_args))...);
            return to_javascript(env, result);
        }
    } catch (const std::exception& e) {
        napi_throw_error(env, nullptr, e.what());
        return nullptr;
    } catch (...) {
        napi_throw_error(env, nullptr, "Unknown C++ exception");
        return nullptr;
    }
}

template<typename T, std::size_t Index>
napi_value js_static_method(napi_env env, napi_callback_info info) {
    constexpr std::size_t param_count = get_static_method_param_count<T, Index>();

    size_t argc = param_count;
    napi_value args[param_count > 0 ? param_count : 1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc != param_count) {
        napi_throw_error(env, nullptr, "Incorrect number of arguments");
        return nullptr;
    }

    return call_static_method_impl<T, Index>(env, args, std::make_index_sequence<param_count>{});
}

// ============================================================================
// Finalizer (Destructor)
// ============================================================================

template<typename T>
void js_finalizer(napi_env env, void* finalize_data, void* finalize_hint) {
    JsWrapper<T>* wrapper = static_cast<JsWrapper<T>*>(finalize_data);
    if (wrapper) {
        if (wrapper->owns_memory && wrapper->cpp_object) {
            delete wrapper->cpp_object;
        }
        delete wrapper;
    }
}

// ============================================================================
// Constructor Reflection Helpers
// ============================================================================

// Count parameterized constructors (exclude default, copy, move)
template<typename T>
consteval std::size_t get_js_constructor_count() {
    auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
    std::size_t count = 0;
    for (auto member : all_members) {
        if (std::meta::is_constructor(member) &&
            !std::meta::is_copy_constructor(member) &&
            !std::meta::is_move_constructor(member)) {
            auto params = std::meta::parameters_of(member);
            if (params.size() > 0) {
                count++;
            }
        }
    }
    return count;
}

// Get the Nth parameterized constructor
template<typename T, std::size_t Index>
consteval auto get_js_constructor() {
    auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
    std::size_t count = 0;
    for (auto member : all_members) {
        if (std::meta::is_constructor(member) &&
            !std::meta::is_copy_constructor(member) &&
            !std::meta::is_move_constructor(member)) {
            auto params = std::meta::parameters_of(member);
            if (params.size() > 0) {
                if (count == Index) {
                    return member;
                }
                count++;
            }
        }
    }
    return all_members[0]; // Should never reach here
}

// Get parameter count for a constructor
template<typename T, std::size_t CtorIndex>
consteval std::size_t get_js_constructor_param_count() {
    constexpr auto ctor = get_js_constructor<T, CtorIndex>();
    auto params = std::meta::parameters_of(ctor);
    return params.size();
}

// Get parameter type for a constructor
template<typename T, std::size_t CtorIndex, std::size_t ParamIndex>
consteval auto get_js_constructor_param_type() {
    constexpr auto ctor = get_js_constructor<T, CtorIndex>();
    auto params = std::meta::parameters_of(ctor);
    return std::meta::type_of(params[ParamIndex]);
}

// Alias-template form: pack expansions must use this instead of splicing
// inline (GCC rejects packs that appear only inside a splice; see the note
// in core/mirror_bridge_core.hpp).
template<typename T, std::size_t CtorIndex, std::size_t ParamIndex>
using js_constructor_param_t = typename [:get_js_constructor_param_type<T, CtorIndex, ParamIndex>():];

// Call constructor with JS arguments
template<typename T, std::size_t CtorIndex, std::size_t... Is>
T* call_js_constructor_impl(napi_env env, napi_value* args, std::index_sequence<Is...>, bool& success) {
    std::tuple<std::remove_cvref_t<js_constructor_param_t<T, CtorIndex, Is>>...> cpp_args;

    success = true;
    ([&] {
        if (!success) return;
        if (!from_javascript(env, args[Is], std::get<Is>(cpp_args))) {
            success = false;
        }
    }(), ...);

    if (!success) {
        return nullptr;
    }

    return new T(std::move(std::get<Is>(cpp_args))...);
}

// Try to match constructor by parameter count and call it
template<typename T, std::size_t CtorIndex>
T* try_js_constructor(napi_env env, napi_value* args, std::size_t nargs, bool& matched) {
    constexpr std::size_t param_count = get_js_constructor_param_count<T, CtorIndex>();
    if (nargs == param_count) {
        matched = true;
        bool success = true;
        T* result = call_js_constructor_impl<T, CtorIndex>(env, args, std::make_index_sequence<param_count>{}, success);
        if (success) {
            return result;
        }
    }
    matched = false;
    return nullptr;
}

// ============================================================================
// Constructor
// ============================================================================

template<typename T>
napi_value js_constructor(napi_env env, napi_callback_info info) {
    napi_value this_arg;
    size_t argc = 16; // Max expected args
    napi_value args[16];
    napi_get_cb_info(env, info, &argc, args, &this_arg, nullptr);

    T* cpp_object = nullptr;

    try {
        if (argc == 0) {
            cpp_object = new T();
        } else {
            constexpr std::size_t ctor_count = get_js_constructor_count<T>();

            if constexpr (ctor_count > 0) {
                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    ([&] {
                        if (cpp_object != nullptr) return;
                        bool matched = false;
                        T* result = try_js_constructor<T, Is>(env, args, argc, matched);
                        if (result != nullptr) {
                            cpp_object = result;
                        }
                    }(), ...);
                }(std::make_index_sequence<ctor_count>{});
            }

            if (cpp_object == nullptr) {
                cpp_object = new T();
            }
        }
    } catch (const std::exception& e) {
        napi_throw_error(env, nullptr, e.what());
        return nullptr;
    } catch (...) {
        napi_throw_error(env, nullptr, "Unknown C++ exception in constructor");
        return nullptr;
    }

    JsWrapper<T>* wrapper = new JsWrapper<T>();
    wrapper->cpp_object = cpp_object;
    wrapper->owns_memory = true;

    napi_wrap(env, this_arg, wrapper, js_finalizer<T>, nullptr, &wrapper->js_ref);

    return this_arg;
}

// ============================================================================
// Nested Bindable Conversion
// ============================================================================

template<typename T>
struct JsConversionHelper {
    static napi_value to_javascript_impl(napi_env env, const T& obj) {
        napi_value js_obj;
        napi_create_object(env, &js_obj);

        constexpr std::size_t member_count = get_data_member_count<T>();

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                constexpr auto member = get_data_member<T, Is>();
                constexpr auto name_sv = std::meta::identifier_of(member);
                constexpr auto name = name_sv.data();

                const auto& value = obj.[:member:];
                napi_value js_value = to_javascript(env, value);
                napi_set_named_property(env, js_obj, name, js_value);
            }(), ...);
        }(std::make_index_sequence<member_count>{});

        return js_obj;
    }

    static bool from_javascript_impl(napi_env env, napi_value js_obj, T& out) {
        constexpr std::size_t member_count = get_data_member_count<T>();
        bool success = true;

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (!success) return;

                constexpr auto member = get_data_member<T, Is>();
                constexpr auto name_sv = std::meta::identifier_of(member);
                constexpr auto name = name_sv.data();
                using MemberType = typename [:std::meta::type_of(member):];

                napi_value js_value;
                if (napi_get_named_property(env, js_obj, name, &js_value) != napi_ok) {
                    success = false;
                    return;
                }

                MemberType cpp_value;
                if (!from_javascript(env, js_value, cpp_value)) {
                    success = false;
                    return;
                }

                out.[:member:] = std::move(cpp_value);
            }(), ...);
        }(std::make_index_sequence<member_count>{});

        return success;
    }
};

template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    napi_value
>
to_javascript(napi_env env, const T& obj) {
    using CleanT = std::remove_cvref_t<T>;

    // Abstract / non-copy-assignable types can't be copied into a fresh
    // wrapper instance. Mirror the Python backend's graceful fallback:
    // emit an object snapshot of the members instead of failing to compile
    // (reachable via e.g. shared_ptr<AbstractBase> members).
    if constexpr (std::is_abstract_v<CleanT> || !std::is_copy_assignable_v<CleanT>) {
        return JsConversionHelper<T>::to_javascript_impl(env, obj);
    } else {
        // Check if this type has been registered with bind_class
        if (JsTypeRegistry<CleanT>::constructor_ref && JsTypeRegistry<CleanT>::cached_env == env) {
            // Get the constructor from the reference
            napi_value constructor;
            napi_get_reference_value(env, JsTypeRegistry<CleanT>::constructor_ref, &constructor);

            // Create a new instance
            napi_value instance;
            napi_new_instance(env, constructor, 0, nullptr, &instance);

            // Unwrap and copy the object
            JsWrapper<CleanT>* wrapper;
            napi_unwrap(env, instance, reinterpret_cast<void**>(&wrapper));
            if (wrapper && wrapper->cpp_object) {
                *wrapper->cpp_object = obj;
            }

            return instance;
        }

        // Fall back to object conversion for unregistered types
        return JsConversionHelper<T>::to_javascript_impl(env, obj);
    }
}

template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    bool
>
from_javascript(napi_env env, napi_value value, T& out) {
    return JsConversionHelper<T>::from_javascript_impl(env, value, out);
}

// ============================================================================
// Class Binding Function
// ============================================================================

template<Bindable T>
napi_value bind_class(napi_env env, napi_value exports, const char* name) {
    static_assert(core::validate_bindable_members<T>(),
        "bind_class<T>: T contains members with types that mirror_bridge cannot convert. "
        "Mark unconvertible members with [[=exclude{}]] or add a custom type converter.");

    constexpr std::size_t member_count = get_data_member_count<T>();
    constexpr std::size_t method_count = get_member_function_count<T>();
    constexpr std::size_t static_method_count = get_static_member_function_count<T>();

    // Create constructor
    napi_value constructor;
    napi_define_class(
        env,
        name,
        NAPI_AUTO_LENGTH,
        js_constructor<T>,
        nullptr,
        0,
        nullptr,
        &constructor
    );

    // Store constructor reference for type registry (used by to_javascript)
    napi_create_reference(env, constructor, 1, &JsTypeRegistry<T>::constructor_ref);
    JsTypeRegistry<T>::cached_env = env;

    // Add properties (getters/setters)
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr auto member_name_sv = std::meta::identifier_of(get_data_member<T, Is>());
            constexpr auto member_name = member_name_sv.data();

            napi_property_descriptor desc = {
                .utf8name = member_name,
                .getter = js_getter<T, Is>,
                .setter = js_setter<T, Is>,
                .attributes = napi_default
            };

            napi_value prototype;
            napi_get_named_property(env, constructor, "prototype", &prototype);
            napi_define_properties(env, prototype, 1, &desc);
        }(), ...);
    }(std::make_index_sequence<member_count>{});

    // Add instance methods
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr auto method_name_sv = std::meta::identifier_of(get_member_function<T, Is>());
            constexpr auto method_name = method_name_sv.data();

            napi_value fn;
            napi_create_function(env, method_name, NAPI_AUTO_LENGTH, js_method<T, Is>, nullptr, &fn);

            napi_value prototype;
            napi_get_named_property(env, constructor, "prototype", &prototype);
            napi_set_named_property(env, prototype, method_name, fn);
        }(), ...);
    }(std::make_index_sequence<method_count>{});

    // Add static methods directly to the constructor
    if constexpr (static_method_count > 0) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                constexpr auto method_name = get_static_member_function_name<T, Is>();

                napi_value fn;
                napi_create_function(env, method_name, NAPI_AUTO_LENGTH, js_static_method<T, Is>, nullptr, &fn);

                // Add to constructor (not prototype) for static methods
                napi_set_named_property(env, constructor, method_name, fn);
            }(), ...);
        }(std::make_index_sequence<static_method_count>{});
    }

    // Add constructor to exports
    napi_set_named_property(env, exports, name, constructor);

    return exports;
}

} // namespace javascript
} // namespace mirror_bridge

// ============================================================================
// Module Definition Macro
// ============================================================================

#define MIRROR_BRIDGE_JS_MODULE(module_name, ...) \
    napi_value Init(napi_env env, napi_value exports) { \
        auto m = exports; \
        __VA_ARGS__ \
        return exports; \
    } \
    NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
