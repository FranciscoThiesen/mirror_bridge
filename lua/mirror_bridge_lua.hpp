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
#include <array>
#include <cstddef>
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

// class_mt is the address of T's metatable in the lua_State the wrapper was
// made in, and it is what every boundary check compares. It leads the layout
// so the check is a load at offset zero. See "Wrapper identity" below for why
// the identity lives in the userdata's own bytes rather than in its metatable.
template<typename T>
struct LuaWrapper {
    const void* class_mt;
    T* cpp_object;
    bool owns_memory;
};

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

// The two overloads that route to a CustomLuaConverter specialization are
// defined after the composite converters, further down, on purpose: see the
// note at the end of the declaration block.

// ============================================================================
// Converter Declarations
// ============================================================================
//
// Every to_lua/from_lua overload this file defines is declared here, before
// any of them is defined, and a new overload belongs on this list.
//
// The composite converters -- containers, pair, tuple, optional, variant,
// smart pointers -- convert each element with an unqualified call whose
// argument type depends on the template. Such a call sees only the overloads
// declared before the composite's own definition, plus whatever
// argument-dependent lookup finds when it is instantiated, and ADL cannot
// help: the element is a user's class in the global namespace or the user's
// own, never in mirror_bridge::lua. So every overload defined below its
// caller was invisible to it. std::vector<Point> failed to compile the whole
// module, because the vector converter could not see the class converter
// defined far below it, and so did std::array<Point, N>,
// std::vector<std::optional<int>>, std::vector<std::shared_ptr<T>> and
// std::vector<std::pair<K, V>>. Declared up front, the set every element
// call sees is the set a top-level call sees.

template<Arithmetic T>
void to_lua(lua_State* L, const T& value);
template<EnumType T>
void to_lua(lua_State* L, const T& value);
template<StringLike T>
void to_lua(lua_State* L, const T& value);
template<Container T>
void to_lua(lua_State* L, const T& container);
template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>
>
to_lua(lua_State* L, const T& obj);
template<SmartPointer T>
void to_lua(lua_State* L, const T& ptr);
template<typename K, typename V, typename... Args>
void to_lua(lua_State* L, const std::map<K, V, Args...>& map);
template<typename K, typename V, typename... Args>
void to_lua(lua_State* L, const std::unordered_map<K, V, Args...>& map);
template<typename V, typename... Args>
void to_lua(lua_State* L, const std::set<V, Args...>& set);
template<typename V, typename... Args>
void to_lua(lua_State* L, const std::unordered_set<V, Args...>& set);
template<typename... Ts>
void to_lua(lua_State* L, const std::tuple<Ts...>& t);
template<typename T1, typename T2>
void to_lua(lua_State* L, const std::pair<T1, T2>& p);
template<typename... Ts>
void to_lua(lua_State* L, const std::variant<Ts...>& v);
template<typename T>
void to_lua(lua_State* L, const std::optional<T>& opt);

template<Arithmetic T>
bool from_lua(lua_State* L, int idx, T& out);
template<EnumType T>
bool from_lua(lua_State* L, int idx, T& out);
template<StringLike T>
bool from_lua(lua_State* L, int idx, T& out);
template<typename E, std::size_t N>
    requires (!HasCustomLuaConverter<std::array<E, N>>)
