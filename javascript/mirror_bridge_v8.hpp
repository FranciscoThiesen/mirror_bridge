#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge V8 - Direct V8 Bindings for C++ Code via C++26 Reflection
// ═══════════════════════════════════════════════════════════════════════════
//
// This header provides DIRECT V8 API bindings, separate from Node.js N-API.
//
// USE THIS WHEN:
//   - Embedding V8 in a custom application (not Node.js)
//   - Building Chrome extensions or Chromium-based applications
//   - Creating custom JavaScript runtimes
//   - Using V8 in game engines or other embedded contexts
//
// DO NOT USE THIS FOR Node.js - use mirror_bridge_javascript.hpp instead.
//
// KEY DIFFERENCES FROM N-API (mirror_bridge_javascript.hpp):
// ┌──────────────────┬─────────────────────────────────────────────────────┐
// │ Aspect           │ V8 Direct                 │ N-API                   │
// ├──────────────────┼─────────────────────────────────────────────────────┤
// │ Header           │ <v8.h>                    │ <node_api.h>            │
// │ Handle type      │ v8::Local<T>              │ napi_value              │
// │ Scope management │ Manual HandleScope        │ Implicit                │
// │ GC integration   │ v8::Persistent<T>         │ napi_ref                │
// │ ABI stability    │ Changes between versions  │ Stable ABI              │
// │ Use case         │ Embedded V8               │ Node.js addons          │
// └──────────────────┴─────────────────────────────────────────────────────┘
//
// REQUIREMENTS:
//   - V8 development headers and libraries
//   - V8 must be initialized before using this library
//   - Caller is responsible for v8::Isolate and v8::Context management
//
// EXAMPLE USAGE:
//   // Initialize V8 (caller's responsibility)
//   v8::Isolate* isolate = v8::Isolate::GetCurrent();
//   v8::HandleScope handle_scope(isolate);
//   v8::Local<v8::Context> context = v8::Context::New(isolate);
//   v8::Context::Scope context_scope(context);
//
//   // Bind your class
//   v8::Local<v8::Object> exports = v8::Object::New(isolate);
//   mirror_bridge::v8::bind_class<MyClass>(isolate, exports, "MyClass");
//
// ═══════════════════════════════════════════════════════════════════════════

#include "../core/mirror_bridge_core.hpp"
#include <v8.h>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <memory>
#include <vector>

namespace mirror_bridge {
namespace v8_bindings {  // Named v8_bindings to avoid conflict with ::v8 namespace

// Import core concepts for convenience
using namespace core;

// ============================================================================
// V8 Wrapper for C++ Objects
// ============================================================================
//
// Each C++ object bound to V8 is wrapped in this structure.
// The wrapper manages:
//   - Pointer to the C++ object
//   - Ownership (whether we should delete the C++ object when GC'd)
//   - A weak persistent handle for GC callback
//
// ============================================================================

// Declared ahead of the wrapper, which tags itself with the name the class
// was bound under so an error message can say what actually arrived.
template<typename T>
struct V8TypeRegistry;

template<typename T>
struct V8Wrapper {
    // What bind_class called this class, so a rejected conversion can name
    // what actually arrived. Identity itself comes from the object's
    // FunctionTemplate, not from here - see resolve_v8_wrapper.
    const char* type_name = V8TypeRegistry<T>::bound_name;
    T* cpp_object;           // Pointer to the actual C++ object
    bool owns_memory;        // If true, delete cpp_object when V8 GCs this wrapper

    // Constructor
    V8Wrapper() : cpp_object(nullptr), owns_memory(false) {}
    V8Wrapper(T* obj, bool owns) : cpp_object(obj), owns_memory(owns) {}

