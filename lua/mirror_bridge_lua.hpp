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
    if constexpr (std::is_floating_point_v<T>) {
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

// Smart pointers
template<SmartPointer T>
void to_lua(lua_State* L, const T& ptr) {
    if (!ptr) {
        lua_pushnil(L);
        return;
    }
    using ElementType = typename std::remove_cvref_t<T>::element_type;
    to_lua(L, *ptr);
}

// Forward declaration for Bindable types
template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>
>
to_lua(lua_State* L, const T& obj);

// ============================================================================
// Type Conversion: Lua → C++
// ============================================================================

// Arithmetic types
template<Arithmetic T>
bool from_lua(lua_State* L, int idx, T& out) {
    if constexpr (std::is_floating_point_v<T>) {
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

// Smart pointers from Lua
template<SmartPointer T>
bool from_lua(lua_State* L, int idx, T& out) {
    using ElementType = typename std::remove_cvref_t<T>::element_type;

    if (lua_isnil(L, idx)) {
        out.reset();
        return true;
    }

    ElementType value;
    if (!from_lua(L, idx, value)) return false;

    if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::unique_ptr<ElementType>>) {
        out = std::make_unique<ElementType>(std::move(value));
    } else {
        out = std::make_shared<ElementType>(std::move(value));
    }
    return true;
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

    std::tuple<std::remove_cvref_t<typename [:get_method_param_type<T, FuncIndex, Is>():]>...> cpp_args;

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

    if constexpr (std::is_void_v<ReturnType>) {
        ((*wrapper->cpp_object).[:member_func:])(std::move(std::get<Is>(cpp_args))...);
        return 0;
    } else {
        ReturnType result = ((*wrapper->cpp_object).[:member_func:])(std::move(std::get<Is>(cpp_args))...);
        to_lua(L, result);
        return 1;
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

    std::tuple<std::remove_cvref_t<typename [:get_static_method_param_type<T, FuncIndex, Is>():]>...> cpp_args;

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

    if constexpr (std::is_void_v<ReturnType>) {
        [:member_func:](std::move(std::get<Is>(cpp_args))...);
        return 0;
    } else {
        ReturnType result = [:member_func:](std::move(std::get<Is>(cpp_args))...);
        to_lua(L, result);
        return 1;
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

// Call constructor with Lua arguments
template<typename T, std::size_t CtorIndex, std::size_t... Is>
T* call_lua_constructor_impl(lua_State* L, int arg_offset, std::index_sequence<Is...>) {
    using ParamTypes = std::tuple<
        std::remove_cvref_t<typename [:get_lua_constructor_param_type<T, CtorIndex, Is>():]>...
    >;

    std::tuple<std::remove_cvref_t<typename [:get_lua_constructor_param_type<T, CtorIndex, Is>():]>...> cpp_args;

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
    // Get number of arguments (subtract 1 for the class table itself when called via __call)
    int nargs = lua_gettop(L) - 1;

    T* cpp_object = nullptr;

    // Default constructor case
    if (nargs == 0) {
        if constexpr (std::is_default_constructible_v<T>) {
            cpp_object = new T();
        } else {
            luaL_error(L, "This class requires constructor arguments");
            return 0;
        }
    } else {
        // Try to find matching constructor by parameter count
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
                // Try default constructor as fallback if available
                if constexpr (std::is_default_constructible_v<T>) {
                    cpp_object = new T();
                } else {
                    luaL_error(L, "No matching constructor found for %d arguments", nargs);
                    return 0;
                }
            }
        } else {
            // No parameterized constructors, try default
            if constexpr (std::is_default_constructible_v<T>) {
                cpp_object = new T();
            } else {
                luaL_error(L, "This class has no constructors accepting arguments");
                return 0;
            }
        }
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
