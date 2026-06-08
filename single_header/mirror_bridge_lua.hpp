#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge - Lua Single Header
// ═══════════════════════════════════════════════════════════════════════════
//
// Auto-generated single-header version for Lua bindings
// Generated: $(date)
//
// This file contains:
// - Core reflection infrastructure (language-agnostic)
// - Lua C API bindings
//
// Usage:
//   #include "mirror_bridge_lua.hpp"
//   MIRROR_BRIDGE_LUA_MODULE(my_module,
//       mirror_bridge::lua::bind_class<MyClass>(L, "MyClass");
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
// LUA - Lua C API Bindings
// ============================================================================


// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge Lua - Lua Bindings for C++ Code via C++26 Reflection
// ═══════════════════════════════════════════════════════════════════════════
// Generates Lua bindings that expose C++ classes to Lua.

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <cstdio>
#include <cstring>
#include <optional>
#include <expected>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <tuple>
#include <utility>
#include <variant>

namespace mirror_bridge {
namespace lua {

// Import core concepts for convenience
using namespace core;

// ============================================================================
// Lua Wrapper for C++ Objects
// ============================================================================

template<typename T>
struct LuaWrapper {
    T* cpp_object;
    bool owns_memory;
};

// ============================================================================
// Type Conversion: C++ → Lua
// ============================================================================

// Arithmetic types
template<Arithmetic T>
void to_lua(lua_State* L, const T& value) {
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
        lua_pushboolean(L, value ? 1 : 0);
    } else if constexpr (std::is_floating_point_v<T>) {
        lua_pushnumber(L, static_cast<lua_Number>(value));
    } else {
        lua_pushinteger(L, static_cast<lua_Integer>(value));
    }
}

// Enum types
template<EnumType T>
void to_lua(lua_State* L, const T& value) {
    lua_pushinteger(L, static_cast<lua_Integer>(value));
}

// String types
template<StringLike T>
void to_lua(lua_State* L, const T& value) {
    using BaseType = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<BaseType, std::string> || std::is_same_v<BaseType, std::string_view>) {
        lua_pushlstring(L, value.data(), value.size());
    } else {
        lua_pushstring(L, value);
    }
}

// Containers → Lua tables (with numeric indices starting at 1)
template<Container T>
void to_lua(lua_State* L, const T& container) {
    lua_createtable(L, container.size(), 0);

    int index = 1;  // Lua arrays start at 1
    for (const auto& item : container) {
        to_lua(L, item);
        lua_rawseti(L, -2, index++);
    }
}

// Forward declaration for Bindable types. Must precede the SmartPointer
// overload: the pointee is typically a user class in the global namespace,
// so the dependent to_lua(L, *ptr) call below finds this only through the
// declaration context, never through ADL.
template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>
>
to_lua(lua_State* L, const T& obj);

// Smart pointers
template<SmartPointer T>
void to_lua(lua_State* L, const T& ptr) {
    if (!ptr) {
        lua_pushnil(L);
        return;
    }
    to_lua(L, *ptr);
}

// ============================================================================
// Type Conversion: Lua → C++
// ============================================================================

// Arithmetic types
template<Arithmetic T>
bool from_lua(lua_State* L, int idx, T& out) {
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
        if (!lua_isboolean(L, idx)) return false;
        out = lua_toboolean(L, idx) != 0;
    } else if constexpr (std::is_floating_point_v<T>) {
        if (!lua_isnumber(L, idx)) return false;
        out = static_cast<T>(lua_tonumber(L, idx));
    } else {
        if (!lua_isinteger(L, idx)) return false;
        out = static_cast<T>(lua_tointeger(L, idx));
    }
    return true;
}

// Enum types
template<EnumType T>
bool from_lua(lua_State* L, int idx, T& out) {
    if (!lua_isinteger(L, idx)) return false;
    out = static_cast<T>(lua_tointeger(L, idx));
    return true;
}

// String types
template<StringLike T>
bool from_lua(lua_State* L, int idx, T& out) {
    if (!lua_isstring(L, idx)) return false;

    size_t len;
    const char* str = lua_tolstring(L, idx, &len);

    using BaseType = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<BaseType, std::string>) {
        out = std::string(str, len);
    } else if constexpr (std::is_same_v<BaseType, std::string_view>) {
        out = std::string_view(str, len);
    } else {
        out = str;
    }
    return true;
}

// Containers from Lua tables
template<Container T>
bool from_lua(lua_State* L, int idx, T& container) {
    if (!lua_istable(L, idx)) return false;

    using ValueType = typename std::remove_cvref_t<T>::value_type;

    if constexpr (requires { container.clear(); }) {
        container.clear();
    }

    // Iterate over Lua table (numeric indices)
    int table_size = lua_rawlen(L, idx);
    if constexpr (requires { container.reserve(table_size); }) {
        container.reserve(table_size);
    }

    for (int i = 1; i <= table_size; ++i) {
        lua_rawgeti(L, idx, i);

        ValueType cpp_item;
        if (!from_lua(L, -1, cpp_item)) {
            lua_pop(L, 1);
            return false;
        }

        if constexpr (requires { container.push_back(cpp_item); }) {
            container.push_back(std::move(cpp_item));
        } else if constexpr (requires { container.insert(cpp_item); }) {
            container.insert(std::move(cpp_item));
        }

        lua_pop(L, 1);
    }

    return true;
}