    // Destructor - cleans up if we own the memory
    ~V8Wrapper() {
        if (owns_memory && cpp_object) {
            delete cpp_object;
            cpp_object = nullptr;
        }
    }
};

// ============================================================================
// Type Conversion: C++ → V8 (to_v8)
// ============================================================================
//
// These functions convert C++ values to V8 JavaScript values.
// Each overload handles a specific category of C++ types.
//
// ============================================================================

// ----------------------------------------------------------------------------
// Arithmetic types (int, float, double, etc.) → V8 Number
// ----------------------------------------------------------------------------
template<Arithmetic T>
::v8::Local<::v8::Value> to_v8(::v8::Isolate* isolate, const T& value) {
    if constexpr (std::is_floating_point_v<T>) {
        return ::v8::Number::New(isolate, static_cast<double>(value));
    } else if constexpr (std::is_same_v<T, bool>) {
        return ::v8::Boolean::New(isolate, value);
    } else if constexpr (sizeof(T) <= 4 && std::is_signed_v<T>) {
        return ::v8::Integer::New(isolate, static_cast<int32_t>(value));
    } else if constexpr (sizeof(T) <= 4) {
        return ::v8::Integer::NewFromUnsigned(isolate, static_cast<uint32_t>(value));
    } else {
        // For 64-bit integers, use Number (may lose precision for very large values)
        return ::v8::Number::New(isolate, static_cast<double>(value));
    }
}

// ----------------------------------------------------------------------------
// Boolean type → V8 Boolean
// ----------------------------------------------------------------------------
inline ::v8::Local<::v8::Value> to_v8(::v8::Isolate* isolate, bool value) {
    return ::v8::Boolean::New(isolate, value);
}

// ----------------------------------------------------------------------------
// Enum types → V8 Integer
// ----------------------------------------------------------------------------
template<EnumType T>
::v8::Local<::v8::Value> to_v8(::v8::Isolate* isolate, const T& value) {
    return ::v8::Integer::New(isolate, static_cast<int32_t>(value));
}

// ----------------------------------------------------------------------------
// String types (std::string, const char*, string_view) → V8 String
// ----------------------------------------------------------------------------
template<StringLike T>
::v8::Local<::v8::Value> to_v8(::v8::Isolate* isolate, const T& value) {
    using BaseType = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<BaseType, std::string> ||
                  std::is_same_v<BaseType, std::string_view>) {
        return ::v8::String::NewFromUtf8(
            isolate,
            value.data(),
            ::v8::NewStringType::kNormal,
            static_cast<int>(value.size())
        ).ToLocalChecked();
    } else {
        // const char*
        return ::v8::String::NewFromUtf8(
            isolate,
            value,
            ::v8::NewStringType::kNormal
        ).ToLocalChecked();
    }
}

// ----------------------------------------------------------------------------
// Containers (vector, set, etc.) → V8 Array
// ----------------------------------------------------------------------------
template<Container T>
::v8::Local<::v8::Value> to_v8(::v8::Isolate* isolate, const T& container) {
    ::v8::Local<::v8::Context> context = isolate->GetCurrentContext();
    ::v8::Local<::v8::Array> array = ::v8::Array::New(isolate, static_cast<int>(container.size()));

    uint32_t index = 0;
    for (const auto& item : container) {
        ::v8::Local<::v8::Value> v8_item = to_v8(isolate, item);
        array->Set(context, index++, v8_item).Check();
    }
    return array;
}

// ----------------------------------------------------------------------------
// Smart pointers (unique_ptr, shared_ptr) → V8 Value or null
// ----------------------------------------------------------------------------
template<SmartPointer T>
::v8::Local<::v8::Value> to_v8(::v8::Isolate* isolate, const T& ptr) {
    if (!ptr) {
        return ::v8::Null(isolate);
    }
    return to_v8(isolate, *ptr);
}

// Forward declaration for Bindable types
template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    ::v8::Local<::v8::Value>
>
to_v8(::v8::Isolate* isolate, const T& obj);

// ============================================================================
// Type Conversion: V8 → C++ (from_v8)
// ============================================================================
//
// These functions convert V8 JavaScript values to C++ values.
// Returns true on success, false on type mismatch or conversion failure.
//
// ============================================================================

// ----------------------------------------------------------------------------
// V8 Number/Integer → Arithmetic types
// ----------------------------------------------------------------------------
template<Arithmetic T>
bool from_v8(::v8::Isolate* isolate, ::v8::Local<::v8::Value> value, T& out) {
    ::v8::Local<::v8::Context> context = isolate->GetCurrentContext();

    if constexpr (std::is_floating_point_v<T>) {
        if (!value->IsNumber()) return false;
        ::v8::Maybe<double> maybe = value->NumberValue(context);
        if (maybe.IsNothing()) return false;
        out = static_cast<T>(maybe.FromJust());
    } else if constexpr (std::is_same_v<T, bool>) {
        out = value->BooleanValue(isolate);
    } else if constexpr (std::is_signed_v<T>) {
        if (!value->IsNumber() && !value->IsInt32()) return false;
        ::v8::Maybe<int64_t> maybe = value->IntegerValue(context);
        if (maybe.IsNothing()) return false;
        out = static_cast<T>(maybe.FromJust());
    } else {
        if (!value->IsNumber() && !value->IsUint32()) return false;
        ::v8::Maybe<uint32_t> maybe = value->Uint32Value(context);
        if (maybe.IsNothing()) return false;
        out = static_cast<T>(maybe.FromJust());
    }
    return true;
}

// ----------------------------------------------------------------------------
// V8 Boolean → bool
// ----------------------------------------------------------------------------
inline bool from_v8(::v8::Isolate* isolate, ::v8::Local<::v8::Value> value, bool& out) {
    out = value->BooleanValue(isolate);
    return true;
}

// ----------------------------------------------------------------------------
// V8 Integer → Enum types
// ----------------------------------------------------------------------------
template<EnumType T>
bool from_v8(::v8::Isolate* isolate, ::v8::Local<::v8::Value> value, T& out) {
    ::v8::Local<::v8::Context> context = isolate->GetCurrentContext();
    if (!value->IsInt32()) return false;
    ::v8::Maybe<int32_t> maybe = value->Int32Value(context);
    if (maybe.IsNothing()) return false;
    out = static_cast<T>(maybe.FromJust());
    return true;
}

// ----------------------------------------------------------------------------
// V8 String → String types
// ----------------------------------------------------------------------------
template<StringLike T>
bool from_v8(::v8::Isolate* isolate, ::v8::Local<::v8::Value> value, T& out) {
    if (!value->IsString()) return false;

    ::v8::Local<::v8::String> str = value.As<::v8::String>();

    using BaseType = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<BaseType, std::string>) {
        int length = str->Utf8Length(isolate);
        std::string temp(length, '\0');
        str->WriteUtf8(isolate, &temp[0], length);
        out = std::move(temp);
    } else {
        // For char* and string_view, use thread-local storage
        static thread_local std::string temp_storage;
        int length = str->Utf8Length(isolate);
        temp_storage.resize(length);
        str->WriteUtf8(isolate, &temp_storage[0], length);
        out = temp_storage.c_str();
    }
    return true;
}

