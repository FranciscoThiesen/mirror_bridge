#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge JavaScript - JavaScript Bindings for C++ Code via C++26 Reflection
// ═══════════════════════════════════════════════════════════════════════════
// Generates Node.js bindings that expose C++ classes to JavaScript.

#include "../core/mirror_bridge_core.hpp"
#include <node_api.h>
#include <cstdio>
#include <cstring>
#include <optional>
#include <future>

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

// Call constructor with JS arguments
template<typename T, std::size_t CtorIndex, std::size_t... Is>
T* call_js_constructor_impl(napi_env env, napi_value* args, std::index_sequence<Is...>, bool& success) {
    std::tuple<std::remove_cvref_t<typename [:get_js_constructor_param_type<T, CtorIndex, Is>():]>...> cpp_args;

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

    if (argc == 0) {
        // Default constructor
        cpp_object = new T();
    } else {
        // Try parameterized constructors
        constexpr std::size_t ctor_count = get_js_constructor_count<T>();

        if constexpr (ctor_count > 0) {
            // Try each constructor
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ([&] {
                    if (cpp_object != nullptr) return; // Already found a match
                    bool matched = false;
                    T* result = try_js_constructor<T, Is>(env, args, argc, matched);
                    if (result != nullptr) {
                        cpp_object = result;
                    }
                }(), ...);
            }(std::make_index_sequence<ctor_count>{});
        }

        // If no constructor matched, fall back to default
        if (cpp_object == nullptr) {
            cpp_object = new T();
        }
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