// Forward declaration for Bindable types. Must precede the SmartPointer
// overload for the same lookup reason as to_lua above: the pointee class
// lives in the global namespace, so ADL can't find a later declaration.
template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    bool
>
from_lua(lua_State* L, int idx, T& out);

// Smart pointers from Lua
template<SmartPointer T>
bool from_lua(lua_State* L, int idx, T& out) {
    using ElementType = typename std::remove_cvref_t<T>::element_type;

    if (lua_isnil(L, idx)) {
        out.reset();
        return true;
    }

    // Mirrors the Python backend: an abstract or non-default-constructible
    // pointee can't be materialized from a Lua table by value, and trying
    // would be a compile error. Only nil→reset is supported for those.
    if constexpr (std::is_abstract_v<ElementType> ||
                  !std::is_default_constructible_v<ElementType> ||
                  !std::is_copy_assignable_v<ElementType>) {
        return true;
    } else {
        ElementType value;
        if (!from_lua(L, idx, value)) return false;

        if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::unique_ptr<ElementType>>) {
            out = std::make_unique<ElementType>(std::move(value));
        } else {
            out = std::make_shared<ElementType>(std::move(value));
        }
        return true;
    }
}

// ============================================================================
// std::map / std::unordered_map Support - Associative Containers
// ============================================================================

// Convert std::map to Lua table
template<typename K, typename V, typename... Args>
void to_lua(lua_State* L, const std::map<K, V, Args...>& map) {
    lua_createtable(L, 0, map.size());

    for (const auto& [key, value] : map) {
        to_lua(L, key);
        to_lua(L, value);
        lua_settable(L, -3);
    }
}

// Convert std::unordered_map to Lua table
template<typename K, typename V, typename... Args>
void to_lua(lua_State* L, const std::unordered_map<K, V, Args...>& map) {
    lua_createtable(L, 0, map.size());

    for (const auto& [key, value] : map) {
        to_lua(L, key);
        to_lua(L, value);
        lua_settable(L, -3);
    }
}

// Convert Lua table to std::map
template<typename K, typename V, typename... Args>
bool from_lua(lua_State* L, int idx, std::map<K, V, Args...>& map) {
    if (!lua_istable(L, idx)) return false;

    map.clear();

    lua_pushnil(L);  // First key
    while (lua_next(L, idx) != 0) {
        K cpp_key;
        V cpp_value;

        // Value is at -1, key is at -2
        if (!from_lua(L, -2, cpp_key) || !from_lua(L, -1, cpp_value)) {
            lua_pop(L, 2);  // Pop key and value
            return false;
        }

        map[std::move(cpp_key)] = std::move(cpp_value);
        lua_pop(L, 1);  // Pop value, keep key for next iteration
    }
    return true;
}

// Convert Lua table to std::unordered_map
template<typename K, typename V, typename... Args>
bool from_lua(lua_State* L, int idx, std::unordered_map<K, V, Args...>& map) {
    if (!lua_istable(L, idx)) return false;

    map.clear();

    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        K cpp_key;
        V cpp_value;

        if (!from_lua(L, -2, cpp_key) || !from_lua(L, -1, cpp_value)) {
            lua_pop(L, 2);
            return false;
        }

        map[std::move(cpp_key)] = std::move(cpp_value);
        lua_pop(L, 1);
    }
    return true;
}

// ============================================================================
// std::set / std::unordered_set Support - Set Containers
// ============================================================================

// Convert std::set to Lua table (as array with sequential indices)
template<typename V, typename... Args>
void to_lua(lua_State* L, const std::set<V, Args...>& set) {
    lua_createtable(L, set.size(), 0);

    int index = 1;
    for (const auto& value : set) {
        to_lua(L, value);
        lua_rawseti(L, -2, index++);
    }
}

// Convert std::unordered_set to Lua table
template<typename V, typename... Args>
void to_lua(lua_State* L, const std::unordered_set<V, Args...>& set) {
    lua_createtable(L, set.size(), 0);

    int index = 1;
    for (const auto& value : set) {
        to_lua(L, value);
        lua_rawseti(L, -2, index++);
    }
}

// Convert Lua table (array) to std::set
template<typename V, typename... Args>
bool from_lua(lua_State* L, int idx, std::set<V, Args...>& set) {
    if (!lua_istable(L, idx)) return false;

    set.clear();

    int table_size = lua_rawlen(L, idx);
    for (int i = 1; i <= table_size; ++i) {
        lua_rawgeti(L, idx, i);

        V cpp_value;
        if (!from_lua(L, -1, cpp_value)) {
            lua_pop(L, 1);
            return false;
        }
        set.insert(std::move(cpp_value));
        lua_pop(L, 1);
    }
    return true;
}