// ----------------------------------------------------------------------------
// V8 Array → Containers
// ----------------------------------------------------------------------------
template<Container T>
bool from_v8(::v8::Isolate* isolate, ::v8::Local<::v8::Value> value, T& container) {
    if (!value->IsArray()) return false;

    ::v8::Local<::v8::Context> context = isolate->GetCurrentContext();
    ::v8::Local<::v8::Array> array = value.As<::v8::Array>();
    uint32_t length = array->Length();

    using ValueType = typename std::remove_cvref_t<T>::value_type;

    if constexpr (requires { container.clear(); }) {
        container.clear();
    }
    if constexpr (requires { container.reserve(length); }) {
        container.reserve(length);
    }

    for (uint32_t i = 0; i < length; ++i) {
        ::v8::MaybeLocal<::v8::Value> maybe_item = array->Get(context, i);
        if (maybe_item.IsEmpty()) return false;

        ValueType cpp_item;
        if (!from_v8(isolate, maybe_item.ToLocalChecked(), cpp_item)) return false;

        if constexpr (requires { container.push_back(cpp_item); }) {
            container.push_back(std::move(cpp_item));
        } else if constexpr (requires { container.insert(cpp_item); }) {
            container.insert(std::move(cpp_item));
        }
    }
    return true;
}

// ----------------------------------------------------------------------------
// V8 Value → Smart pointers
// ----------------------------------------------------------------------------
template<SmartPointer T>
bool from_v8(::v8::Isolate* isolate, ::v8::Local<::v8::Value> value, T& out) {
    using ElementType = typename std::remove_cvref_t<T>::element_type;

    if (value->IsNull() || value->IsUndefined()) {
        out.reset();
        return true;
    }

    ElementType cpp_value;
    if (!from_v8(isolate, value, cpp_value)) return false;

    if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::unique_ptr<ElementType>>) {
        out = std::make_unique<ElementType>(std::move(cpp_value));
    } else {
        out = std::make_shared<ElementType>(std::move(cpp_value));
    }
    return true;
}

// ============================================================================
// Internal Data Field Indices
// ============================================================================
//
// V8 objects have internal fields where we can store native pointers.
// We use index 0 to store the V8Wrapper pointer.
//
// ============================================================================

constexpr int kWrapperFieldIndex = 0;
constexpr int kInternalFieldCount = 1;

// ============================================================================
// Type Registry for V8 Class Templates
// ============================================================================
//
// Stores the V8 FunctionTemplate for each bound C++ type.
// Used for creating new instances and type checking.
//
// ============================================================================

template<typename T>
struct V8TypeRegistry {
    static inline ::v8::Global<::v8::FunctionTemplate> constructor_template;
    static inline ::v8::Isolate* cached_isolate = nullptr;
    static inline const char* bound_name = nullptr;
};

// ============================================================================
// Wrapper identity - what a JavaScript value is allowed to become
// ============================================================================
//
// Reading the internal field of whatever object arrives is two mistakes at
// once: the object may not be one of ours at all, and even when it is, it
// may hold a different C++ class whose bytes then get reinterpreted. Both
// questions have the same answer in V8: FunctionTemplate::HasInstance says
// whether an object was made from a given class's template, so the class is
// established before any internal field is touched. One gate,
// resolve_v8_wrapper, stands in front of every read.

// The part of V8Wrapper<X> that does not depend on X.
struct V8WrapperView {
    const char* type_name;
    void* cpp_object;
    bool owns_memory;
};

using V8UpcastThunk = void* (*)(void*);

// Every class bound in this module, so an object that is not the expected
// class can still be identified before its address is adjusted. Scanned only
// on the slow path, and a generated module binds a handful of classes.
struct V8BoundClass {
    ::v8::Global<::v8::FunctionTemplate>* constructor_template;
    const char* type_id;
    // Which isolate the template belongs to. A Global may only be read back
    // through the isolate it was created in, and bind_class overwrites the
    // one Global per class, so an embedder with two isolates would otherwise
    // have the scan below read a template through the wrong one.
    ::v8::Isolate* isolate;
};