bool from_lua(lua_State* L, int idx, std::array<E, N>& arr);
template<Container T>
bool from_lua(lua_State* L, int idx, T& container);
// The class converter a top-level parameter or member uses is deliberately
// not declared here. It reads a table leniently -- a missing field keeps its
// default -- so it accepts every table, and a composite that asks "does this
// convert?" needs a strict answer: std::variant<Point, Color> keeps the first
// alternative that converts, and std::optional<Point> must refuse {}. A class
// element of a composite is read by the converter below instead, from a
// wrapper or from a table that has every field.
template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    bool
>
from_lua(lua_State* L, int idx, T& out);
template<SmartPointer T>
bool from_lua(lua_State* L, int idx, T& out);
template<typename K, typename V, typename... Args>
bool from_lua(lua_State* L, int idx, std::map<K, V, Args...>& map);
template<typename K, typename V, typename... Args>
bool from_lua(lua_State* L, int idx, std::unordered_map<K, V, Args...>& map);
template<typename V, typename... Args>
bool from_lua(lua_State* L, int idx, std::set<V, Args...>& set);
template<typename V, typename... Args>
bool from_lua(lua_State* L, int idx, std::unordered_set<V, Args...>& set);
template<typename... Ts>
bool from_lua(lua_State* L, int idx, std::tuple<Ts...>& t);
template<typename T1, typename T2>
bool from_lua(lua_State* L, int idx, std::pair<T1, T2>& p);
template<typename... Ts>
bool from_lua(lua_State* L, int idx, std::variant<Ts...>& v);
template<typename T>
bool from_lua(lua_State* L, int idx, std::optional<T>& out);
template<typename T, typename E>
bool from_lua(lua_State* L, int idx, std::expected<T, E>& out);
// Not declared here either: the CustomLuaConverter overloads, which are
// defined after the composites (see the comment there).

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
    using ValueType = typename std::remove_cvref_t<T>::value_type;
    lua_createtable(L, container.size(), 0);

    int index = 1;  // Lua arrays start at 1
    for (const auto& item : container) {
        // Iterating libc++'s std::vector<bool> yields a bit-reference proxy,
        // not a bool. The generic class converter would take the proxy for a
        // class and emit an empty table for every element, so convert the
        // value it stands for instead.
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(item)>, ValueType>) {
            to_lua(L, item);
        } else {
            to_lua(L, static_cast<ValueType>(item));
        }
        lua_rawseti(L, -2, index++);
    }
}

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
    using BaseType = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<BaseType, std::string>) {
        // lua_tolstring turns a number into a string in place, and a failed
        // conversion must leave the caller's value as it found it: the
        // constructor dispatcher tries overloads one after another on the
        // same slots, so Foo(5, 6) reached Foo(int, int) holding "5" after a
        // Foo(std::string, std::vector<int>) attempt, and was refused. It also
        // corrupts a map key mid-traversal, which lua_next documents. A
        // std::string owns its bytes, so converting a copy costs nothing.
        lua_pushvalue(L, idx);
        const char* str = lua_tolstring(L, -1, &len);
        out = std::string(str, len);
        lua_pop(L, 1);
    } else {
        // A string_view or const char* borrows the bytes, so they have to stay
        // on the stack for the call: the slot itself is converted.
        const char* str = lua_tolstring(L, idx, &len);
        if constexpr (std::is_same_v<BaseType, std::string_view>) {
            out = std::string_view(str, len);
        } else {
            out = str;
        }
    }
    return true;
}

// std::array from a Lua table of exactly N elements, filled in place.
//
// The generic container converter below can only append, and a std::array
// has no push_back or insert, so it used to convert every element, drop it,
// and report success. A std::array member was then assigned the setter's
// uninitialised local -- body.pos = {1, 2, 3} read back as 9.3e-310 -- a
// std::array parameter arrived as zeros, and any length was accepted. The
// Python backend requires the exact length (since 4fd587d).
//
// A std::array type with a CustomLuaConverter specialization is left to the
// extension point. Partial ordering would otherwise pick this overload over
// it, silently, where before there was an ambiguity error to say so.
template<typename E, std::size_t N>
    requires (!HasCustomLuaConverter<std::array<E, N>>)
bool from_lua(lua_State* L, int idx, std::array<E, N>& arr) {
    if (!lua_istable(L, idx)) return false;
    if (lua_rawlen(L, idx) != N) return false;

    for (std::size_t i = 0; i < N; ++i) {
        lua_rawgeti(L, idx, static_cast<lua_Integer>(i + 1));
        const bool converted = from_lua(L, -1, arr[i]);
        lua_pop(L, 1);
        if (!converted) return false;
    }
    return true;
}