// Convert Lua table to std::unordered_set
template<typename V, typename... Args>
bool from_lua(lua_State* L, int idx, std::unordered_set<V, Args...>& set) {
    if (!lua_istable(L, idx)) return false;

    set.clear();

    int table_size = lua_rawlen(L, idx);
    for (int i = 1; i <= table_size; ++i) {
        lua_rawgeti(L, idx, i);

        V cpp_value;
        if (!from_lua(L, -1, cpp_value)) {
            lua_pop(L, 1);
            return false;
        }
        set.insert(std::move(cpp_value));
        lua_pop(L, 1);
    }
    return true;
}

// ============================================================================
// std::tuple Support - Heterogeneous Fixed-Size Containers
// ============================================================================

// Helper to convert tuple elements to Lua table
template<typename Tuple, std::size_t... Is>
void tuple_to_lua_impl(lua_State* L, const Tuple& t, std::index_sequence<Is...>) {
    lua_createtable(L, sizeof...(Is), 0);
    (void)std::initializer_list<int>{(
        to_lua(L, std::get<Is>(t)),
        lua_rawseti(L, -2, Is + 1),  // Lua arrays are 1-indexed
        0
    )...};
}

// Convert std::tuple to Lua table
template<typename... Ts>
void to_lua(lua_State* L, const std::tuple<Ts...>& t) {
    tuple_to_lua_impl(L, t, std::index_sequence_for<Ts...>{});
}

// Helper to convert Lua table to tuple
template<typename Tuple, std::size_t... Is>
bool tuple_from_lua_impl(lua_State* L, int idx, Tuple& t, std::index_sequence<Is...>) {
    if (!lua_istable(L, idx)) return false;

    int table_size = lua_rawlen(L, idx);
    if (static_cast<std::size_t>(table_size) != sizeof...(Is)) return false;

    bool success = true;
    (void)std::initializer_list<int>{(
        [&]() {
            if (!success) return;
            lua_rawgeti(L, idx, Is + 1);  // Lua arrays are 1-indexed
            if (!from_lua(L, -1, std::get<Is>(t))) {
                success = false;
            }
            lua_pop(L, 1);
        }(), 0
    )...};

    return success;
}

// Convert Lua table to std::tuple
template<typename... Ts>
bool from_lua(lua_State* L, int idx, std::tuple<Ts...>& t) {
    return tuple_from_lua_impl(L, idx, t, std::index_sequence_for<Ts...>{});
}

// ============================================================================
// std::pair Support - Two-Element Tuple
// ============================================================================

// Convert std::pair to Lua table
template<typename T1, typename T2>
void to_lua(lua_State* L, const std::pair<T1, T2>& p) {
    lua_createtable(L, 2, 0);
    to_lua(L, p.first);
    lua_rawseti(L, -2, 1);
    to_lua(L, p.second);
    lua_rawseti(L, -2, 2);
}

// Convert Lua table to std::pair
template<typename T1, typename T2>
bool from_lua(lua_State* L, int idx, std::pair<T1, T2>& p) {
    if (!lua_istable(L, idx)) return false;

    int table_size = lua_rawlen(L, idx);
    if (table_size != 2) return false;

    lua_rawgeti(L, idx, 1);
    if (!from_lua(L, -1, p.first)) {
        lua_pop(L, 1);
        return false;
    }
    lua_pop(L, 1);

    lua_rawgeti(L, idx, 2);
    if (!from_lua(L, -1, p.second)) {
        lua_pop(L, 1);
        return false;
    }
    lua_pop(L, 1);

    return true;
}

// ============================================================================
// std::variant Support - Type-Safe Unions
// ============================================================================

// Convert std::variant to Lua (converts the active alternative)
template<typename... Ts>
void to_lua(lua_State* L, const std::variant<Ts...>& v) {
    std::visit([L](const auto& val) {
        to_lua(L, val);
    }, v);
}

// Helper to try converting Lua value to each variant alternative
template<typename Variant, typename T, typename... Rest>
bool try_lua_variant_alternatives(lua_State* L, int idx, Variant& v) {
    T value;
    if (from_lua(L, idx, value)) {
        v = std::move(value);
        return true;
    }

    if constexpr (sizeof...(Rest) > 0) {
        return try_lua_variant_alternatives<Variant, Rest...>(L, idx, v);
    }
    return false;
}

// Convert Lua to std::variant (tries each alternative in order)
template<typename... Ts>
bool from_lua(lua_State* L, int idx, std::variant<Ts...>& v) {
    return try_lua_variant_alternatives<std::variant<Ts...>, Ts...>(L, idx, v);
}

// ============================================================================
// std::optional Type Conversion
// ============================================================================

// Helper trait to detect std::optional (if not already defined)
template<typename T>
struct is_lua_std_optional : std::false_type {};

