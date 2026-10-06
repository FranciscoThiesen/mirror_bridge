#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge Lua - Lua Bindings for C++ Code via C++26 Reflection
// ═══════════════════════════════════════════════════════════════════════════
// Generates Lua bindings that expose C++ classes to Lua.

#include "../core/mirror_bridge_core.hpp"
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

// Type-based registry for looking up metatable name by C++ type, plus the
// identity of that metatable in the state bind_class ran in, which is what
// makes the type check below a pointer comparison.
template<typename T>
struct LuaTypeRegistry {
    static inline const char* metatable_name = nullptr;
    static inline const void* metatable = nullptr;
};

// ============================================================================
// Wrapper identity - what a Lua value is allowed to become
// ============================================================================
//
// Every bound class gets the same wrapper layout (T*, bool). That uniformity
// is what lets one generated binding serve every class, and it is also what
// makes the wrappers indistinguishable from one another at the boundary:
// reading cpp_object out of whatever userdata arrives "succeeds" for an
// unrelated bound class just as readily as for the right one, and the caller
// then gets a number computed from the wrong object's bytes. So each
// conversion has to establish identity first.
//
// A userdata's metatable is the only thing about it that cannot be forged
// from Lua, so it is the identity, and nothing is read out of the userdata
// until the metatable has been recognised. Lua already keeps a registry
// entry per class, keyed by typeid name and created by bind_class, and two
// modules binding the same class share that one entry - so the exact-type
// check is cross-module with no extra bookkeeping. Two registry tables of
// our own carry the rest, both consulted off the hot path:
//
//   REGISTRY["mirror_bridge.upcasts"][base typeid][derived metatable] -> thunk
//   REGISTRY["mirror_bridge.names"][metatable] -> the name bind_class was given
//
// The upcast table converts a derived object's address to the address of a
// specific base subobject, an adjustment that reusing the pointer gets wrong
// for every base after the first. The name table is for error messages.
//
// Nothing is stored on the class metatables themselves: they are what Lua
// consults on every `obj.field`, and extra entries there measurably slow
// down every property access and method lookup.

// The part of LuaWrapper<X> that does not depend on X. Every wrapper starts
// this way, which is what makes a generic read possible - and why the read
// has to be preceded by a check.
struct LuaWrapperView {
    void* cpp_object;
    bool owns_memory;
};

using LuaUpcastThunk = void* (*)(void*);

inline constexpr const char* kLuaUpcastTableKey = "mirror_bridge.upcasts";
inline constexpr const char* kLuaNameTableKey = "mirror_bridge.names";

// Push one of our registry tables, creating it on first use. The Lua
// registry belongs to the global state rather than to a module, so a base
// and a derived class bound by two different .so files still meet here.
inline void push_lua_side_table(lua_State* L, const char* key) {
    if (lua_getfield(L, LUA_REGISTRYINDEX, key) == LUA_TTABLE) return;
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, key);
}

// Record how to turn a Derived* into a Base*, under Derived's metatable.
// Registered by the DERIVED class's bind_class, so the two classes may be
// bound in different modules and loaded in either order.
//
// A base the language will not let us reach - inaccessible, or ambiguous
// because it is inherited twice non-virtually - is skipped rather than
// registered, so the guard keeps bind_class compiling for hierarchies that
// have one. Lua then declines the conversion, which is the same answer C++
// gives at that call site.
template<typename Derived, typename Base>
void register_lua_upcast(lua_State* L) {
    if constexpr (requires (Derived* d) { static_cast<Base*>(d); }) {
        LuaUpcastThunk thunk = +[](void* p) -> void* {
            return static_cast<void*>(static_cast<Base*>(static_cast<Derived*>(p)));
        };
        push_lua_side_table(L, kLuaUpcastTableKey);
        lua_getfield(L, -1, typeid(Base).name());
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setfield(L, -3, typeid(Base).name());
        }
        luaL_getmetatable(L, typeid(Derived).name());
        // Round-tripping a function pointer through void* is how the C API
        // carries callbacks; lightuserdata has no function-pointer form.
        lua_pushlightuserdata(L, reinterpret_cast<void*>(thunk));
        lua_rawset(L, -3);
        lua_pop(L, 2);
    }
}

template<typename T>
void register_lua_base_upcasts(lua_State* L) {
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (register_lua_upcast<T, core::base_t<T, Is>>(L), ...);
    }(std::make_index_sequence<core::BaseClosure<T>::types.size()>{});
}