inline std::vector<V8BoundClass>& v8_bound_classes() {
    static std::vector<V8BoundClass> classes;
    return classes;
}

// Recorded by the DERIVED class's bind_class, keyed "base|derived". Per
// shared library, which is as far as this backend's cross-module story
// reaches: to_v8 already falls back to a plain-object snapshot for a class
// this module did not bind.
inline std::map<std::string, V8UpcastThunk>& v8_upcast_table() {
    static std::map<std::string, V8UpcastThunk> table;
    return table;
}

inline V8UpcastThunk find_v8_upcast(const char* base_tid, const char* derived_tid) {
    auto& table = v8_upcast_table();
    auto it = table.find(std::string(base_tid) + "|" + derived_tid);
    return it == table.end() ? nullptr : it->second;
}

// A base the language will not let us reach - inaccessible, or ambiguous
// because it is inherited twice non-virtually - is skipped rather than
// recorded, and the conversion is then declined, which is the same answer
// C++ gives at that call site.
//
// The guard does not make such a hierarchy bindable, though: a class that
// inherits one base twice non-virtually also inherits its members twice, and
// reflection enumerates both, so bind_class fails earlier on the ambiguous
// member call. That is older than the upcast table and unchanged by it.
template<typename Derived, typename Base>
void register_v8_upcast() {
    if constexpr (requires (Derived* d) { static_cast<Base*>(d); }) {
        v8_upcast_table()[std::string(core::type_key<Base>) + "|" + core::type_key<Derived>] =
            +[](void* p) -> void* {
                return static_cast<void*>(static_cast<Base*>(static_cast<Derived*>(p)));
            };
    }
}

template<typename T>
void register_v8_base_upcasts() {
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (register_v8_upcast<T, core::base_t<T, Is>>(), ...);
    }(std::make_index_sequence<core::BaseClosure<T>::types.size()>{});
}

inline V8WrapperView* v8_wrapper_view(::v8::Local<::v8::Object> obj) {
    return static_cast<V8WrapperView*>(
        obj->GetAlignedPointerFromInternalField(kWrapperFieldIndex));
}

// Resolve a JavaScript value to the address of the C++ object it wraps, after
// checking that it really is a wrapper for Expected (or for a class derived
// from it). Returns false - never a bad pointer - for anything else.
template<typename Expected>
bool resolve_v8_wrapper(::v8::Isolate* isolate, ::v8::Local<::v8::Value> value, void*& raw) {
    if (value.IsEmpty() || !value->IsObject()) return false;
    ::v8::Local<::v8::Object> obj = value.As<::v8::Object>();
    if (obj->InternalFieldCount() < kInternalFieldCount) return false;

    auto& registry = V8TypeRegistry<Expected>::constructor_template;
    if (!registry.IsEmpty() && V8TypeRegistry<Expected>::cached_isolate == isolate) {
        ::v8::Local<::v8::FunctionTemplate> tpl = registry.Get(isolate);
        if (tpl->HasInstance(obj)) {
            V8WrapperView* view = v8_wrapper_view(obj);
            if (!view || !view->cpp_object) return false;
            raw = view->cpp_object;
            return true;
        }
    }

    // Some other class. HasInstance is what makes reading the internal field
    // safe, so the dynamic type has to be established the same way rather
    // than by trusting the field's contents.
    for (const V8BoundClass& bound : v8_bound_classes()) {
        if (bound.constructor_template->IsEmpty() || bound.isolate != isolate) continue;
        ::v8::Local<::v8::FunctionTemplate> tpl = bound.constructor_template->Get(isolate);
        if (!tpl->HasInstance(obj)) continue;

        V8UpcastThunk to_base = find_v8_upcast(core::type_key<Expected>, bound.type_id);
        if (!to_base) return false;
        V8WrapperView* view = v8_wrapper_view(obj);
        if (!view || !view->cpp_object) return false;
        // The held pointer addresses the whole derived object, which is not
        // where a second or virtual base subobject begins.
        raw = to_base(view->cpp_object);
        return raw != nullptr;
    }
    return false;
}

template<typename T>
inline const char* v8_expected_name() {
    const char* name = V8TypeRegistry<T>::bound_name;
    return name ? name : core::type_key<T>;
}

// The bound name of an object made from one of this module's templates, or
// nullptr. Through HasInstance like everything else: an internal field count
// is not a sign the object is ours. Node hands scripts plenty of objects with
// internal fields of their own - Buffer, MessagePort, vm.Script - and reading
// the field out of one of those yields whatever Node stored there, which is
// then followed as a const char*.
inline const char* v8_bound_name_of(::v8::Isolate* isolate,
                                    ::v8::Local<::v8::Value> value) {
    if (value.IsEmpty() || !value->IsObject()) return nullptr;
    ::v8::Local<::v8::Object> obj = value.As<::v8::Object>();
    if (obj->InternalFieldCount() < kInternalFieldCount) return nullptr;
    for (const V8BoundClass& bound : v8_bound_classes()) {
        if (bound.constructor_template->IsEmpty() || bound.isolate != isolate) continue;
        if (!bound.constructor_template->Get(isolate)->HasInstance(obj)) continue;
        V8WrapperView* view = v8_wrapper_view(obj);
        return (view && view->type_name) ? view->type_name : bound.type_id;
    }
    return nullptr;
}