template<typename T>
struct is_lua_std_optional<std::optional<T>> : std::true_type {};

// std::optional to Lua (nil if empty, otherwise convert value)
template<typename T>
void to_lua(lua_State* L, const std::optional<T>& opt) {
    if (!opt.has_value()) {
        lua_pushnil(L);
        return;
    }
    to_lua(L, *opt);
}

// Lua to std::optional (nil → nullopt, otherwise convert value)
template<typename T>
bool from_lua(lua_State* L, int idx, std::optional<T>& out) {
    if (lua_isnil(L, idx)) {
        out = std::nullopt;
        return true;
    }

    T value;
    if (!from_lua(L, idx, value)) {
        return false;
    }
    out = std::move(value);
    return true;
}

// ============================================================================
// std::expected Type Conversion
// ============================================================================
//
// Enables conversion between C++ std::expected<T, E> and Lua values using
// idiomatic Lua multi-return: value, err.
//
// On success: pushes the value and nil (two return values)
// On error: pushes nil and the error message (two return values)
//
// This follows the standard Lua error convention used by io.open, pcall, etc.
//
// Example C++ code:
//   std::expected<double, std::string> safe_sqrt(double x) {
//       if (x < 0) return std::unexpected("cannot take sqrt of negative number");
//       return std::sqrt(x);
//   }
//
// Lua usage:
//   local result, err = obj:safe_sqrt(4.0)
//   if err then print("Error: " .. err)
//   else print("Result: " .. result) end

// Helper trait to detect std::expected
template<typename T>
struct is_lua_std_expected : std::false_type {};

template<typename T, typename E>
struct is_lua_std_expected<std::expected<T, E>> : std::true_type {};

// std::expected to Lua — pushes two values (value, nil) or (nil, error)
// Returns the number of values pushed (always 2)
template<typename T, typename E>
int to_lua_expected(lua_State* L, const std::expected<T, E>& exp) {
    if (exp.has_value()) {
        if constexpr (std::is_void_v<T>) {
            lua_pushboolean(L, 1);  // true for void success
        } else {
            to_lua(L, *exp);
        }
        lua_pushnil(L);  // no error
        return 2;
    }

    // Error case: push nil, then error
    lua_pushnil(L);
    if constexpr (std::is_same_v<std::remove_cvref_t<E>, std::string>) {
        lua_pushstring(L, exp.error().c_str());
    } else if constexpr (std::is_arithmetic_v<std::remove_cvref_t<E>>) {
        std::string msg = "error code: " + std::to_string(exp.error());
        lua_pushstring(L, msg.c_str());
    } else {
        lua_pushstring(L, "expected contained an error");
    }
    return 2;
}

// Lua to std::expected (always produces success value; Lua errors are separate)
template<typename T, typename E>
bool from_lua(lua_State* L, int idx, std::expected<T, E>& out) {
    if constexpr (std::is_void_v<T>) {
        out = std::expected<T, E>{};
        return true;
    } else {
        T value;
        if (!from_lua(L, idx, value)) {
            return false;
        }
        out = std::move(value);
        return true;
    }
}

// ============================================================================
// Custom Type Converter Extension Point
// ============================================================================
//
// Users can add conversion support for custom types by defining to_lua/from_lua
// overloads in the mirror_bridge::lua namespace, or by specializing
// CustomLuaConverter.
//
// Example:
//   namespace mirror_bridge::lua {
//       void to_lua(lua_State* L, const MyType& v) { ... }
//       bool from_lua(lua_State* L, int idx, MyType& v) { ... }
//   }

template<typename T, typename Enable = void>
struct CustomLuaConverter {
    static constexpr bool has_custom_conversion = false;
};

template<typename T>
concept HasCustomLuaConverter = requires {
    { CustomLuaConverter<T>::has_custom_conversion } -> std::convertible_to<bool>;
} && CustomLuaConverter<T>::has_custom_conversion;

template<typename T>
    requires HasCustomLuaConverter<T>
void to_lua(lua_State* L, const T& value) {
    CustomLuaConverter<T>::to_lua(L, value);
}

template<typename T>
    requires HasCustomLuaConverter<T>
bool from_lua(lua_State* L, int idx, T& value) {
    return CustomLuaConverter<T>::from_lua(L, idx, value);
}

// Forward declaration for LuaWrapper (needed for from_lua with wrapped objects)
template<typename T> struct LuaWrapper;

// Type-based registry for looking up metatable name by C++ type
template<typename T>
struct LuaTypeRegistry {
    static inline const char* metatable_name = nullptr;
};

// Convert Lua wrapped objects or tables to C++ types
// Handles const reference parameters like dot(const Vec3& other)
// Also handles Lua tables for nested struct assignment
template<typename T>
    requires (std::is_class_v<std::remove_cvref_t<T>> &&
              !Arithmetic<T> && !StringLike<T> && !SmartPointer<T> && !Container<T>)
