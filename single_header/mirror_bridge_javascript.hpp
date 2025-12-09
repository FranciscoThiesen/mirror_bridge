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
    constexpr auto ctors = std::meta::members_of(^^T, std::meta::access_context::unchecked());
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

// ============================================================================
// JAVASCRIPT - Node.js N-API Bindings
// ============================================================================


// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge JavaScript - JavaScript Bindings for C++ Code via C++26 Reflection
// ═══════════════════════════════════════════════════════════════════════════
// Generates Node.js bindings that expose C++ classes to JavaScript.

#include <node_api.h>
#include <cstdio>

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
    if constexpr (std::is_floating_point_v<T>) {
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

// Containers → JavaScript arrays
template<Container T>
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

// Smart pointers
template<SmartPointer T>
napi_value to_javascript(napi_env env, const T& ptr) {
    if (!ptr) {
        napi_value result;
        napi_get_null(env, &result);
        return result;
    }
    using ElementType = typename std::remove_cvref_t<T>::element_type;
    return to_javascript(env, *ptr);
}

// Forward declaration for Bindable types
template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    napi_value
>
to_javascript(napi_env env, const T& obj);

// ============================================================================
// Type Conversion: JavaScript → C++
// ============================================================================

// Arithmetic types
template<Arithmetic T>
bool from_javascript(napi_env env, napi_value value, T& out) {
    if constexpr (std::is_floating_point_v<T>) {
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

    ElementType cpp_value;
    if (!from_javascript(env, value, cpp_value)) return false;

    if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::unique_ptr<ElementType>>) {
        out = std::make_unique<ElementType>(std::move(cpp_value));
    } else {
        out = std::make_shared<ElementType>(std::move(cpp_value));
    }
    return true;
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

    auto& value = (*wrapper->cpp_object).[:member:];
    return to_javascript(env, value);
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

    (*wrapper->cpp_object).[:member:] = std::move(cpp_value);

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

    std::tuple<std::remove_cvref_t<typename [:get_method_param_type<T, FuncIndex, Is>():]>...> cpp_args;

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

    if constexpr (std::is_void_v<ReturnType>) {
        ((*wrapper->cpp_object).[:member_func:])(std::move(std::get<Is>(cpp_args))...);
        napi_value undefined;
        napi_get_undefined(env, &undefined);
        return undefined;
    } else {
        ReturnType result = ((*wrapper->cpp_object).[:member_func:])(std::move(std::get<Is>(cpp_args))...);
        return to_javascript(env, result);
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

    std::tuple<std::remove_cvref_t<typename [:get_static_method_param_type<T, FuncIndex, Is>():]>...> cpp_args;

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

    if constexpr (std::is_void_v<ReturnType>) {
        [:member_func:](std::move(std::get<Is>(cpp_args))...);
        napi_value undefined;
        napi_get_undefined(env, &undefined);
        return undefined;
    } else {
        ReturnType result = [:member_func:](std::move(std::get<Is>(cpp_args))...);
        return to_javascript(env, result);
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
// Constructor
// ============================================================================

template<typename T>
napi_value js_constructor(napi_env env, napi_callback_info info) {
    napi_value this_arg;
    size_t argc = 0;
    napi_get_cb_info(env, info, &argc, nullptr, &this_arg, nullptr);

    // For now, only support default constructor
    JsWrapper<T>* wrapper = new JsWrapper<T>();
    wrapper->cpp_object = new T();
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