inline void describe_v8_value(::v8::Isolate* isolate, ::v8::Local<::v8::Value> value,
                              char* out, std::size_t out_size) {
    if (const char* name = v8_bound_name_of(isolate, value)) {
        std::snprintf(out, out_size, "%s", name);
        return;
    }
    const char* kind = "value";
    if (value.IsEmpty())            kind = "nothing";
    else if (value->IsUndefined())  kind = "undefined";
    else if (value->IsNull())       kind = "null";
    else if (value->IsNumber())     kind = "number";
    else if (value->IsString())     kind = "string";
    else if (value->IsBoolean())    kind = "boolean";
    else if (value->IsFunction())   kind = "function";
    else if (value->IsArray())      kind = "array";
    else if (value->IsObject())     kind = "object";
    std::snprintf(out, out_size, "%s", kind);
}

template<typename Expected>
void throw_v8_wrong_type(::v8::Isolate* isolate, ::v8::Local<::v8::Value> value,
                         const char* context) {
    char actual[256];
    describe_v8_value(isolate, value, actual, sizeof actual);
    char message[640];
    std::snprintf(message, sizeof message, "%s: expected %s, got %s",
                  context, v8_expected_name<Expected>(), actual);
    isolate->ThrowException(::v8::Exception::TypeError(
        ::v8::String::NewFromUtf8(isolate, message, ::v8::NewStringType::kNormal)
            .ToLocalChecked()));
}

// ============================================================================
// Weak Callback for GC
// ============================================================================
//
// Called by V8's garbage collector when a wrapped object is no longer
// referenced. Cleans up the C++ wrapper and optionally the C++ object.
//
// ============================================================================

template<typename T>
void weak_callback(const ::v8::WeakCallbackInfo<V8Wrapper<T>>& data) {
    V8Wrapper<T>* wrapper = data.GetParameter();
    if (wrapper) {
        delete wrapper;
    }
}

// ============================================================================
// Property Accessor (Getter)
// ============================================================================

template<typename T, std::size_t Index>
void v8_getter(::v8::Local<::v8::Name> property,
               const ::v8::PropertyCallbackInfo<::v8::Value>& info) {
    ::v8::Isolate* isolate = info.GetIsolate();

    // Checked like everything else, although V8 is stricter here than
    // N-API is: these accessors are installed on the instance template with
    // SetNativeDataProperty, so V8 surfaces them as data descriptors and
    // always passes the holder it found the property on. The gate is what
    // makes that an invariant rather than an assumption about V8 internals.
    void* raw = nullptr;
    if (!resolve_v8_wrapper<T>(isolate, info.Holder(), raw)) {
        constexpr auto name_sv = std::meta::identifier_of(get_data_member<T, Index>());
        throw_v8_wrong_type<T>(isolate, info.Holder(), name_sv.data());
        return;
    }
    T* self = static_cast<T*>(raw);

    constexpr auto member = get_data_member<T, Index>();
    const auto& value = (*self).[:member:];
    info.GetReturnValue().Set(to_v8(isolate, value));
}

// ============================================================================
// Property Accessor (Setter)
// ============================================================================

template<typename T, std::size_t Index>
void v8_setter(::v8::Local<::v8::Name> property,
               ::v8::Local<::v8::Value> value,
               const ::v8::PropertyCallbackInfo<void>& info) {
    ::v8::Isolate* isolate = info.GetIsolate();

    constexpr auto member = get_data_member<T, Index>();
    constexpr auto member_name_sv = std::meta::identifier_of(member);
    using MemberType = typename [:std::meta::type_of(member):];

    void* raw = nullptr;
    if (!resolve_v8_wrapper<T>(isolate, info.Holder(), raw)) {
        throw_v8_wrong_type<T>(isolate, info.Holder(), member_name_sv.data());
        return;
    }
    T* self = static_cast<T*>(raw);

    MemberType cpp_value;
    if (!from_v8(isolate, value, cpp_value)) {
        isolate->ThrowException(::v8::Exception::TypeError(
            ::v8::String::NewFromUtf8Literal(isolate, "Type conversion failed")
        ));
        return;
    }

    (*self).[:member:] = std::move(cpp_value);
}

// ============================================================================
// Method Binding Implementation
// ============================================================================