// Containers from Lua tables
template<Container T>
bool from_lua(lua_State* L, int idx, T& container) {
    using ValueType = typename std::remove_cvref_t<T>::value_type;

    // Two shapes that look like containers and cannot be filled from a table.
    // A type can have begin, end, size and value_type and still offer no way
    // to put an element in -- std::span, a fixed-size Eigen vector, a custom
    // range -- and converting into one used to convert each element, drop it
    // and report success, so a std::span<const double> parameter computed
    // over nothing. And an element type with no default constructor cannot
    // be converted into, because each element is built in a local first;
    // reaching that local was a compile error that failed the whole module.
    // Both now decline, which turns the call into an error naming the
    // argument, as the Python backend does.
    constexpr bool can_append = requires(T& c, ValueType v) { c.push_back(std::move(v)); };
    constexpr bool can_insert = requires(T& c, ValueType v) { c.insert(std::move(v)); };
    if constexpr ((!can_append && !can_insert) || !std::is_default_constructible_v<ValueType>) {
        return false;
    } else {
    if (!lua_istable(L, idx)) return false;

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

        if constexpr (can_append) {
            container.push_back(std::move(cpp_item));
        } else {
            container.insert(std::move(cpp_item));
        }

        lua_pop(L, 1);
    }

    return true;
    }  // close the fillable-container branch
}