// How to reach Expected from the class whose metatable is on top of the
// stack, or nullptr when there is no such conversion. Leaves the stack as
// it found it, metatable included.
inline LuaUpcastThunk find_lua_upcast(lua_State* L, const char* base_tid) {
    push_lua_side_table(L, kLuaUpcastTableKey);
    lua_getfield(L, -1, base_tid);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        return nullptr;
    }
    lua_pushvalue(L, -3);                // the metatable, as the key
    lua_rawget(L, -2);
    LuaUpcastThunk thunk = nullptr;
    if (lua_islightuserdata(L, -1)) {
        thunk = reinterpret_cast<LuaUpcastThunk>(lua_touserdata(L, -1));
    }
    lua_pop(L, 3);
    return thunk;
}

// Everything except the common case: the exact class reached in a state this
// module did not bind it in, or a derived class whose address has to be
// shifted to the base subobject.
//
// Unlike the Python backend this decision is not memoized. The lookups below
// are plain table reads that allocate nothing, so there is no 5x gap to
// close - and a memo keyed on a metatable's address would be wrong across
// lua_close/lua_newstate, where the allocator can hand the same address to
// an unrelated class.
template<typename Expected>
bool resolve_lua_wrapper_slow(lua_State* L, int idx, void*& raw) {
    if (!lua_isuserdata(L, idx)) return false;
    idx = lua_absindex(L, idx);          // the lookups below push

    // Metatables are keyed by typeid name in the registry, so the module
    // that bound Expected registered the very table this object carries.
    if (void* ud = luaL_testudata(L, idx, typeid(Expected).name())) {
        void* held = static_cast<LuaWrapperView*>(ud)->cpp_object;
        if (!held) return false;
        raw = held;
        return true;
    }

    if (!lua_getmetatable(L, idx)) return false;   // not one of our wrappers
    LuaUpcastThunk to_base = find_lua_upcast(L, typeid(Expected).name());
    lua_pop(L, 1);
    if (!to_base) return false;

    void* held = static_cast<LuaWrapperView*>(lua_touserdata(L, idx))->cpp_object;
    if (!held) return false;

    // The held pointer addresses the whole derived object, which is not where
    // a second or virtual base subobject begins.
    raw = to_base(held);
    return raw != nullptr;
}

// Resolve a Lua value to the address of the C++ object it wraps, after
// checking that it really is a wrapper for Expected (or for a class derived
// from it). Returns false - never a bad pointer - for anything else.
//
// Every path that reinterprets a Lua value as a bound class goes through
// here, so the exact-type case is kept to a comparison of two metatable
// addresses rather than luaL_testudata, which looks the expected metatable
// up in the registry by name and so interns and hashes the mangled type name
// on every call. Everything else is one call away in the slow path, which
// does use luaL_testudata.
//
// The cached address is the one bind_class saw. Two live Lua states cannot
// have two live tables at the same address, so a stale cache can only cost a
// trip through the slow path, never a wrong answer.
template<typename Expected>
inline bool resolve_lua_wrapper(lua_State* L, int idx, void*& raw) {
    const void* want = LuaTypeRegistry<Expected>::metatable;
    if (want && lua_getmetatable(L, idx)) {
        bool same = lua_topointer(L, -1) == want;
        lua_pop(L, 1);
        if (same) {
            void* held = static_cast<LuaWrapperView*>(lua_touserdata(L, idx))->cpp_object;
            if (!held) return false;
            raw = held;
            return true;
        }
    }
    return resolve_lua_wrapper_slow<Expected>(L, idx, raw);
}

// The name bind_class gave the class whose metatable is on top of the stack,
// or nullptr. Read out of the registry rather than a C++ static so it is
// right even for a class another module bound.
inline const char* lua_name_for_metatable(lua_State* L) {
    push_lua_side_table(L, kLuaNameTableKey);
    lua_pushvalue(L, -2);
    lua_rawget(L, -2);
    const char* name = lua_tostring(L, -1);
    lua_pop(L, 2);
    return name;        // interned in the registry table, so it outlives the pop
}

// Copy the bound name of a C++ type into `out`, falling back to the mangled
// typeid name, which is still enough to tell two classes apart.
inline void copy_lua_registered_name(lua_State* L, const char* type_id,
                                     char* out, std::size_t out_size) {
    const char* name = nullptr;
    if (luaL_getmetatable(L, type_id) == LUA_TTABLE) {
        name = lua_name_for_metatable(L);
    }
    lua_pop(L, 1);
    std::snprintf(out, out_size, "%s", name ? name : type_id);
}

// What arrived, in the words a Lua author would use: the bound class name for
// one of our wrappers, otherwise the Lua type name.
inline void describe_lua_value(lua_State* L, int idx, char* out, std::size_t out_size) {
    const char* name = nullptr;
    if (lua_getmetatable(L, idx)) {
        name = lua_name_for_metatable(L);
        lua_pop(L, 1);
    }
    std::snprintf(out, out_size, "%s", name ? name : luaL_typename(L, idx));
}