template<typename T, std::size_t FuncIndex, std::size_t... Is>
::v8::Local<::v8::Value> call_method_impl(
    ::v8::Isolate* isolate,
    T* self,
    const ::v8::FunctionCallbackInfo<::v8::Value>& args,
    std::index_sequence<Is...>)
{
    constexpr auto member_func = get_member_function<T, FuncIndex>();
    constexpr auto return_type = get_method_return_type<T, FuncIndex>();
    using ReturnType = typename [:return_type:];

    // Build tuple of converted arguments
    std::tuple<std::remove_cvref_t<typename [:get_method_param_type<T, FuncIndex, Is>():]>...> cpp_args;

    bool success = true;
    ([&] {
        if (!success) return;
        if (!from_v8(isolate, args[Is], std::get<Is>(cpp_args))) {
            success = false;
        }
    }(), ...);

    if (!success) {
        isolate->ThrowException(::v8::Exception::TypeError(
            ::v8::String::NewFromUtf8Literal(isolate, "Argument type conversion failed")
        ));
        return ::v8::Undefined(isolate);
    }

    if constexpr (std::is_void_v<ReturnType>) {
        ((*self).[:member_func:])(std::move(std::get<Is>(cpp_args))...);
        return ::v8::Undefined(isolate);
    } else {
        ReturnType result = ((*self).[:member_func:])(std::move(std::get<Is>(cpp_args))...);
        return to_v8(isolate, result);
    }
}

template<typename T, std::size_t Index>
void v8_method(const ::v8::FunctionCallbackInfo<::v8::Value>& args) {
    ::v8::Isolate* isolate = args.GetIsolate();
    constexpr auto method_name_sv = std::meta::identifier_of(get_member_function<T, Index>());

    // `obj.method(...)` and `Class.prototype.method.call(x, ...)` reach the
    // same callback, so the receiver is whatever the caller chose.
    void* raw = nullptr;
    if (!resolve_v8_wrapper<T>(isolate, args.This(), raw)) {
        throw_v8_wrong_type<T>(isolate, args.This(), method_name_sv.data());
        return;
    }
    T* self = static_cast<T*>(raw);

    constexpr std::size_t param_count = get_method_param_count<T, Index>();

    if (static_cast<std::size_t>(args.Length()) != param_count) {
        isolate->ThrowException(::v8::Exception::Error(
            ::v8::String::NewFromUtf8Literal(isolate, "Incorrect number of arguments")
        ));
        return;
    }

    ::v8::Local<::v8::Value> result = call_method_impl<T, Index>(
        isolate, self, args, std::make_index_sequence<param_count>{}
    );
    args.GetReturnValue().Set(result);
}

// ============================================================================
// Static Method Binding Implementation
// ============================================================================

template<typename T, std::size_t FuncIndex, std::size_t... Is>
::v8::Local<::v8::Value> call_static_method_impl(
    ::v8::Isolate* isolate,
    const ::v8::FunctionCallbackInfo<::v8::Value>& args,
    std::index_sequence<Is...>)
{
    constexpr auto member_func = get_static_member_function<T, FuncIndex>();
    constexpr auto return_type = get_static_method_return_type<T, FuncIndex>();
    using ReturnType = typename [:return_type:];

    std::tuple<std::remove_cvref_t<typename [:get_static_method_param_type<T, FuncIndex, Is>():]>...> cpp_args;

    bool success = true;
    ([&] {
        if (!success) return;
        if (!from_v8(isolate, args[Is], std::get<Is>(cpp_args))) {
            success = false;
        }
    }(), ...);

    if (!success) {
        isolate->ThrowException(::v8::Exception::TypeError(
            ::v8::String::NewFromUtf8Literal(isolate, "Argument type conversion failed")
        ));
        return ::v8::Undefined(isolate);
    }

    if constexpr (std::is_void_v<ReturnType>) {
        [:member_func:](std::move(std::get<Is>(cpp_args))...);
        return ::v8::Undefined(isolate);
    } else {
        ReturnType result = [:member_func:](std::move(std::get<Is>(cpp_args))...);
        return to_v8(isolate, result);
    }
}

template<typename T, std::size_t Index>
void v8_static_method(const ::v8::FunctionCallbackInfo<::v8::Value>& args) {
    ::v8::Isolate* isolate = args.GetIsolate();

    constexpr std::size_t param_count = get_static_method_param_count<T, Index>();

    if (static_cast<std::size_t>(args.Length()) != param_count) {
        isolate->ThrowException(::v8::Exception::Error(
            ::v8::String::NewFromUtf8Literal(isolate, "Incorrect number of arguments")
        ));
        return;
    }

    ::v8::Local<::v8::Value> result = call_static_method_impl<T, Index>(
        isolate, args, std::make_index_sequence<param_count>{}
    );
    args.GetReturnValue().Set(result);
}

// ============================================================================
// Constructor Callback
// ============================================================================

template<typename T>
void v8_constructor(const ::v8::FunctionCallbackInfo<::v8::Value>& args) {
    ::v8::Isolate* isolate = args.GetIsolate();

    if (!args.IsConstructCall()) {
        isolate->ThrowException(::v8::Exception::Error(
            ::v8::String::NewFromUtf8Literal(isolate, "Must be called with 'new'")
        ));
        return;
    }

    // Create wrapper with new C++ object
    V8Wrapper<T>* wrapper = new V8Wrapper<T>(new T(), true);

    // Store wrapper in internal field
    ::v8::Local<::v8::Object> self = args.This();
    self->SetAlignedPointerInInternalField(kWrapperFieldIndex, wrapper);

    // Set up weak reference for GC cleanup
    // Note: In production, you'd want to use a weak persistent handle
    // For simplicity, we rely on the destructor being called

    args.GetReturnValue().Set(self);
}