// Smart pointers from Lua
template<SmartPointer T>
bool from_lua(lua_State* L, int idx, T& out) {
    using ElementType = typename std::remove_cvref_t<T>::element_type;

    if (lua_isnil(L, idx)) {
        out.reset();
        return true;
    }

    // An abstract, non-default-constructible or non-copy-assignable pointee
    // cannot be materialised from a Lua value, so nil (handled above) is the
    // only value that converts. This branch used to report success without
    // writing `out`, which handed C++ a null pointer for any argument at
    // all: scene:area_of(circle) dereferenced it, and scene.shape = circle
    // silently cleared the member. The Python backend refuses here
    // (091bb2d); a bound derived object becoming a shared_ptr<Base> needs
    // shared ownership with the Lua object, which is issue #54.
    if constexpr (std::is_abstract_v<ElementType> ||
                  !std::is_default_constructible_v<ElementType> ||
                  !std::is_copy_assignable_v<ElementType>) {
        return false;
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

    // lua_pushnil below shifts every relative index, and a container element
    // arrives as -1: lua_next would then read the nil as the table. That
    // crashed a std::vector<std::map<...>> on its first element, and a
    // nested struct holding a map assigned from a table.
    idx = lua_absindex(L, idx);
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

    idx = lua_absindex(L, idx);   // see the std::map converter above
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

// The overloads behind a CustomLuaConverter specialization. They come after
// every composite converter, so a composite never sees them: their
// constraint neither subsumes nor is subsumed by Arithmetic, EnumType,
// StringLike or Container, so a composite element of a custom-converted
// enum or string would be an ambiguous call, where without them it compiles
// and the element converts the built-in way. Top-level calls, which are all
// defined below, do see them. (A plain to_lua/from_lua overload declared
// before this header, as tests/lua/custom_converter does, is visible
// everywhere and beats every template here.)
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
// The identity is a value written into the wrapper when it is created: the
// address of its class's metatable. Not the userdata's *attached* metatable,
// which is the obvious choice and is wrong, because Lua hands scripts two
// ways to put any metatable on any value:
//
//   setmetatable({}, getmetatable(curve))        -- plain Lua, no debug
//   debug.setmetatable(io.stdout, getmetatable(curve))
//
// The first makes a table that answers the metatable test and then has no
// userdata payload at all; the second makes any full userdata in the process
// answer it, including the standard library's file handles, whose first word
// is a FILE*. Trusting the attached metatable therefore turns a bad script
// into a read, a write and a free at an address the script chose.
//
// Lua has no API in the other direction: nothing in the language or the debug
// library can write a full userdata's payload bytes. So the bytes are the one
// place a forgery cannot reach, provided two things are established first -
// that the value really is a full userdata, and that it is long enough to
// hold a wrapper. lua_touserdata and lua_rawlen answer both, and together
// they are cheaper than fetching the metatable was.
//
// What remains is a foreign full userdata whose first word happens to equal a
// registered class metatable's address. A script cannot aim for one: it never
// learns that address, and cannot write those bytes if it did.
//
// Two registry tables of our own carry the rest, both consulted off the hot
// path, and both keyed by that same metatable address as a light userdata:
//
//   REGISTRY["mirror_bridge.upcasts"][base type key][derived class_mt] -> thunk
//   REGISTRY["mirror_bridge.names"][class_mt] -> the name bind_class was given
//
// The upcast table converts a derived object's address to the address of a
// specific base subobject, an adjustment that reusing the pointer gets wrong
// for every base after the first. The name table is for error messages, and
// keying it on the stored identity rather than the attached metatable is what
// stops a forged value from describing itself as the class it is imitating.
//
// Nothing is stored on the class metatables themselves: they are what Lua
// consults on every `obj.field`, and extra entries there measurably slow
// down every property access and method lookup.

// The part of LuaWrapper<X> that does not depend on X. Every wrapper starts
// this way, which is what makes a generic read possible - and why the read
// has to be preceded by a check.
struct LuaWrapperView {
    const void* class_mt;
    void* cpp_object;
    bool owns_memory;
};

// The view is read out of memory laid out as a LuaWrapper<T>, so the two have
// to agree. A member added to one and not the other would make every boundary
// check read the wrong word.
static_assert(sizeof(LuaWrapperView) == sizeof(LuaWrapper<int>) &&
              offsetof(LuaWrapperView, class_mt) == offsetof(LuaWrapper<int>, class_mt) &&
              offsetof(LuaWrapperView, cpp_object) == offsetof(LuaWrapper<int>, cpp_object) &&
              offsetof(LuaWrapperView, owns_memory) == offsetof(LuaWrapper<int>, owns_memory),
              "LuaWrapperView must mirror LuaWrapper<T>");

// A Lua value as one of our wrappers, or nullptr. This is the only place
// that turns a Lua value into a pointer into our own memory, so it is the
// only place the bounds argument has to be made.
//
// lua_touserdata is non-null for a light userdata as well, whose payload is
// an address the script chose and whose length is not ours to assume.
// lua_rawlen reports 0 for one - and 0 for every other non-userdata value
// that could reach here - so the length test is what rules it out. It also
// rules out a full userdata from another C library that is too short to hold
// a wrapper, which would otherwise be read out of bounds.
inline LuaWrapperView* lua_wrapper_view(lua_State* L, int idx) {
    void* p = lua_touserdata(L, idx);
    if (!p || lua_rawlen(L, idx) < sizeof(LuaWrapperView)) return nullptr;
    return static_cast<LuaWrapperView*>(p);
}

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
// registered, and Lua then declines the conversion, which is the same answer
// C++ gives at that call site.
//
// The guard does not make such a hierarchy bindable, though: a class that
// inherits one base twice non-virtually also inherits its members twice, and
// reflection enumerates both, so bind_class fails earlier on the ambiguous
// member call. That is older than the upcast table and unchanged by it.
template<typename Derived, typename Base>
void register_lua_upcast(lua_State* L) {
    if constexpr (requires (Derived* d) { static_cast<Base*>(d); }) {
        LuaUpcastThunk thunk = +[](void* p) -> void* {
            return static_cast<void*>(static_cast<Base*>(static_cast<Derived*>(p)));
        };
        push_lua_side_table(L, kLuaUpcastTableKey);
        lua_getfield(L, -1, core::type_key<Base>);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setfield(L, -3, core::type_key<Base>);
        }
        // Keyed by the address of Derived's metatable, which is what an
        // arriving wrapper carries in its bytes, not by the metatable value:
        // the lookup must not depend on anything a script can attach.
        luaL_getmetatable(L, core::type_key<Derived>);
        lua_pushlightuserdata(L, const_cast<void*>(lua_topointer(L, -1)));
        lua_remove(L, -2);
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

// How to reach Expected from the class identified by `derived_mt`, or
// nullptr when there is no such conversion. Leaves the stack as it found it.
inline LuaUpcastThunk find_lua_upcast(lua_State* L, const char* base_tid,
                                      const void* derived_mt) {
    push_lua_side_table(L, kLuaUpcastTableKey);
    lua_getfield(L, -1, base_tid);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        return nullptr;
    }
    lua_pushlightuserdata(L, const_cast<void*>(derived_mt));
    lua_rawget(L, -2);
    LuaUpcastThunk thunk = nullptr;
    if (lua_islightuserdata(L, -1)) {
        thunk = reinterpret_cast<LuaUpcastThunk>(lua_touserdata(L, -1));
    }
    lua_pop(L, 3);
    return thunk;
}

// Whether `id` names T's metatable in this lua_State. The address bind_class
// cached answers the common case without touching the stack; the registry
// answers when the cache belongs to a different state, or when the class was
// bound by a module other than this one.
template<typename T>
inline bool lua_identity_is(lua_State* L, const void* id) {
    if (!id) return false;
    if (id == LuaTypeRegistry<T>::metatable) return true;
    bool same = luaL_getmetatable(L, core::type_key<T>) == LUA_TTABLE &&
                lua_topointer(L, -1) == id;
    lua_pop(L, 1);
    return same;
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
bool resolve_lua_wrapper_slow(lua_State* L, LuaWrapperView* view, void*& raw) {
    if (!view->cpp_object) return false;

    if (lua_identity_is<Expected>(L, view->class_mt)) {
        raw = view->cpp_object;
        return true;
    }

    LuaUpcastThunk to_base =
        find_lua_upcast(L, core::type_key<Expected>, view->class_mt);
    if (!to_base) return false;

    // The held pointer addresses the whole derived object, which is not where
    // a second or virtual base subobject begins.
    raw = to_base(view->cpp_object);
    return raw != nullptr;
}

// Resolve a Lua value to the address of the C++ object it wraps, after
// checking that it really is a wrapper for Expected (or for a class derived
// from it). Returns false - never a bad pointer - for anything else.
//
// Every path that reinterprets a Lua value as a bound class goes through
// here. The common case is two Lua C API calls and a load: establish the
// value is a full userdata long enough to be ours, then compare the identity
// it carries against the address bind_class cached for Expected.
//
// A stale cache - this module bound Expected in a different lua_State - only
// costs a trip through the slow path, which asks that state's registry.
template<typename Expected>
inline bool resolve_lua_wrapper(lua_State* L, int idx, void*& raw) {
    LuaWrapperView* view = lua_wrapper_view(L, idx);
    if (!view) return false;

    const void* want = LuaTypeRegistry<Expected>::metatable;
    if (want && view->class_mt == want) {
        if (!view->cpp_object) return false;
        raw = view->cpp_object;
        return true;
    }
    return resolve_lua_wrapper_slow<Expected>(L, view, raw);
}

// The name bind_class gave the class `id` identifies, or nullptr. Read out of
// the registry rather than a C++ static so it is right even for a class
// another module bound.
inline const char* lua_name_for_identity(lua_State* L, const void* id) {
    if (!id) return nullptr;
    push_lua_side_table(L, kLuaNameTableKey);
    lua_pushlightuserdata(L, const_cast<void*>(id));
    lua_rawget(L, -2);
    const char* name = lua_tostring(L, -1);
    lua_pop(L, 2);
    return name;        // interned in the registry table, so it outlives the pop
}

// Copy the bound name of a C++ type into `out`, falling back to its C++
// spelling, which is still enough to tell two classes apart.
inline void copy_lua_registered_name(lua_State* L, const char* type_id,
                                     char* out, std::size_t out_size) {
    const char* name = nullptr;
    if (luaL_getmetatable(L, type_id) == LUA_TTABLE) {
        name = lua_name_for_identity(L, lua_topointer(L, -1));
    }
    lua_pop(L, 1);
    std::snprintf(out, out_size, "%s", name ? name : type_id);
}

// What arrived, in the words a Lua author would use: the bound class name for
// one of our wrappers, otherwise the Lua type name.
//
// Reads the identity the wrapper carries, not the metatable attached to it,
// so a value wearing a borrowed metatable is described as what it is.
inline void describe_lua_value(lua_State* L, int idx, char* out, std::size_t out_size) {
    const char* name = nullptr;
    if (LuaWrapperView* view = lua_wrapper_view(L, idx)) {
        name = lua_name_for_identity(L, view->class_mt);
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
// shorter message.
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
                    lua_wrong_type_error(L, 1, member_name, core::type_key<T>);
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
                    lua_wrong_type_error(L, 1, member_name, core::type_key<T>);
                    return;
                }

                // A const member is readable and never assignable. Without this
                // branch the declaration below was a default-initialised const
                // and the assignment a write to a const lvalue, so a class with
                // one const field failed to compile the whole module rather
                // than failing to set that one field.
                // Two shapes of the same situation: readable, never
                // assignable. A const member cannot be written, and a member
                // whose type has no default constructor cannot even be
                // converted into, because the setter needs a local to move
                // from. nlohmann::detail::dtoa_impl::diyfp is the real second
                // case and it took the whole of nlohmann-json down.
                if constexpr (std::is_const_v<MemberType>
                              || !std::is_default_constructible_v<MemberType>) {
                    luaL_error(L, "Field is not assignable from Lua: %s", member_name);
                    return;
                } else {
                    MemberType cpp_value;
                    if (!from_lua(L, 3, cpp_value)) {
                        lua_bad_field_error(L, member_name,
                                            core::type_key<MemberType>, 3);
                        return;
                    }

                    (*static_cast<T*>(raw)).[:member:] = std::move(cpp_value);
                }
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
            bad_type_id = core::type_key<method_param_t<T, FuncIndex, Is>>;
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
        return lua_wrong_type_error(L, 1, method_name_sv.data(), core::type_key<T>);
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
            bad_type_id = core::type_key<static_method_param_t<T, FuncIndex, Is>>;
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
    // without freeing anything rather than raise during collection - and the
    // identity has to come from the wrapper's own bytes, since the whole
    // point of reaching __gc by hand is to bring a borrowed metatable along.
    LuaWrapperView* view = lua_wrapper_view(L, 1);
    if (!view || !lua_identity_is<T>(L, view->class_mt)) return 0;
    LuaWrapper<T>* wrapper = static_cast<LuaWrapper<T>*>(static_cast<void*>(view));
    if (wrapper->owns_memory && wrapper->cpp_object) {
        delete wrapper->cpp_object;
        wrapper->cpp_object = nullptr;
    }
    return 0;
}

// ============================================================================
// Constructor Support
// ============================================================================
//
// Which constructors are offered, which can be called, and what a call with
// the wrong arguments says are decided in core (construct_from_host_arguments)
// and shared with the JavaScript backend. What is Lua's own: reading the
// arguments off the stack, and raising the refusal safely.

// Convert the Lua arguments for constructor C and call it. On a conversion
// failure, return nullptr and record which argument refused; the argument
// tuple is destroyed on the way out, before anything raises.
//
// The arguments are converted from copies pushed for this attempt. A
// converter may rewrite the value it reads -- lua_tolstring turns a number
// into a string in place -- and the next candidate reads the same arguments,
// so an attempt that failed part way through used to hand it "5" for 5, or
// 1/3 rounded to fourteen digits. The copies stay on the stack until the
// constructor returns, which also keeps the bytes of a string_view or
// const char* argument alive for the call.
template<typename T, std::size_t C, std::size_t... Is>
T* convert_lua_constructor_args(lua_State* L, int arg_base, std::index_sequence<Is...>,
                                core::ConstructorRefusal& attempt) {
    const int top = lua_gettop(L);
    if (!lua_checkstack(L, static_cast<int>(sizeof...(Is)))) return nullptr;
    (lua_pushvalue(L, arg_base + 1 + static_cast<int>(Is)), ...);

    std::tuple<std::remove_cvref_t<core::constructor_param_t<T, C, Is>>...> cpp_args;

    bool converted = true;
    ([&] {
        if (!converted) return;
        if (!from_lua(L, top + 1 + static_cast<int>(Is), std::get<Is>(cpp_args))) {
            converted = false;
            attempt.position = static_cast<int>(Is) + 1;
            attempt.expected = core::type_key<core::constructor_param_t<T, C, Is>>;
        }
    }(), ...);
    if (!converted) {
        lua_settop(L, top);
        return nullptr;
    }

    T* object = new T(std::move(std::get<Is>(cpp_args))...);
    lua_settop(L, top);
    return object;
}

// The Lua values at arg_base+1 .. arg_base+nargs, described as "number, table".
inline void describe_lua_arguments(lua_State* L, int arg_base, int nargs,
                                   char* out, std::size_t out_size) {
    std::size_t used = 0;
    out[0] = '\0';
    for (int i = 1; i <= nargs && used < out_size; ++i) {
        char one[256];
        describe_lua_value(L, arg_base + i, one, sizeof one);
        const int written = std::snprintf(out + used, out_size - used, "%s%s",
                                          i == 1 ? "" : ", ", one);
        if (written < 0) break;
        used += static_cast<std::size_t>(written);
    }
}

// luaL_error long-jumps out of here, so the class name and the message are
// built in plain buffers and nothing with a destructor is live.
template<typename T>
int raise_lua_constructor_refusal(lua_State* L, const core::ConstructorRefusal& refusal,
                                  int arg_base, int nargs) {
    char name[256];
    copy_lua_registered_name(L, core::type_key<T>, name, sizeof name);

    if (refusal.kind == core::ConstructorRefusal::bad_argument) {
        return lua_bad_argument_error(L, name, refusal.position, refusal.expected,
                                      arg_base + refusal.position);
    }

    char message[1024];
    if (refusal.kind == core::ConstructorRefusal::no_overload_accepts) {
        char got[512];
        describe_lua_arguments(L, arg_base, nargs, got, sizeof got);
        std::snprintf(message, sizeof message, "%s: no constructor taking %d arguments accepts (%s)",
                      name, nargs, got);
    } else {
        core::describe_constructor_refusal<T>(refusal, name, "Lua",
                                              static_cast<std::size_t>(nargs),
                                              message, sizeof message);
    }
    return luaL_error(L, "%s", message);
}

// Construct a T from the Lua values above arg_base: arg_base is 1 when slot 1
// holds the class table (Cls(...) and Cls:new(...)) and 0 when it does not
// (Cls.new(...)).
//
// Every outcome is decided inside the try block and raised after it.
// luaL_error long-jumps, so it must not run while the argument tuple (which
// can own strings and vectors) is alive, nor inside a catch handler, whose
// exception object would never be released.
template<typename T>
int lua_construct(lua_State* L, int arg_base) {
    const int nargs = lua_gettop(L) - arg_base;

    T* cpp_object = nullptr;
    core::ConstructorRefusal refusal;
    char exception_text[512];
    bool threw = false;

    try {
        cpp_object = core::construct_from_host_arguments<T>(
            static_cast<std::size_t>(nargs), refusal,
            [&]<std::size_t C>(core::ConstructorRefusal& attempt) -> T* {
                return convert_lua_constructor_args<T, C>(
                    L, arg_base, std::make_index_sequence<core::constructor_arity<T, C>()>{}, attempt);
            });
    } catch (const std::exception& e) {
        threw = true;
        std::snprintf(exception_text, sizeof exception_text, "C++ constructor exception: %s", e.what());
    } catch (...) {
        threw = true;
        std::snprintf(exception_text, sizeof exception_text, "Unknown C++ exception in constructor");
    }

    if (threw) return luaL_error(L, "%s", exception_text);
    if (!cpp_object) return raise_lua_constructor_refusal<T>(L, refusal, arg_base, nargs);

    // Allocate userdata for wrapper
    LuaWrapper<T>* wrapper = static_cast<LuaWrapper<T>*>(lua_newuserdata(L, sizeof(LuaWrapper<T>)));

    wrapper->cpp_object = cpp_object;
    wrapper->owns_memory = true;

    // Set metatable. The identity is taken from the table actually being
    // installed rather than from the cached address, so a wrapper made in a
    // second lua_State carries that state's metatable.
    luaL_getmetatable(L, core::type_key<T>);
    wrapper->class_mt = lua_topointer(L, -1);
    lua_setmetatable(L, -2);

    return 1;
}

// Cls(...) arrives through the class table's __call metamethod, which passes
// the class table itself first.
template<typename T>
int lua_constructor_call(lua_State* L) {
    return lua_construct<T>(L, 1);
}

// Whether the value at idx is the table at class_idx, or a table that
// inherits from it through a chain of __index metatables -- the receiver of
// Cls:new(...) or of Sub:new(...) after Sub = setmetatable({}, {__index = Cls}).
// A struct written as a table has no such chain, so it stays an argument.
inline bool lua_is_class_or_subclass(lua_State* L, int idx, int class_idx) {
    if (!lua_istable(L, idx)) return false;
    if (lua_rawequal(L, idx, class_idx)) return true;

    const int top = lua_gettop(L);
    bool found = false;
    lua_pushvalue(L, idx);                          // current
    for (int depth = 0; depth < 32 && !found; ++depth) {   // a cycle ends here
        if (!lua_getmetatable(L, -1)) break;        // current, mt
        lua_pushliteral(L, "__index");
        lua_rawget(L, -2);                          // current, mt, __index
        if (!lua_istable(L, -1)) break;
        found = lua_rawequal(L, -1, class_idx);
        lua_replace(L, -3);                         // __index, mt
        lua_pop(L, 1);                              // __index is the new current
    }
    lua_settop(L, top);
    return found;
}

// Cls.new(...) and Cls:new(...) share one field. The colon form passes its
// receiver first and the dot form does not, and a single function that
// always skipped slot 1 -- as __call must -- read Vec3.new(1, 2, 3) as
// (2, 3). The class table is this closure's upvalue, and slot 1 is skipped
// only when it is that table or a table inheriting from it, never merely
// because it is a table: a struct converted from a table is a legitimate
// first argument.
template<typename T>
int lua_constructor_new(lua_State* L) {
    return lua_construct<T>(L, lua_is_class_or_subclass(L, 1, lua_upvalueindex(1)) ? 1 : 0);
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
        // A wrapper of this class or of one derived from it, which is how a
        // composite element round-trips: c.points = {p}, or assigning back
        // what reading c.points returned. Copy assignment is the only way to
        // take one, so a class without it accepts tables only.
        if constexpr (std::is_copy_assignable_v<T>) {
            void* raw = nullptr;
            if (resolve_lua_wrapper<T>(L, idx, raw)) {
                out = *static_cast<T*>(raw);
                return true;
            }
        }
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

            // Set the metatable, and take the identity from it (see
            // lua_construct)
            luaL_getmetatable(L, LuaTypeRegistry<CleanT>::metatable_name);
            wrapper->class_mt = lua_topointer(L, -1);
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
        "Add a custom type converter for them. [[=exclude{}]] will not help here: "
        "this backend binds every named member and does not read annotations yet.");

    static_assert(core::type_key_is_distinctive<T>(),
        "bind_class<T>: this compiler cannot spell T distinctly, so T would share a "
        "metatable with another specialisation and the boundary check could not tell "
        "them apart. Reached by a template argument that is an enum value or a type "
        "with no name. There is no workaround on this compiler: an alias does not "
        "change the spelling, because the key is derived from the type and not from "
        "the name it was bound under. Bind one specialisation, or wrap each in its "
        "own named struct.");

    constexpr std::size_t static_method_count = get_static_member_function_count<T>();

    // Store metatable name in type registry (for to_lua wrapper creation)
    LuaTypeRegistry<T>::metatable_name = core::type_key<T>;

    // Create metatable for this class. The registry keys it by the type's
    // reflection-derived name, so a second module binding the same class
    // finds this very table and the identity check agrees across .so
    // boundaries.
    luaL_newmetatable(L, core::type_key<T>);

    // The address of that metatable is what the exact-type check compares
    // against, and the name is what an error message calls the class. The
    // name goes in a side table rather than on the metatable itself, which
    // Lua reads on every property access.
    LuaTypeRegistry<T>::metatable = lua_topointer(L, -1);
    push_lua_side_table(L, kLuaNameTableKey);
    lua_pushlightuserdata(L, const_cast<void*>(LuaTypeRegistry<T>::metatable));
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
    lua_pushcfunction(L, lua_constructor_call<T>);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);  // set metatable on class table

    // Also add constructor directly as "new", closing over the class table so
    // it can tell Cls:new(...) from Cls.new(...)
    lua_pushvalue(L, -1);
    lua_pushcclosure(L, lua_constructor_new<T>, 1);
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