bool from_lua(lua_State* L, int idx, T& out) {
    using CleanT = std::remove_cvref_t<T>;

    // Get as userdata (wrapped C++ object)
    if (lua_isuserdata(L, idx)) {
        LuaWrapper<CleanT>* wrapper = static_cast<LuaWrapper<CleanT>*>(lua_touserdata(L, idx));
        if (wrapper && wrapper->cpp_object) {
            out = *wrapper->cpp_object;
            return true;
        }
    }

    // Also support Lua tables for nested struct assignment
    // e.g., person.address = {street = "123 Main", city = "NYC", zip = 10001}
    if (lua_istable(L, idx)) {
        constexpr std::size_t member_count = core::get_data_member_count<CleanT>();
        bool success = true;

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (!success) return;

                constexpr auto member = core::get_data_member<CleanT, Is>();
                constexpr auto member_name = std::meta::identifier_of(member);
                using MemberType = typename [:std::meta::type_of(member):];

                // Get field from Lua table
                lua_getfield(L, idx, member_name.data());

                if (!lua_isnil(L, -1)) {
                    MemberType value;
                    if (!from_lua(L, -1, value)) {
                        success = false;
                    } else {
                        out.[:member:] = std::move(value);
                    }
                }

                lua_pop(L, 1);
            }(), ...);
        }(std::make_index_sequence<member_count>{});

        return success;
    }

    return false;
}

// ============================================================================
// Forward Declarations
// ============================================================================

template<typename T, std::size_t Index>
int lua_method(lua_State* L);

// ============================================================================
// Property Access via Metatables
// ============================================================================

// No optimized property accessors - keep using reflection-based __index/__newindex

template<typename T>
int lua_index(lua_State* L) {
    // L[1] = userdata (wrapper), L[2] = key (field name)
    LuaWrapper<T>* wrapper = static_cast<LuaWrapper<T>*>(lua_touserdata(L, 1));
    if (!wrapper || !wrapper->cpp_object) {
        return luaL_error(L, "Invalid C++ object");
    }

    const char* key = lua_tostring(L, 2);
    if (!key) return 0;

    // Use reflection to find matching member
    constexpr std::size_t member_count = get_data_member_count<T>();
    bool found = false;

    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            if (found) return;
            constexpr auto member_name_sv = std::meta::identifier_of(get_data_member<T, Is>());
            constexpr auto member_name = member_name_sv.data();

            if (std::strcmp(key, member_name) == 0) {
                constexpr auto member = get_data_member<T, Is>();
                const auto& value = (*wrapper->cpp_object).[:member:];
                to_lua(L, value);
                found = true;
            }
        }(), ...);
    }(std::make_index_sequence<member_count>{});

    if (!found) {
        // Check for methods
        constexpr std::size_t method_count = get_member_function_count<T>();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (found) return;
                constexpr auto method_name_sv = std::meta::identifier_of(get_member_function<T, Is>());
                constexpr auto method_name = method_name_sv.data();

                if (std::strcmp(key, method_name) == 0) {
                    // Push a closure that captures the method index
                    lua_pushinteger(L, Is);
                    lua_pushcclosure(L, lua_method<T, Is>, 1);
                    found = true;
                }
            }(), ...);
        }(std::make_index_sequence<method_count>{});
    }

    return found ? 1 : 0;
}

template<typename T>
int lua_newindex(lua_State* L) {
    // L[1] = userdata (wrapper), L[2] = key (field name), L[3] = value
    LuaWrapper<T>* wrapper = static_cast<LuaWrapper<T>*>(lua_touserdata(L, 1));
    if (!wrapper || !wrapper->cpp_object) {
        return luaL_error(L, "Invalid C++ object");
    }

    const char* key = lua_tostring(L, 2);
    if (!key) return 0;

    // Use reflection to find matching member
    constexpr std::size_t member_count = get_data_member_count<T>();
    bool found = false;

    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            if (found) return;
            constexpr auto member_name_sv = std::meta::identifier_of(get_data_member<T, Is>());
            constexpr auto member_name = member_name_sv.data();

            if (std::strcmp(key, member_name) == 0) {
                constexpr auto member = get_data_member<T, Is>();
                using MemberType = typename [:std::meta::type_of(member):];

                MemberType cpp_value;
                if (!from_lua(L, 3, cpp_value)) {
                    luaL_error(L, "Type conversion failed for field %s", key);
                    return;
                }

                (*wrapper->cpp_object).[:member:] = std::move(cpp_value);
                found = true;
            }
        }(), ...);
    }(std::make_index_sequence<member_count>{});

    if (!found) {
        luaL_error(L, "Unknown field: %s", key);
    }

    return 0;
}

// ============================================================================
// Method Binding
// ============================================================================