// Whether a C++ type has been bound in this state, which decides whether an
// error message can name it in the words a Lua author would recognise.
inline bool is_lua_bound_type(lua_State* L, const char* type_id) {
    bool bound = luaL_getmetatable(L, type_id) == LUA_TTABLE;
    lua_pop(L, 1);
    return bound;
}

// luaL_error long-jumps out of here, so nothing with a destructor may be
// live: the two names are copied into plain buffers first.
inline int lua_wrong_type_error(lua_State* L, int idx, const char* context,
                                const char* expected_type_id) {
    char expected[256];
    char actual[256];
    copy_lua_registered_name(L, expected_type_id, expected, sizeof expected);
    describe_lua_value(L, idx, actual, sizeof actual);
    return luaL_error(L, "%s: expected %s, got %s", context, expected, actual);
}

// An argument that would not convert. Naming the expected class is only
// possible when it is one of ours; a plain scalar parameter keeps the
// shorter message rather than printing a mangled typeid.
inline int lua_bad_argument_error(lua_State* L, const char* context, int position,
                                  const char* expected_type_id, int idx) {
    char actual[256];
    describe_lua_value(L, idx, actual, sizeof actual);
    if (is_lua_bound_type(L, expected_type_id)) {
        char expected[256];
        copy_lua_registered_name(L, expected_type_id, expected, sizeof expected);
        return luaL_error(L, "%s: argument %d expected %s, got %s",
                          context, position, expected, actual);
    }
    return luaL_error(L, "%s: argument %d could not be converted from %s",
                      context, position, actual);
}

// A value that cannot be stored in a data member.
inline int lua_bad_field_error(lua_State* L, const char* field,
                               const char* expected_type_id, int idx) {
    char actual[256];
    describe_lua_value(L, idx, actual, sizeof actual);
    if (is_lua_bound_type(L, expected_type_id)) {
        char expected[256];
        copy_lua_registered_name(L, expected_type_id, expected, sizeof expected);
        return luaL_error(L, "%s: expected %s, got %s", field, expected, actual);
    }
    return luaL_error(L, "%s: cannot be assigned from %s", field, actual);
}

// Convert Lua wrapped objects or tables to C++ types
// Handles const reference parameters like dot(const Vec3& other)
// Also handles Lua tables for nested struct assignment
template<typename T>
    requires (std::is_class_v<std::remove_cvref_t<T>> &&
              !Arithmetic<T> && !StringLike<T> && !SmartPointer<T> && !Container<T>)