// ============================================================================
// Nested Bindable Conversion Helper
// ============================================================================

template<typename T>
struct V8ConversionHelper {
    static ::v8::Local<::v8::Value> to_v8_impl(::v8::Isolate* isolate, const T& obj) {
        ::v8::Local<::v8::Context> context = isolate->GetCurrentContext();
        ::v8::Local<::v8::Object> v8_obj = ::v8::Object::New(isolate);

        constexpr std::size_t member_count = get_data_member_count<T>();

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                constexpr auto member = get_data_member<T, Is>();
                constexpr auto name_sv = std::meta::identifier_of(member);

                ::v8::Local<::v8::String> key = ::v8::String::NewFromUtf8(
                    isolate, name_sv.data(), ::v8::NewStringType::kNormal,
                    static_cast<int>(name_sv.size())
                ).ToLocalChecked();

                const auto& value = obj.[:member:];
                ::v8::Local<::v8::Value> v8_value = to_v8(isolate, value);
                v8_obj->Set(context, key, v8_value).Check();
            }(), ...);
        }(std::make_index_sequence<member_count>{});

        return v8_obj;
    }

    static bool from_v8_impl(::v8::Isolate* isolate, ::v8::Local<::v8::Value> v8_val, T& out) {
        if (!v8_val->IsObject()) return false;

        ::v8::Local<::v8::Context> context = isolate->GetCurrentContext();
        ::v8::Local<::v8::Object> v8_obj = v8_val.As<::v8::Object>();
        constexpr std::size_t member_count = get_data_member_count<T>();
        bool success = true;

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (!success) return;

                constexpr auto member = get_data_member<T, Is>();
                constexpr auto name_sv = std::meta::identifier_of(member);
                using MemberType = typename [:std::meta::type_of(member):];

                ::v8::Local<::v8::String> key = ::v8::String::NewFromUtf8(
                    isolate, name_sv.data(), ::v8::NewStringType::kNormal,
                    static_cast<int>(name_sv.size())
                ).ToLocalChecked();

                ::v8::MaybeLocal<::v8::Value> maybe_val = v8_obj->Get(context, key);
                if (maybe_val.IsEmpty()) {
                    success = false;
                    return;
                }

                MemberType cpp_value;
                if (!from_v8(isolate, maybe_val.ToLocalChecked(), cpp_value)) {
                    success = false;
                    return;
                }

                out.[:member:] = std::move(cpp_value);
            }(), ...);
        }(std::make_index_sequence<member_count>{});

        return success;
    }
};

// Bindable type conversion (implementation)
template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    ::v8::Local<::v8::Value>
>
to_v8(::v8::Isolate* isolate, const T& obj) {
    using CleanT = std::remove_cvref_t<T>;

    // Check if this type has been registered with bind_class
    if (!V8TypeRegistry<CleanT>::constructor_template.IsEmpty() &&
        V8TypeRegistry<CleanT>::cached_isolate == isolate) {
        ::v8::Local<::v8::FunctionTemplate> tpl =
            V8TypeRegistry<CleanT>::constructor_template.Get(isolate);
        ::v8::Local<::v8::Context> context = isolate->GetCurrentContext();

        // Create new instance
        ::v8::Local<::v8::Function> constructor = tpl->GetFunction(context).ToLocalChecked();
        ::v8::Local<::v8::Object> instance = constructor->NewInstance(context).ToLocalChecked();

        // Copy object data
        V8Wrapper<CleanT>* wrapper = static_cast<V8Wrapper<CleanT>*>(
            instance->GetAlignedPointerFromInternalField(kWrapperFieldIndex)
        );
        if (wrapper && wrapper->cpp_object) {
            *wrapper->cpp_object = obj;
        }

        return instance;
    }

    // Fall back to plain object conversion
    return V8ConversionHelper<T>::to_v8_impl(isolate, obj);
}

template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    bool
>
from_v8(::v8::Isolate* isolate, ::v8::Local<::v8::Value> value, T& out) {
    using CleanT = std::remove_cvref_t<T>;

    void* raw = nullptr;
    if (!resolve_v8_wrapper<CleanT>(isolate, value, raw)) {
        // Not a wrapper for CleanT: anything else would be a reinterpretation
        return false;
    }
    out = *static_cast<CleanT*>(raw);
    return true;
}

// ============================================================================
// Class Binding Function
// ============================================================================
//
// Binds a C++ class to V8, creating:
//   - A constructor function
//   - Property getters/setters for data members
//   - Methods for member functions
//   - Static methods for static member functions
//
// USAGE:
//   v8::Local<v8::Object> exports = v8::Object::New(isolate);
//   mirror_bridge::v8_bindings::bind_class<MyClass>(isolate, exports, "MyClass");
//
// ============================================================================