template<typename T, std::size_t FuncIndex, std::size_t... Is>
int call_method_impl(lua_State* L, LuaWrapper<T>* wrapper, std::index_sequence<Is...>) {
    constexpr auto member_func = get_member_function<T, FuncIndex>();
    constexpr auto return_type = get_method_return_type<T, FuncIndex>();
    using ReturnType = typename [:return_type:];

    std::tuple<std::remove_cvref_t<method_param_t<T, FuncIndex, Is>>...> cpp_args;

    bool success = true;
    ([&] {
        if (!success) return;
        // Lua stack: [1]=self, [2]=arg1, [3]=arg2, etc.
        if (!from_lua(L, 2 + Is, std::get<Is>(cpp_args))) {
            success = false;
        }
    }(), ...);

    if (!success) {
        return luaL_error(L, "Argument type conversion failed");
    }

    try {
        if constexpr (std::is_void_v<ReturnType>) {
            ((*wrapper->cpp_object).[:member_func:])(std::move(std::get<Is>(cpp_args))...);
            return 0;
        } else {
            // Handle std::expected return types with idiomatic Lua multi-return
            using CleanReturn = std::remove_cvref_t<ReturnType>;
            if constexpr (is_lua_std_expected<CleanReturn>::value) {
                auto result = ((*wrapper->cpp_object).[:member_func:])(std::move(std::get<Is>(cpp_args))...);
                return to_lua_expected(L, result);
            } else {
                ReturnType result = ((*wrapper->cpp_object).[:member_func:])(std::move(std::get<Is>(cpp_args))...);
                to_lua(L, result);
                return 1;
            }
        }
    } catch (const std::exception& e) {
        return luaL_error(L, "C++ exception: %s", e.what());
    } catch (...) {
        return luaL_error(L, "Unknown C++ exception");
    }
}

template<typename T, std::size_t Index>
int lua_method(lua_State* L) {
    LuaWrapper<T>* wrapper = static_cast<LuaWrapper<T>*>(lua_touserdata(L, 1));
    if (!wrapper || !wrapper->cpp_object) {
        return luaL_error(L, "Invalid C++ object");
    }

    constexpr std::size_t param_count = get_method_param_count<T, Index>();

    // Check argument count (excluding self)
    int nargs = lua_gettop(L) - 1;
    if (nargs != static_cast<int>(param_count)) {
        return luaL_error(L, "Incorrect number of arguments");
    }

    return call_method_impl<T, Index>(L, wrapper, std::make_index_sequence<param_count>{});
}

// ============================================================================
// Static Method Binding
// ============================================================================

template<typename T, std::size_t FuncIndex, std::size_t... Is>
int call_static_method_impl(lua_State* L, std::index_sequence<Is...>) {
    constexpr auto member_func = get_static_member_function<T, FuncIndex>();
    constexpr auto return_type = get_static_method_return_type<T, FuncIndex>();
    using ReturnType = typename [:return_type:];

    std::tuple<std::remove_cvref_t<static_method_param_t<T, FuncIndex, Is>>...> cpp_args;

    bool success = true;
    ([&] {
        if (!success) return;
        // Static methods: args start at index 1 (no self)
        if (!from_lua(L, 1 + Is, std::get<Is>(cpp_args))) {
            success = false;
        }
    }(), ...);

    if (!success) {
        return luaL_error(L, "Argument type conversion failed");
    }

    try {
        if constexpr (std::is_void_v<ReturnType>) {
            [:member_func:](std::move(std::get<Is>(cpp_args))...);
            return 0;
        } else {
            using CleanReturn = std::remove_cvref_t<ReturnType>;
            if constexpr (is_lua_std_expected<CleanReturn>::value) {
                auto result = [:member_func:](std::move(std::get<Is>(cpp_args))...);
                return to_lua_expected(L, result);
            } else {
                ReturnType result = [:member_func:](std::move(std::get<Is>(cpp_args))...);
                to_lua(L, result);
                return 1;
            }
        }
    } catch (const std::exception& e) {
        return luaL_error(L, "C++ exception: %s", e.what());
    } catch (...) {
        return luaL_error(L, "Unknown C++ exception");
    }
}

template<typename T, std::size_t Index>
int lua_static_method(lua_State* L) {
    constexpr std::size_t param_count = get_static_method_param_count<T, Index>();

    // Check argument count
    int nargs = lua_gettop(L);
    if (nargs != static_cast<int>(param_count)) {
        return luaL_error(L, "Incorrect number of arguments (expected %d, got %d)",
                          static_cast<int>(param_count), nargs);
    }

    return call_static_method_impl<T, Index>(L, std::make_index_sequence<param_count>{});
}

// ============================================================================
// Garbage Collection
// ============================================================================

template<typename T>
int lua_gc(lua_State* L) {
    LuaWrapper<T>* wrapper = static_cast<LuaWrapper<T>*>(lua_touserdata(L, 1));
    if (wrapper && wrapper->owns_memory && wrapper->cpp_object) {
        delete wrapper->cpp_object;
    }
    return 0;
}

// ============================================================================
// Constructor Support (Parameterized)
// ============================================================================

