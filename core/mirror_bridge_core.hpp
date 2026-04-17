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
        return (is_value_bindable<std::remove_cvref_t<typename [:get_method_param_type<T, FuncIndex, Is>():]>>() && ...);
    }(std::make_index_sequence<param_count>{});
}

template<typename T, std::size_t FuncIndex>
consteval bool static_method_params_are_value_bindable() {
    constexpr std::size_t param_count = get_static_method_param_count<T, FuncIndex>();
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (is_value_bindable<std::remove_cvref_t<typename [:get_static_method_param_type<T, FuncIndex, Is>():]>>() && ...);
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
    using type = std::conditional_t<
        is_value_bindable<Clean>(),
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
        return (is_param_bindable<typename [:get_method_param_type<T, FuncIndex, Is>():]>() && ...);
    }(std::make_index_sequence<param_count>{});
}

template<typename T, std::size_t FuncIndex>
consteval bool static_method_params_all_bindable() {
    constexpr std::size_t param_count = get_static_method_param_count<T, FuncIndex>();
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (is_param_bindable<typename [:get_static_method_param_type<T, FuncIndex, Is>():]>() && ...);
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