template<Bindable T>
::v8::Local<::v8::Object> bind_class(
    ::v8::Isolate* isolate,
    ::v8::Local<::v8::Object> exports,
    const char* name)
{
    ::v8::Local<::v8::Context> context = isolate->GetCurrentContext();

    constexpr std::size_t member_count = get_data_member_count<T>();
    constexpr std::size_t method_count = get_member_function_count<T>();
    constexpr std::size_t static_method_count = get_static_member_function_count<T>();

    // Create function template for constructor
    ::v8::Local<::v8::FunctionTemplate> tpl = ::v8::FunctionTemplate::New(
        isolate, v8_constructor<T>
    );

    tpl->SetClassName(::v8::String::NewFromUtf8(isolate, name).ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(kInternalFieldCount);

    // Store in type registry
    V8TypeRegistry<T>::constructor_template.Reset(isolate, tpl);
    V8TypeRegistry<T>::cached_isolate = isolate;
    V8TypeRegistry<T>::bound_name = name;

    // Listed so an object of some other bound class can still be identified
    // by HasInstance, and recorded so a derived object's address can be
    // shifted to the base subobject it is being passed as.
    bool already_listed = false;
    for (V8BoundClass& bound : v8_bound_classes()) {
        if (std::strcmp(bound.type_id, core::type_key<T>) == 0) {
            bound.isolate = isolate;   // the Global was just reset into this one
            already_listed = true;
            break;
        }
    }
    if (!already_listed) {
        v8_bound_classes().push_back(
            V8BoundClass{&V8TypeRegistry<T>::constructor_template, core::type_key<T>,
                         isolate});
    }
    register_v8_base_upcasts<T>();

    // Get prototype template
    ::v8::Local<::v8::ObjectTemplate> proto_tpl = tpl->PrototypeTemplate();

    // Add property accessors (getters/setters)
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr auto member = get_data_member<T, Is>();
            constexpr auto name_sv = std::meta::identifier_of(member);

            ::v8::Local<::v8::String> prop_name = ::v8::String::NewFromUtf8(
                isolate, name_sv.data(), ::v8::NewStringType::kNormal,
                static_cast<int>(name_sv.size())
            ).ToLocalChecked();

            // Cast to Local<Name> to match v8_getter/v8_setter signatures
            tpl->InstanceTemplate()->SetNativeDataProperty(
                prop_name.As<v8::Name>(),
                v8_getter<T, Is>,
                v8_setter<T, Is>
            );
        }(), ...);
    }(std::make_index_sequence<member_count>{});

    // Add instance methods
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr auto method_name_sv = std::meta::identifier_of(get_member_function<T, Is>());

            ::v8::Local<::v8::String> method_name = ::v8::String::NewFromUtf8(
                isolate, method_name_sv.data(), ::v8::NewStringType::kNormal,
                static_cast<int>(method_name_sv.size())
            ).ToLocalChecked();

            proto_tpl->Set(
                method_name,
                ::v8::FunctionTemplate::New(isolate, v8_method<T, Is>)
            );
        }(), ...);
    }(std::make_index_sequence<method_count>{});

    // Get constructor function
    ::v8::Local<::v8::Function> constructor = tpl->GetFunction(context).ToLocalChecked();

    // Add static methods to constructor
    if constexpr (static_method_count > 0) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                constexpr auto method_name = get_static_member_function_name<T, Is>();

                ::v8::Local<::v8::String> name_str = ::v8::String::NewFromUtf8(
                    isolate, method_name
                ).ToLocalChecked();

                ::v8::Local<::v8::Function> fn = ::v8::FunctionTemplate::New(
                    isolate, v8_static_method<T, Is>
                )->GetFunction(context).ToLocalChecked();

                constructor->Set(context, name_str, fn).Check();
            }(), ...);
        }(std::make_index_sequence<static_method_count>{});
    }

    // Add constructor to exports
    ::v8::Local<::v8::String> export_name = ::v8::String::NewFromUtf8(
        isolate, name
    ).ToLocalChecked();
    exports->Set(context, export_name, constructor).Check();

    return exports;
}

} // namespace v8_bindings
} // namespace mirror_bridge

// ============================================================================
// Module Definition Macro for V8
// ============================================================================
//
// Unlike Node.js N-API which has a standard module initialization pattern,
// V8 embedding requires you to manage the Isolate and Context yourself.
// This macro provides a helper for defining a module initialization function.
//
// USAGE:
//   MIRROR_BRIDGE_V8_MODULE(my_module,
//       mirror_bridge::v8_bindings::bind_class<MyClass>(isolate, exports, "MyClass");
//   )
//
//   // Then call: my_module_init(isolate, exports);
//
// ============================================================================

#define MIRROR_BRIDGE_V8_MODULE(module_name, ...) \
    inline void module_name##_init( \
        ::v8::Isolate* isolate, \
        ::v8::Local<::v8::Object> exports) \
    { \
        __VA_ARGS__ \
    }