// Count constructors (exclude default, copy, move)
template<typename T>
consteval std::size_t get_lua_constructor_count() {
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

// Get the Nth non-default constructor
template<typename T, std::size_t Index>
consteval auto get_lua_constructor() {
    auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
    std::size_t ctor_index = 0;
    for (auto member : all_members) {
        if (std::meta::is_constructor(member) &&
            !std::meta::is_copy_constructor(member) &&
            !std::meta::is_move_constructor(member)) {
            auto params = std::meta::parameters_of(member);
            if (params.size() > 0) {
                if (ctor_index == Index) {
                    return member;
                }
                ctor_index++;
            }
        }
    }
    return all_members[0];
}

// Get constructor parameter count
template<typename T, std::size_t CtorIndex>
consteval std::size_t get_lua_constructor_param_count() {
    constexpr auto ctor = get_lua_constructor<T, CtorIndex>();
    return std::meta::parameters_of(ctor).size();
}

// Get constructor parameter type
template<typename T, std::size_t CtorIndex, std::size_t ParamIndex>
consteval auto get_lua_constructor_param_type() {
    constexpr auto ctor = get_lua_constructor<T, CtorIndex>();
    auto params = std::meta::parameters_of(ctor);
    return std::meta::type_of(params[ParamIndex]);
}

// Alias-template form: pack expansions must use this instead of splicing
// inline (GCC rejects packs that appear only inside a splice; see the note
// in core/mirror_bridge_core.hpp).
template<typename T, std::size_t CtorIndex, std::size_t ParamIndex>
using lua_constructor_param_t = typename [:get_lua_constructor_param_type<T, CtorIndex, ParamIndex>():];

// Call constructor with Lua arguments
template<typename T, std::size_t CtorIndex, std::size_t... Is>
T* call_lua_constructor_impl(lua_State* L, int arg_offset, std::index_sequence<Is...>) {
    using ParamTypes = std::tuple<
        std::remove_cvref_t<lua_constructor_param_t<T, CtorIndex, Is>>...
    >;

    std::tuple<std::remove_cvref_t<lua_constructor_param_t<T, CtorIndex, Is>>...> cpp_args;

    bool success = true;
    ([&] {
        if (!success) return;
        // Lua stack indices are 1-based, and we skip the class table (arg_offset accounts for this)
        int lua_idx = arg_offset + Is + 1;
        if (!from_lua(L, lua_idx, std::get<Is>(cpp_args))) {
            success = false;
        }
    }(), ...);

    if (!success) {
        return nullptr;
    }

    return new T(std::move(std::get<Is>(cpp_args))...);
}

// ============================================================================
// Constructor
// ============================================================================

template<typename T>
int lua_constructor(lua_State* L) {
    int nargs = lua_gettop(L) - 1;

    T* cpp_object = nullptr;

    try {
        if (nargs == 0) {
            if constexpr (std::is_default_constructible_v<T>) {
                cpp_object = new T();
            } else {
                return luaL_error(L, "This class requires constructor arguments");
            }
        } else {
            constexpr std::size_t ctor_count = get_lua_constructor_count<T>();

            if constexpr (ctor_count > 0) {
                bool found = false;
                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    ([&] {
                        if (found) return;

                        constexpr std::size_t param_count = get_lua_constructor_param_count<T, Is>();
                        if (nargs == static_cast<int>(param_count)) {
                            T* obj = call_lua_constructor_impl<T, Is>(L, 1,
                                std::make_index_sequence<param_count>{});

                            if (obj) {
                                cpp_object = obj;
                                found = true;
                            }
                        }
                    }(), ...);
                }(std::make_index_sequence<ctor_count>{});

                if (!found && !cpp_object) {
                    if constexpr (std::is_default_constructible_v<T>) {
                        cpp_object = new T();
                    } else {
                        return luaL_error(L, "No matching constructor found for %d arguments", nargs);
                    }
                }
            } else {
                if constexpr (std::is_default_constructible_v<T>) {
                    cpp_object = new T();
                } else {
                    return luaL_error(L, "This class has no constructors accepting arguments");
                }
            }
        }
    } catch (const std::exception& e) {
        return luaL_error(L, "C++ constructor exception: %s", e.what());
    } catch (...) {
        return luaL_error(L, "Unknown C++ exception in constructor");
    }

    // Allocate userdata for wrapper
    LuaWrapper<T>* wrapper = static_cast<LuaWrapper<T>*>(lua_newuserdata(L, sizeof(LuaWrapper<T>)));

    wrapper->cpp_object = cpp_object;
    wrapper->owns_memory = true;

    // Set metatable
    luaL_getmetatable(L, typeid(T).name());
    lua_setmetatable(L, -2);

    return 1;
}

// ============================================================================
// Nested Bindable Conversion
// ============================================================================