bool from_lua(lua_State* L, int idx, T& out) {
    using CleanT = std::remove_cvref_t<T>;

    // A wrapped C++ object, once we know it really is a CleanT
    void* raw = nullptr;
    if (resolve_lua_wrapper<CleanT>(L, idx, raw)) {
        out = *static_cast<CleanT*>(raw);
        return true;
    }

    // A userdata that failed the check is the wrong class, not a table of
    // fields to read; falling through would reinterpret its bytes.
    if (lua_isuserdata(L, idx)) return false;

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
    // L[1] = userdata (wrapper), L[2] = key (field name).
    // __index is installed on T's metatable, but a metamethod can be pulled
    // off a metatable and called on anything, so self is not ours to assume.
    // The check is deferred to the branch that actually dereferences self:
    // looking a method up never touches the C++ object, and that lookup is
    // on the path of every single `obj:method(...)` call.
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
                void* raw = nullptr;
                if (!resolve_lua_wrapper<T>(L, 1, raw)) {
                    lua_wrong_type_error(L, 1, member_name, typeid(T).name());
                    return;
                }
                const auto& value = (*static_cast<T*>(raw)).[:member:];
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
    // L[1] = userdata (wrapper), L[2] = key (field name), L[3] = value.
    // Same reachability as __index above, and the same deferral: the check
    // belongs with the write, which is the only thing here that touches the
    // C++ object.
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

                void* raw = nullptr;
                if (!resolve_lua_wrapper<T>(L, 1, raw)) {
                    lua_wrong_type_error(L, 1, member_name, typeid(T).name());
                    return;
                }

                MemberType cpp_value;
                if (!from_lua(L, 3, cpp_value)) {
                    lua_bad_field_error(L, member_name,
                                        typeid(std::remove_cvref_t<MemberType>).name(), 3);
                    return;
                }

                (*static_cast<T*>(raw)).[:member:] = std::move(cpp_value);
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
int call_method_impl(lua_State* L, T* self, std::index_sequence<Is...>) {
    constexpr auto member_func = get_member_function<T, FuncIndex>();
    constexpr auto return_type = get_method_return_type<T, FuncIndex>();
    using ReturnType = typename [:return_type:];

    std::tuple<std::remove_cvref_t<method_param_t<T, FuncIndex, Is>>...> cpp_args;

    // Remember which argument refused and what it was supposed to be, so the
    // error can say so: "expected Curve, got Label" is the whole point of the
    // identity check, and "conversion failed" would throw that away.
    int bad_arg = -1;
    const char* bad_type_id = nullptr;
    ([&] {
        if (bad_arg >= 0) return;
        // Lua stack: [1]=self, [2]=arg1, [3]=arg2, etc.
        if (!from_lua(L, 2 + Is, std::get<Is>(cpp_args))) {
            bad_arg = static_cast<int>(Is);
            bad_type_id = typeid(std::remove_cvref_t<method_param_t<T, FuncIndex, Is>>).name();
        }
    }(), ...);

    if (bad_arg >= 0) {
        constexpr auto method_name_sv = std::meta::identifier_of(get_member_function<T, FuncIndex>());
        return lua_bad_argument_error(L, method_name_sv.data(), bad_arg + 1,
                                      bad_type_id, 2 + bad_arg);
    }

    try {
        if constexpr (std::is_void_v<ReturnType>) {
            ((*self).[:member_func:])(std::move(std::get<Is>(cpp_args))...);
            return 0;
        } else {
            // Handle std::expected return types with idiomatic Lua multi-return
            using CleanReturn = std::remove_cvref_t<ReturnType>;
            if constexpr (is_lua_std_expected<CleanReturn>::value) {
                auto result = ((*self).[:member_func:])(std::move(std::get<Is>(cpp_args))...);
                return to_lua_expected(L, result);
            } else {
                ReturnType result = ((*self).[:member_func:])(std::move(std::get<Is>(cpp_args))...);
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
    constexpr auto method_name_sv = std::meta::identifier_of(get_member_function<T, Index>());

    // `obj:method(...)` and `Class.method(obj, ...)` are the same call, so
    // self is whatever the caller put first. Before the check, passing an
    // unrelated bound class here read that object's bytes as a T.
    void* raw = nullptr;
    if (!resolve_lua_wrapper<T>(L, 1, raw)) {
        return lua_wrong_type_error(L, 1, method_name_sv.data(), typeid(T).name());
    }
    T* self = static_cast<T*>(raw);

    constexpr std::size_t param_count = get_method_param_count<T, Index>();

    // Check argument count (excluding self)
    int nargs = lua_gettop(L) - 1;
    if (nargs != static_cast<int>(param_count)) {
        return luaL_error(L, "Incorrect number of arguments");
    }

    return call_method_impl<T, Index>(L, self, std::make_index_sequence<param_count>{});
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

    int bad_arg = -1;
    const char* bad_type_id = nullptr;
    ([&] {
        if (bad_arg >= 0) return;
        // Static methods: args start at index 1 (no self)
        if (!from_lua(L, 1 + Is, std::get<Is>(cpp_args))) {
            bad_arg = static_cast<int>(Is);
            bad_type_id = typeid(std::remove_cvref_t<static_method_param_t<T, FuncIndex, Is>>).name();
        }
    }(), ...);

    if (bad_arg >= 0) {
        constexpr auto method_name_sv =
            std::meta::identifier_of(get_static_member_function<T, FuncIndex>());
        return lua_bad_argument_error(L, method_name_sv.data(), bad_arg + 1,
                                      bad_type_id, 1 + bad_arg);
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
    // Strictly exact, unlike the other metamethods: a derived object reaching
    // here would be deleted through a base subobject address. `__gc` is also
    // callable by hand off the metatable, so a wrong userdata must leave
    // without freeing anything rather than raise during collection.
    void* ud = luaL_testudata(L, 1, typeid(T).name());
    if (!ud) return 0;
    LuaWrapper<T>* wrapper = static_cast<LuaWrapper<T>*>(ud);
    if (wrapper->owns_memory && wrapper->cpp_object) {
        delete wrapper->cpp_object;
        wrapper->cpp_object = nullptr;
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

    // Create metatable for this class. The registry keys it by typeid name,
    // so a second module binding the same class finds this very table and
    // the identity check agrees across .so boundaries.
    luaL_newmetatable(L, typeid(T).name());

    // The address of that metatable is what the exact-type check compares
    // against, and the name is what an error message calls the class. The
    // name goes in a side table rather than on the metatable itself, which
    // Lua reads on every property access.
    LuaTypeRegistry<T>::metatable = lua_topointer(L, -1);
    push_lua_side_table(L, kLuaNameTableKey);
    lua_pushvalue(L, -2);
    lua_pushstring(L, name);
    lua_rawset(L, -3);
    lua_pop(L, 1);

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

    // Record how to reach each base subobject from a T. Done by the derived
    // class, so base and derived can be bound in different modules and
    // loaded in either order.
    register_lua_base_upcasts<T>(L);

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
    extern "C" MIRROR_BRIDGE_EXPORT int luaopen_##module_name(lua_State* L) { \
        lua_newtable(L); \
        __VA_ARGS__ \
        return 1; \
    }