template<typename T>
struct LuaConversionHelper {
    static void to_lua_impl(lua_State* L, const T& obj) {
        lua_createtable(L, 0, get_data_member_count<T>());

        constexpr std::size_t member_count = get_data_member_count<T>();

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                constexpr auto member = get_data_member<T, Is>();
                constexpr auto name_sv = std::meta::identifier_of(member);
                constexpr auto name = name_sv.data();

                const auto& value = obj.[:member:];
                to_lua(L, value);
                lua_setfield(L, -2, name);
            }(), ...);
        }(std::make_index_sequence<member_count>{});
    }

    static bool from_lua_impl(lua_State* L, int idx, T& out) {
        if (!lua_istable(L, idx)) return false;

        constexpr std::size_t member_count = get_data_member_count<T>();
        bool success = true;

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (!success) return;

                constexpr auto member = get_data_member<T, Is>();
                constexpr auto name_sv = std::meta::identifier_of(member);
                constexpr auto name = name_sv.data();
                using MemberType = typename [:std::meta::type_of(member):];

                lua_getfield(L, idx, name);

                MemberType cpp_value;
                if (!from_lua(L, -1, cpp_value)) {
                    success = false;
                    lua_pop(L, 1);
                    return;
                }

                out.[:member:] = std::move(cpp_value);
                lua_pop(L, 1);
            }(), ...);
        }(std::make_index_sequence<member_count>{});

        return success;
    }
};

template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>
>
to_lua(lua_State* L, const T& obj) {
    using CleanT = std::remove_cvref_t<T>;

    // Abstract / non-copyable types can't be copied into a userdata
    // wrapper. Mirror the Python backend's graceful fallback: emit a table
    // snapshot of the members instead of failing to compile (reachable via
    // e.g. shared_ptr<AbstractBase> members).
    if constexpr (std::is_abstract_v<CleanT> || !std::is_copy_constructible_v<CleanT>) {
        LuaConversionHelper<T>::to_lua_impl(L, obj);
    } else {
        // Check if this type has been registered with bind_class
        if (LuaTypeRegistry<CleanT>::metatable_name) {
            // Create a new userdata wrapper
            LuaWrapper<CleanT>* wrapper = static_cast<LuaWrapper<CleanT>*>(
                lua_newuserdata(L, sizeof(LuaWrapper<CleanT>)));

            // Copy the C++ object
            wrapper->cpp_object = new CleanT(obj);
            wrapper->owns_memory = true;

            // Set the metatable
            luaL_getmetatable(L, LuaTypeRegistry<CleanT>::metatable_name);
            lua_setmetatable(L, -2);

            return;
        }

        // Fall back to table conversion for unregistered types
        LuaConversionHelper<T>::to_lua_impl(L, obj);
    }
}

template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    bool
>
from_lua(lua_State* L, int idx, T& out) {
    return LuaConversionHelper<T>::from_lua_impl(L, idx, out);
}

// ============================================================================
// Class Binding Function
// ============================================================================

template<Bindable T>
void bind_class(lua_State* L, const char* name) {
    static_assert(core::validate_bindable_members<T>(),
        "bind_class<T>: T contains members with types that mirror_bridge cannot convert. "
        "Mark unconvertible members with [[=exclude{}]] or add a custom type converter.");

    constexpr std::size_t static_method_count = get_static_member_function_count<T>();

    // Store metatable name in type registry (for to_lua wrapper creation)
    LuaTypeRegistry<T>::metatable_name = typeid(T).name();

    // Create metatable for this class
    luaL_newmetatable(L, typeid(T).name());

    // Set __index metamethod
    lua_pushcfunction(L, lua_index<T>);
    lua_setfield(L, -2, "__index");

    // Set __newindex metamethod
    lua_pushcfunction(L, lua_newindex<T>);
    lua_setfield(L, -2, "__newindex");

    // Set __gc metamethod
    lua_pushcfunction(L, lua_gc<T>);
    lua_setfield(L, -2, "__gc");

    // Pop metatable
    lua_pop(L, 1);

    // Create a table for the class (holds constructor and static methods)
    lua_newtable(L);

    // Add constructor as __call on a metatable for the class table
    lua_newtable(L);  // metatable for class table
    lua_pushcfunction(L, lua_constructor<T>);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);  // set metatable on class table

    // Also add constructor directly as "new" method
    lua_pushcfunction(L, lua_constructor<T>);
    lua_setfield(L, -2, "new");

    // Add static methods to the class table
    if constexpr (static_method_count > 0) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                constexpr auto method_name = get_static_member_function_name<T, Is>();
                lua_pushcclosure(L, lua_static_method<T, Is>, 0);
                lua_setfield(L, -2, method_name);
            }(), ...);
        }(std::make_index_sequence<static_method_count>{});
    }

    // Set the class table in the module table
    lua_setfield(L, -2, name);
}

} // namespace lua
} // namespace mirror_bridge

// ============================================================================
// Module Definition Macro
// ============================================================================

#define MIRROR_BRIDGE_LUA_MODULE(module_name, ...) \
    extern "C" int luaopen_##module_name(lua_State* L) { \
        lua_newtable(L); \
        __VA_ARGS__ \
        return 1; \
    }
