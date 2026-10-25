#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge C# — C# bindings for C++ code via C++26 reflection
// ═══════════════════════════════════════════════════════════════════════════
//
// Python, Lua and JavaScript are bound by generating a C++ translation unit
// that calls the host runtime's own C API, so reflection and the binding live
// in one compile. The CLR has no such API to call from C++: every managed
// caller reaches native code through P/Invoke, which needs a flat C ABI on one
// side and C# declarations on the other. SWIG's C# module works the same way,
// for the same reason.
//
// So this backend emits text, and does it in two stages:
//
//   stage 1   a generator program, built from the project's headers with
//             reflection on, writes <module>_shim.cpp and <module>.cs
//   stage 2   <module>_shim.cpp compiles to the shared library C# loads
//
// Text rather than splices because of one hard constraint: P2996 cannot
// declare a function whose *name* is computed, and every P/Invoke target needs
// a unique exported symbol. The alternative is a single indexed entry point
// dispatching on a class id, which costs a jump table on every call -- and the
// point of this project is that the generated path is the same code a person
// would have written by hand.
//
// Both files come out of one reflection pass over one class list, which is
// what keeps them in step: a member renamed in C++ renames the exported
// symbol and the C# property together, because neither is written down twice.
//
// Usage, from the translation unit the CLI generates:
//     #include "csharp/mirror_bridge_csharp.hpp"
//     int main(int argc, char** argv) {
//         mirror_bridge::csharp::Emitter e("my_module", "MyModule");
//         e.add_class<ns::Widget>("Widget", "widget.hpp");
//         return e.write(argv[1]) ? 0 : 1;
//     }

#include "../core/mirror_bridge_core.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <algorithm>
#include <map>
#include <memory>
#include <unordered_map>
#include <set>
#include <sstream>
#include <string>
#include <optional>
#include <typeinfo>
#include <deque>
#include <vector>

namespace mirror_bridge {
namespace csharp {

using namespace mirror_bridge::core;

// ============================================================================
// What crosses the boundary
// ============================================================================
//
// Three spellings are needed for every type that crosses: how the C ABI
// declares it, how C# declares it in the DllImport, and how C# shows it to the
// caller. They differ in two places.
//
// bool: C++ bool is one byte, C# bool marshals as four by default, and the
// mismatch is silent. Both sides use a 32-bit integer on the wire and convert
// at the edge, which is what the Rust backend does for the same reason.
//
// std::string: there is no managed string on the wire. A getter returns a heap
// copy as UTF-8 and the caller frees it through mb_free_string, rather than a
// pointer into the live std::string, because the cheap version dangles the
// moment anything mutates the object and nothing reports it.

enum class Wire {
    None,       // void
    I8, U8, I16, U16, I32, U32, I64, U64,
    F32, F64,
    Bool,       // i32 on the wire
    Utf8,       // const char* out, const char* in
    Object,     // void* handle to a bound class
    Vector,     // void* handle to a std::vector of a wire-able element
    Array,      // T* into a std::array's own storage, with N known at generation
    Map,        // void* handle to a std::map/unordered_map of wire-able K and V
    Opt,        // std::optional of a wire-able scalar or string
    Unsupported
};

template<typename T>
consteval Wire wire_of();

// std::optional<T> where T itself crosses. A scalar needs a separate
// has-value flag on the wire; a string does not, because a null pointer is
// already the natural spelling of "no value".
template<typename, typename = void>
struct optional_element { using type = void; static constexpr bool is_optional = false; };

template<typename O>
struct optional_element<O, std::void_t<typename O::value_type,
                                       decltype(std::declval<const O&>().has_value())>> {
    using type = typename O::value_type;
    static constexpr bool is_optional =
        std::is_same_v<O, std::optional<typename O::value_type>>;
};

template<typename O>
consteval bool is_wire_optional() {
    using OE = optional_element<std::remove_cvref_t<O>>;
    if constexpr (!OE::is_optional) {
        return false;
    } else {
        using E = std::remove_cvref_t<typename OE::type>;
        return std::is_arithmetic_v<E> || core::StringLike<E> || std::is_enum_v<E>;
    }
}

// std::vector<E> where E itself crosses as a scalar, bool or string. The guard
// keeps out the cases that cannot work rather than discovering them later:
// vector<bool> stores bits and hands back a proxy, so there is no element to
// address, and a vector of an unbound class has no C# element type.
template<typename, typename = void>
struct vector_element { using type = void; static constexpr bool is_vector = false; };

template<typename C>
struct vector_element<C, std::void_t<typename C::value_type,
                                     typename C::allocator_type,
                                     decltype(std::declval<C&>().data())>> {
    using type = typename C::value_type;
    static constexpr bool is_vector =
        std::is_same_v<C, std::vector<typename C::value_type,
                                      typename C::allocator_type>>;
};

// std::map and std::unordered_map, told apart from std::set by mapped_type:
// a set has key_type and value_type but no mapped_type. The void_t guard is
// not optional, for the same reason it is not optional for vectors -- naming
// mapped_type outside it makes any member whose type lacks one a hard error in
// the generator instead of a member reported as not crossing.
//
// The two shapes need separate guards because their template parameter lists
// differ: std::map names key_compare, unordered_map names hasher and key_equal,
// and mentioning either on the wrong one is itself a hard error.
template<typename, typename = void>
struct is_ordered_map_t : std::false_type {};

template<typename C>
struct is_ordered_map_t<C, std::void_t<typename C::key_type, typename C::mapped_type,
                                       typename C::key_compare, typename C::allocator_type>>
    : std::bool_constant<std::is_same_v<C, std::map<typename C::key_type,
                                                    typename C::mapped_type,
                                                    typename C::key_compare,
                                                    typename C::allocator_type>>> {};

template<typename, typename = void>
struct is_hashed_map_t : std::false_type {};

template<typename C>
struct is_hashed_map_t<C, std::void_t<typename C::key_type, typename C::mapped_type,
                                      typename C::hasher, typename C::key_equal,
                                      typename C::allocator_type>>
    : std::bool_constant<std::is_same_v<C, std::unordered_map<typename C::key_type,
                                                              typename C::mapped_type,
                                                              typename C::hasher,
                                                              typename C::key_equal,
                                                              typename C::allocator_type>>> {};

template<typename, typename = void>
struct map_kv {
    using key = void;
    using mapped = void;
    static constexpr bool is_map = false;
    static constexpr bool ordered = false;
};

template<typename C>
struct map_kv<C, std::void_t<typename C::key_type, typename C::mapped_type>> {
    using key = typename C::key_type;
    using mapped = typename C::mapped_type;
    static constexpr bool ordered = is_ordered_map_t<C>::value;
    // A custom comparator, hash or allocator makes both checks false, so the
    // map is reported as not crossing rather than bound with a type name this
    // generator cannot spell.
    static constexpr bool is_map = ordered || is_hashed_map_t<C>::value;
};

// Both halves have to cross on their own, and they cross through the vector
// machinery: a snapshot of a map is two owned vectors. That is why an enum key
// is excluded for now -- std::vector<SomeEnum> is not a wire vector either.
template<typename>
struct array_element {
    using type = void;
    static constexpr bool is_array = false;
    static constexpr std::size_t size = 0;
};

template<typename E, std::size_t N>
struct array_element<std::array<E, N>> {
    using type = E;
    static constexpr bool is_array = true;
    static constexpr std::size_t size = N;
};

// The element rule is the vector one, and for the same reasons: bool is out
// because it crosses as int32 while the storage is one byte, so a bulk copy
// would read the wrong bytes. A zero-length array has no storage to point at,
// so data() may be null and there is nothing to view.
template<typename C>
consteval bool is_wire_array() {
    using AE = array_element<std::remove_cvref_t<C>>;
    if constexpr (!AE::is_array) {
        return false;
    } else if constexpr (AE::size == 0) {
        return false;
    } else {
        using E = std::remove_cvref_t<typename AE::type>;
        if constexpr (std::is_same_v<E, bool>) return false;
        else return std::is_arithmetic_v<E> || core::StringLike<E>;
    }
}

template<typename C>
consteval bool is_wire_map() {
    using MK = map_kv<std::remove_cvref_t<C>>;
    if constexpr (!MK::is_map) {
        return false;
    } else {
        using K = std::remove_cvref_t<typename MK::key>;
        using V = std::remove_cvref_t<typename MK::mapped>;
        constexpr bool k_ok = (std::is_arithmetic_v<K> && !std::is_same_v<K, bool>)
                              || core::StringLike<K>;
        constexpr bool v_ok = (std::is_arithmetic_v<V> && !std::is_same_v<V, bool>)
                              || core::StringLike<V>;
        return k_ok && v_ok;
    }
}

template<typename C>
consteval bool is_wire_vector() {
    using VE = vector_element<std::remove_cvref_t<C>>;
    if constexpr (!VE::is_vector) {
        return false;
    } else if constexpr (std::is_same_v<typename VE::type, bool>) {
        return false;
    } else {
        using E = std::remove_cvref_t<typename VE::type>;
        return std::is_arithmetic_v<E> || core::StringLike<E>;
    }
}

template<typename T>
consteval Wire wire_of() {
    using U = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<U, void>) return Wire::None;
    // An enum crosses as its underlying integer and is cast back on both
    // sides. The C# surface is still the enum type, which is what makes the
    // generated API typed rather than a pile of ints -- see cs_name_for.
    else if constexpr (std::is_enum_v<U>) return wire_of<std::underlying_type_t<U>>();
    else if constexpr (std::is_same_v<U, bool>) return Wire::Bool;
    else if constexpr (StringLike<U>) return Wire::Utf8;
    // A bound class crosses as an opaque handle. Whether that handle is owned
    // or borrowed is decided at the call site, not by the type: a member is
    // borrowed from its owner, a value returned by a method is the caller's.
    else if constexpr (is_wire_optional<U>()) return Wire::Opt;
    else if constexpr (is_wire_vector<U>()) return Wire::Vector;
    else if constexpr (is_wire_array<U>()) return Wire::Array;
    else if constexpr (is_wire_map<U>()) return Wire::Map;
    else if constexpr (core::NestedBindable<U>) return Wire::Object;
    else if constexpr (std::is_same_v<U, char> || std::is_same_v<U, signed char>) return Wire::I8;
    else if constexpr (std::is_same_v<U, unsigned char>) return Wire::U8;
    else if constexpr (std::is_same_v<U, short>) return Wire::I16;
    else if constexpr (std::is_same_v<U, unsigned short>) return Wire::U16;
    else if constexpr (std::is_same_v<U, int>) return Wire::I32;
    else if constexpr (std::is_same_v<U, unsigned int>) return Wire::U32;
    else if constexpr (std::is_same_v<U, long> && sizeof(long) == 8) return Wire::I64;
    else if constexpr (std::is_same_v<U, unsigned long> && sizeof(unsigned long) == 8) return Wire::U64;
    else if constexpr (std::is_same_v<U, long>) return Wire::I32;
    else if constexpr (std::is_same_v<U, unsigned long>) return Wire::U32;
    else if constexpr (std::is_same_v<U, long long>) return Wire::I64;
    else if constexpr (std::is_same_v<U, unsigned long long>) return Wire::U64;
    else if constexpr (std::is_same_v<U, float>) return Wire::F32;
    else if constexpr (std::is_same_v<U, double>) return Wire::F64;
    else return Wire::Unsupported;
}

constexpr const char* c_type(Wire w) {
    switch (w) {
        case Wire::None: return "void";
        case Wire::I8:   return "int8_t";
        case Wire::U8:   return "uint8_t";
        case Wire::I16:  return "int16_t";
        case Wire::U16:  return "uint16_t";
        case Wire::I32:  return "int32_t";
        case Wire::U32:  return "uint32_t";
        case Wire::I64:  return "int64_t";
        case Wire::U64:  return "uint64_t";
        case Wire::F32:  return "float";
        case Wire::F64:  return "double";
        case Wire::Bool: return "int32_t";
        case Wire::Utf8: return "const char*";
        case Wire::Object: return "void*";
        case Wire::Vector: return "void*";
        case Wire::Array:  return "void*";
        case Wire::Map:    return "void*";
        // Never nullptr: these results are streamed straight into the output,
        // and streaming a null const char* is undefined behaviour that has
        // already crashed this generator once (Wire::Opt returning nullptr
        // from a site that had not special-cased it yet). A poison spelling
        // turns the same slip into a shim that fails to compile and names the
        // symbol it failed on, which is a bug report instead of a core dump.
        default:         return "MB_WIRE_UNHANDLED";
    }
}

// The type in the extern declaration. A UTF-8 string is an IntPtr there and
// becomes a string only after Marshal converts it, because the managed side
// has to free the native copy and cannot do that through a string parameter.
constexpr const char* cs_wire_type(Wire w) {
    switch (w) {
        case Wire::None: return "void";
        case Wire::I8:   return "sbyte";
        case Wire::U8:   return "byte";
        case Wire::I16:  return "short";
        case Wire::U16:  return "ushort";
        case Wire::I32:  return "int";
        case Wire::U32:  return "uint";
        case Wire::I64:  return "long";
        case Wire::U64:  return "ulong";
        case Wire::F32:  return "float";
        case Wire::F64:  return "double";
        case Wire::Bool: return "int";
        case Wire::Utf8: return "IntPtr";
        case Wire::Object: return "IntPtr";
        case Wire::Vector: return "IntPtr";
        case Wire::Array:  return "IntPtr";
        case Wire::Map:    return "IntPtr";
        default:         return "MB_WIRE_UNHANDLED";
    }
}

// A string parameter is declared differently from a string return. Marshalling
// a string in is free to us: the CLR allocates a native UTF-8 buffer, passes
// it, and releases it. A string coming out cannot be a managed string, because
// then nothing would free the native copy, so it arrives as IntPtr and
// MbNative.TakeString copies and frees it.
constexpr const char* cs_param_type(Wire w) {
    return w == Wire::Utf8 ? "[MarshalAs(UnmanagedType.LPUTF8Str)] string"
                           : cs_wire_type(w);
}

// The type the C# caller sees.
constexpr const char* cs_type(Wire w) {
    switch (w) {
        case Wire::Bool: return "bool";
        case Wire::Utf8: return "string";
        default:         return cs_wire_type(w);
    }
}

constexpr bool wire_ok(Wire w) { return w != Wire::Unsupported; }

// What can cross INWARD, which is a smaller set than what can cross outward.
// A map parameter would need the materialise-or-pass-through prologue that a
// vector parameter has, and that is not written yet, so a method taking one is
// reported by name instead of emitted wrong.
constexpr bool param_wire_ok(Wire w) {
    return wire_ok(w) && w != Wire::Map && w != Wire::Array;
}

// And what can be RETURNED, which excludes a fixed array for a different
// reason than the parameter case: an array returned by value has no identity
// apart from its storage, so a view of it would be a view of a temporary.
// The honest surface is a copied T[], which needs an out-buffer signature
// rather than a handle, and that is its own change.
constexpr bool ret_wire_ok(Wire w) { return wire_ok(w) && w != Wire::Array; }

// What the CONSTRUCTOR and STATIC emitters can spell inbound, which is less
// than emit_method can. Both have a Bool arm, a Utf8 arm and a generic arm and
// nothing else, so a std::vector or a bound class reaching that generic arm is
// declared as a bare void* and forwarded raw. C++ then converts the pointer to
// bool if the target has a bool overload, which means it compiles, calls the
// wrong function, and reports nothing -- `new VecOrBool(xs)` with `void* xs`
// beside a `VecOrBool(bool)` ran the bool constructor and left the vector one
// unreachable. An optional is caught by the MB_WIRE_UNHANDLED poison instead,
// loudly, but it is refused here so the reason is a sentence rather than a
// compile error in generated code.
constexpr bool simple_param_wire_ok(Wire w) {
    return param_wire_ok(w) && w != Wire::Vector && w != Wire::Object
        && w != Wire::Opt;
}

// And outbound, for the static emitter. A value returned by value is the
// caller's copy, so it has to be heap-copied behind a handle before it
// crosses, and only emit_method emits that copy; the static emitter would hand
// back a raw IntPtr where the C# signature says the class, which does not
// compile but kills the module rather than naming the member.
constexpr bool simple_ret_wire_ok(Wire w) {
    return ret_wire_ok(w) && w != Wire::Vector && w != Wire::Map
        && w != Wire::Object && w != Wire::Opt;
}

// ============================================================================
// Why a member or method was left out
// ============================================================================
//
// A generator that silently binds a subset is the failure this project keeps
// finding: the caller reads the generated C#, sees no Transform property, and
// has no way to tell whether the C++ has none or whether it was dropped. So
// every omission is recorded with its reason and written into the .cs as a
// comment, next to where the member would have been.

struct Skipped {
    std::string owner;
    std::string name;
    std::string reason;
};

template<typename E>
consteval const char* cpp_type_name() {
    using U = std::remove_cvref_t<E>;
    if constexpr (core::StringLike<U>)                     return "std::string";
    else if constexpr (std::is_same_v<U, bool>)            return "bool";
    else if constexpr (std::is_same_v<U, char>)            return "char";
    else if constexpr (std::is_same_v<U, signed char>)     return "signed char";
    else if constexpr (std::is_same_v<U, unsigned char>)   return "unsigned char";
    else if constexpr (std::is_same_v<U, short>)           return "short";
    else if constexpr (std::is_same_v<U, unsigned short>)  return "unsigned short";
    else if constexpr (std::is_same_v<U, int>)             return "int";
    else if constexpr (std::is_same_v<U, unsigned int>)    return "unsigned int";
    else if constexpr (std::is_same_v<U, long>)            return "long";
    else if constexpr (std::is_same_v<U, unsigned long>)   return "unsigned long";
    else if constexpr (std::is_same_v<U, long long>)       return "long long";
    else if constexpr (std::is_same_v<U, unsigned long long>) return "unsigned long long";
    else if constexpr (std::is_same_v<U, float>)           return "float";
    else if constexpr (std::is_same_v<U, double>)          return "double";
    else if constexpr (std::is_same_v<U, long double>)     return "long double";
    else return nullptr;
}

struct OptPlan {
    Wire inner = Wire::Unsupported;
    std::string cpp_inner;    // the C++ element type
    std::string cpp_opt;      // the full std::optional<...> spelling
    std::string c_wire;       // the C ABI type the value crosses as
    std::string cs_wire;      // the C# type in the DllImport
    std::string cs_surface;   // the C# type the caller sees
    bool is_string = false;   // a null pointer already means "no value"
};

struct VectorPlan {
    std::string tag;          // the symbol infix, e.g. "d" or "str"
    std::string cpp_elem;     // the C++ element type
    std::string cpp_vec;      // the full std::vector<...> spelling
    std::string cs_elem;      // the C# element type
    std::string cs_wire;      // the C# type on the wire
    std::string cs_name;      // the generated view class
    std::string wire_c;       // the C ABI type one element crosses as
    bool is_string = false;
    std::string shim;
};

// A map is not a third collection: its snapshot is two owned vectors, so the
// key and value halves ARE VectorPlans and the generated C# reuses their view
// classes rather than growing a second indexable type.
struct MapPlan {
    std::string tag;          // the symbol infix, unique per C++ map type
    std::string cpp_key;
    std::string cpp_val;
    std::string cpp_map;      // the full std::map<...>/std::unordered_map<...>
    std::string cs_key;       // the C# surface type of a key
    std::string cs_val;
    std::string cs_key_wire;  // what a key crosses as, C# side
    std::string cs_val_wire;
    std::string wire_key_c;   // and C side
    std::string wire_val_c;
    bool key_is_string = false;
    bool val_is_string = false;
    bool ordered = true;      // std::map, as opposed to std::unordered_map
    std::string cs_name;      // the generated view class
    const VectorPlan* keys = nullptr;
    const VectorPlan* vals = nullptr;
    std::string shim;
};

struct ClassRef {
    std::string cs_name;
    std::string cpp_name;
    std::string handle;
};

struct EnumPlan {
    std::string cpp_name;
    std::string cs_name;
    std::string underlying;      // the C# integral type it derives from
    std::vector<std::pair<std::string, long long>> members;
};

struct ClassPlan {
    std::string cpp_name;     // fully qualified, as C++ spells it
    std::string cs_name;      // the C# class name
    std::string symbol;       // the C ABI infix, e.g. "Widget"
    std::string handle;       // the SafeHandle subclass name
    std::string header;
    bool default_constructible = false;
    std::string shim;         // extern "C" definitions
    std::string externs;      // C# DllImport declarations
    std::string body;         // C# properties and methods
    std::vector<Skipped> skipped;
    // One exported symbol per name, so a second overload cannot be bound.
    std::vector<std::string> emitted_methods;
    // Public C# signatures already taken, so a second overload that C# cannot
    // tell apart is refused rather than emitted twice.
    std::vector<std::string> public_sigs;
    std::vector<std::string> emitted_statics;
    // C# names already spoken for inside this class, so a C++ member called
    // Dispose or Handle is renamed instead of producing source that will not
    // compile. Overloads deliberately share one entry here.
    std::set<std::string> taken_names;
    std::map<std::string, std::string> cs_name_of;
    std::vector<std::pair<std::string, std::string>> renamed;
    // Native symbols already minted for this class. A member `x` and a method
    // `get_x` both want mb_<C>_get_x, and the second definition is a link
    // error, not a warning -- the two name spaces were never disjoint.
    std::set<std::string> symbols;
};

inline std::string escape_cs_keyword(const std::string& name) {
    static const std::set<std::string> reserved = {
        "abstract","as","base","bool","break","byte","case","catch","char",
        "checked","class","const","continue","decimal","default","delegate",
        "do","double","else","enum","event","explicit","extern","false",
        "finally","fixed","float","for","foreach","goto","if","implicit","in",
        "int","interface","internal","is","lock","long","namespace","new",
        "null","object","operator","out","override","params","private",
        "protected","public","readonly","ref","return","sbyte","sealed",
        "short","sizeof","stackalloc","static","string","struct","switch",
        "this","throw","true","try","typeof","uint","ulong","unchecked",
        "unsafe","ushort","using","virtual","void","volatile","while"
    };
    return reserved.count(name) ? "@" + name : name;
}

// A C++ member name is lower_snake by convention and a C# property is Pascal.
// Renaming is the kind of helpfulness that makes a binding hard to search, so
// the C++ spelling is kept and only a leading underscore is dropped, which C#
// allows but which reads as private.
inline std::string cs_member_name(const std::string& name) {
    std::string out = name;
    while (!out.empty() && out.front() == '_') out.erase(out.begin());
    if (out.empty()) out = name;
    return escape_cs_keyword(out);
}

// Names the generated class already uses for itself. A C++ class that declares
// any of them produces C# that does not compile -- a PascalCase codebase with
// `int Handle;` or a method called `Dispose()` is not exotic, it is the
// house style at several large shops. The collision is resolved by suffixing
// the C++ name, which is visible in the generated source rather than silent.
inline bool cs_name_is_reserved(const std::string& n) {
    static const std::set<std::string> taken = {
        "Dispose", "Check", "New", "Delete", "Lib", "Handle", "Borrow",
        "MbAlive", "_h", "_parent", "ReleaseHandle", "IsInvalid", "SetHandle", "Equals",
        "GetHashCode", "GetType", "ToString", "MemberwiseClone", "Finalize",
    };
    return taken.count(n) != 0;
}

// Both generated files are formatted in one place rather than by the care taken
// at every emission site. Brace depth is brace depth, so one function serves
// the C++ shim and the C# alike. The emitters below write lines in any indentation at all and
// this fixes them by brace depth, which means a new emitter cannot introduce a
// formatting bug -- the previous version of this file had three different
// indentations in one class because each site had guessed.
inline std::string reindent_braced(const std::string& src, int base_depth = 0) {
    std::ostringstream out;
    std::istringstream in(src);
    std::string line;
    int depth = base_depth;
    while (std::getline(in, line)) {
        std::size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) { out << "\n"; continue; }
        const std::string body = line.substr(first);

        // Count braces outside string and character literals, so a brace in
        // a C# string constant cannot shift the whole file.
        int opens = 0, closes = 0;
        bool in_str = false, in_chr = false;
        for (std::size_t i = 0; i < body.size(); ++i) {
            const char ch = body[i];
            if (in_str) {
                if (ch == '\\') { ++i; continue; }
                if (ch == '"') in_str = false;
            } else if (in_chr) {
                if (ch == '\\') { ++i; continue; }
                if (ch == '\'') in_chr = false;
            } else if (ch == '"') in_str = true;
            else if (ch == '\'') in_chr = true;
            else if (ch == '/' && i + 1 < body.size() && body[i + 1] == '/') break;
            else if (ch == '{') ++opens;
            else if (ch == '}') ++closes;
        }

        int indent = depth;
        if (body[0] == '}' && closes > opens) indent = depth - 1;
        if (indent < 0) indent = 0;
        out << std::string(static_cast<std::size_t>(indent) * 4, ' ') << body << "\n";
        depth += opens - closes;
        if (depth < 0) depth = 0;
    }
    return out.str();
}

// The raw enumerator list. The Python backend has its own version of this in
// python/mirror_bridge_stubgen.hpp and the two deliberately diverge at the
// tail: that one appends underscores to Python keywords, because
// `Permission.None` is a syntax error in Python with no spelling a caller
// could find. C# has a spelling -- `@None` -- so it escapes instead of
// renaming, and the enumerator keeps the name the C++ gave it.
namespace detail {

template<typename E>
consteval auto enumerators_array() {
    return std::define_static_array(std::meta::enumerators_of(^^E));
}

template<typename E, std::size_t I>
consteval std::string_view enumerator_name() {
    return std::meta::identifier_of(enumerators_array<E>()[I]);
}

template<typename E, std::size_t I>
consteval long long enumerator_value() {
    return static_cast<long long>([:enumerators_array<E>()[I]:]);
}

}  // namespace detail


class Emitter {
public:
    Emitter(std::string module, std::string ns)
        : module_(std::move(module)), namespace_(std::move(ns)) {}

    // cpp_name is passed in rather than read off the reflection. The only
    // spelling certainly valid in the shim's own translation unit is the fully
    // qualified one the caller already has: display_string_of gives an
    // unqualified name, and on an alias it gives the alias, which is how the
    // first shim came out declaring remove_cvref_t<Device>.
    // An enum needs no shim at all: it crosses as its underlying integer and
    // the C# side declares a real enum over the same values, so the typing is
    // free. Register it before the classes that use it, which is what the CLI
    // does, or a member of that type is reported as not crossing.
    template<typename E>
    void add_enum(const char* cs_name, const char* cpp_name) {
        static_assert(std::is_enum_v<std::remove_cvref_t<E>>,
                      "add_enum needs an enumeration type");
        using CleanE = std::remove_cvref_t<E>;
        EnumPlan e;
        e.cpp_name = cpp_name;
        e.cs_name = escape_cs_keyword(cs_name);
        e.underlying = cs_wire_type(wire_of<std::underlying_type_t<CleanE>>());
        constexpr std::size_t count = detail::enumerators_array<CleanE>().size();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ((e.members.emplace_back(
                  escape_cs_keyword(std::string(detail::enumerator_name<CleanE, Is>())),
                  detail::enumerator_value<CleanE, Is>())), ...);
        }(std::make_index_sequence<count>{});
        enum_key_[type_key<CleanE>()] = enums_.size();
        enums_.push_back(std::move(e));
    }

    // Registration is immediate and emission is deferred to write(), because a
    // member of class type has to name a C# type that may not be registered
    // yet: discovery order is not dependency order, and a struct holding one
    // declared after it is ordinary C++.
    template<Bindable T>
    void add_class(const char* cs_name, const char* header, const char* cpp_name) {
        const std::size_t index = classes_.size();
        ClassPlan p;
        p.cpp_name = cpp_name;
        p.cs_name = escape_cs_keyword(cs_name);
        p.symbol = sanitize_symbol(cs_name);
        p.handle = std::string(cs_name) + "Handle";
        p.taken_names.insert(p.cs_name);
        p.taken_names.insert(p.handle);
        p.header = header ? header : "";
        p.default_constructible = std::is_default_constructible_v<T>;
        if (!p.header.empty()) headers_.insert(p.header);
        class_key_[type_key<std::remove_cvref_t<T>>()] =
            ClassRef{p.cs_name, p.cpp_name, p.handle};
        classes_.push_back(std::move(p));

        pending_.push_back([this, index]() {
            ClassPlan& cp = classes_[index];
            std::ostringstream shim, externs, body;
            emit_lifetime<T>(cp, shim, externs, body);
            emit_members<T>(cp, shim, externs, body);
            emit_methods<T>(cp, shim, externs, body);
            emit_static_methods<T>(cp, shim, externs, body);
                emit_constructors<T>(cp, shim, externs, body);
            emit_upcasts<T>(cp, shim, externs, body);
            cp.shim = shim.str();
            cp.externs = externs.str();
            cp.body = body.str();
        });
    }

    void flush() {
        for (auto& fn : pending_) fn();
        pending_.clear();
    }

    bool write(const std::string& out_dir) {
        flush();
        return write_files(out_dir);
    }

    // The C header for the shim, derived from the shim's own text rather than
    // emitted a second time. Every declaration here IS a shim signature with
    // the body removed, so the two cannot drift -- which is the failure this
    // project keeps finding when one thing is written twice.
    //
    // It exists because the shim is already a flat C ABI with no reflection in
    // it, so anything that can call C can use these bindings: that is issue #3,
    // asked in December, "generate the bindings and then use them from, say,
    // C++17 in a normal compiler".
    static std::string c_header_source(const std::string& module,
                                       const std::string& shim) {
        std::string guard = "MIRROR_BRIDGE_";
        for (char ch : module) {
            guard += static_cast<char>(std::isalnum(static_cast<unsigned char>(ch))
                                       ? std::toupper(static_cast<unsigned char>(ch)) : '_');
        }
        guard += "_H";

        std::ostringstream o;
        o << "/* Generated by mirror_bridge from C++26 reflection. Do not edit.\n"
          << " *\n"
          << " * The flat C ABI of " << module << ". Every declaration below is a\n"
          << " * signature from " << module << "_shim.cpp with its body removed, so the\n"
          << " * header cannot drift from the library.\n"
          << " *\n"
          << " * The shim contains no reflection, so this is callable from any C or C++\n"
          << " * compiler, not only one that can do C++26. A handle is an opaque void*\n"
          << " * owned by whoever the entry point's name says owns it: mb_<C>_new and a\n"
          << " * by-value return give you one to free with mb_<C>_delete, while a member\n"
          << " * accessor hands back a pointer interior to its owner that you must not\n"
          << " * free and must not use after the owner is gone.\n"
          << " *\n"
          << " * A string returned from here is a malloc'd UTF-8 copy: free it with\n"
          << " * mb_free_string. After any call that can fail, mb_last_error returns\n"
          << " * non-NULL and mb_last_error_kind says which C++ exception it was.\n"
          << " */\n"
          << "#ifndef " << guard << "\n"
          << "#define " << guard << "\n\n"
          << "#include <stdint.h>\n\n"
          << "#ifdef __cplusplus\n"
          << "extern \"C\" {\n"
          << "#endif\n\n";

        std::istringstream in(shim);
        std::string line;
        std::size_t count = 0;
        while (std::getline(in, line)) {
            const std::string tag = "MB_EXPORT ";
            if (line.compare(0, tag.size(), tag) != 0) continue;
            std::string decl = line.substr(tag.size());
            const std::size_t brace = decl.rfind(" {");
            if (brace != std::string::npos) decl = decl.substr(0, brace);
            o << decl << ";\n";
            ++count;
        }

        o << "\n#ifdef __cplusplus\n"
          << "}\n"
          << "#endif\n\n"
          << "#endif  /* " << guard << " */\n";
        if (count == 0) return std::string();
        return o.str();
    }

    bool write_files(const std::string& out_dir) const {
        const std::string shim_path = out_dir + "/" + module_ + "_shim.cpp";
        const std::string cs_path = out_dir + "/" + module_ + ".cs";
        const std::string h_path = out_dir + "/" + module_ + ".h";
        std::ofstream shim(shim_path), cs(cs_path), hdr(h_path);
        if (!shim || !cs || !hdr) {
            std::fprintf(stderr, "mirror_bridge csharp: cannot write into %s\n",
                         out_dir.c_str());
            return false;
        }
        const std::string shim_text = shim_source();
        shim << shim_text;
        cs << cs_source();
        hdr << c_header_source(module_, shim_text);
        if (!shim || !cs || !hdr) {
            std::fprintf(stderr, "mirror_bridge csharp: write failed\n");
            return false;
        }
        report(stdout);
        return true;
    }

    void report(std::FILE* out) const {
        std::size_t skipped = 0;
        for (const auto& c : classes_) skipped += c.skipped.size();
        std::fprintf(out, "csharp: %zu class(es), %zu member(s) or method(s) left out\n",
                     classes_.size(), skipped);
        for (const auto& c : classes_) {
            for (const auto& r : c.renamed) {
                std::fprintf(out, "  %s.%s renamed to %s: the generated class "
                                  "already uses that name\n",
                             c.cs_name.c_str(), r.first.c_str(), r.second.c_str());
            }
            for (const auto& s : c.skipped) {
                std::fprintf(out, "  %s.%s: %s\n",
                             c.cs_name.c_str(), s.name.c_str(), s.reason.c_str());
            }
        }
    }

private:
    std::string module_;
    std::string namespace_;
    std::vector<ClassPlan> classes_;
    std::vector<EnumPlan> enums_;
    // Keyed on display_string_of, which is the only spelling the emission
    // sites and the registration site can both compute from the type alone.
    // The value indexes enums_ so both the C# name and the C++ one are
    // reachable: the shim has to name the enum type to forward a parameter,
    // and decltype cannot reach a parameter's type from the call site.
    std::map<std::string, std::size_t> enum_key_;
    // Every class the caller will register, recorded before any of them are
    // emitted. A member of class type has to name a C# type that may be
    // declared later in the file, and discovery order is not dependency order.
    std::map<std::string, ClassRef> class_key_;
    // Emission is deferred for the same reason: the closure runs once every
    // registration is in, so a forward reference resolves.
    std::vector<std::function<void()>> pending_;
    // Keyed on the vector type, so std::vector<double> used by three classes
    // produces one set of element helpers and one view class rather than three.
    std::map<std::string, std::size_t> vector_key_;
    // A deque, not a vector: vector_plan hands out pointers into this
    // container and a later registration must not invalidate them.
    std::deque<VectorPlan> vectors_;
    std::map<std::string, int> helper_tags_;
    std::map<std::string, std::size_t> map_key_;
    std::deque<MapPlan> maps_;
    std::set<std::string> headers_;

    // The type the C# caller sees. For an enum that is the registered enum
    // name rather than the integer it travels as, so the generated API stays
    // typed; an unregistered enum falls back to the integer, which is honest
    // but worth reporting, so callers of this check cs_is_untyped_enum too.
    // typeid rather than a reflected name: display_string_of prints an alias
    // as the alias, so any key built from ^^SomeAlias<T> keys on the alias's
    // own spelling and two sites that spell the type differently never agree.
    template<typename U>
    static std::string type_key() {
        return std::string(typeid(U).name());
    }

    // Registered on first use, because which vector types exist is a property
    // of the members and returns actually bound, not of anything the caller
    // declares up front.
    // No registry: an optional needs no shared helpers, so it is described
    // where it is used.
    template<typename O>
    OptPlan opt_plan() const {
        using U = std::remove_cvref_t<O>;
        OptPlan op;
        if constexpr (is_wire_optional<U>()) {
            using E = std::remove_cvref_t<typename optional_element<U>::type>;
            constexpr Wire ew = wire_of<E>();
            op.inner = ew;
            const char* in = cpp_type_name<E>();
            op.cpp_inner = in ? std::string(in) : std::string();
            if constexpr (std::is_enum_v<E>) {
                const EnumPlan* ep = enum_plan<E>();
                op.cpp_inner = ep ? ep->cpp_name : std::string();
                op.cs_surface = (ep ? ep->cs_name : std::string("int")) + "?";
            }
            op.cpp_opt = "std::optional<" + op.cpp_inner + ">";
            op.is_string = (ew == Wire::Utf8);
            const char* cw = c_type(ew);
            const char* sw = cs_wire_type(ew);
            op.c_wire = cw ? cw : "";
            op.cs_wire = sw ? sw : "";
            if (op.cs_surface.empty()) {
                // A string is already nullable in C#; everything else needs
                // the nullable spelling.
                op.cs_surface = op.is_string ? std::string("string")
                                             : std::string(cs_type(ew)) + "?";
            }
        }
        return op;
    }

    template<typename O>
    bool opt_ok() const {
        using U = std::remove_cvref_t<O>;
        if constexpr (!is_wire_optional<U>()) return false;
        else {
            const OptPlan op = opt_plan<O>();
            return !op.cpp_inner.empty() && !op.c_wire.empty();
        }
    }

    template<typename V>
    const VectorPlan* vector_plan() {
        using U = std::remove_cvref_t<V>;
        if constexpr (!is_wire_vector<U>()) {
            return nullptr;
        } else {
            const std::string key = type_key<U>();
            const auto it = vector_key_.find(key);
            if (it != vector_key_.end()) return &vectors_[it->second];

            using E = std::remove_cvref_t<typename vector_element<U>::type>;
            constexpr Wire ew = wire_of<E>();
            VectorPlan vp;
            constexpr const char* en = cpp_type_name<E>();
            static_assert(cpp_type_name<E>() != nullptr,
                          "is_wire_vector admitted an element cpp_type_name cannot spell");
            vp.cpp_elem = en;
            vp.cpp_vec = std::string("std::vector<") + en + ">";
            vp.cs_elem = cs_type(ew);
            vp.cs_wire = cs_wire_type(ew);
            vp.is_string = (ew == Wire::Utf8);
            vp.wire_c = vp.is_string ? "const char*" : c_type(ew);
            // The tag names this element type's helper symbols and its C#
            // class, so it has to be unique per C++ type. The C# spelling is
            // not: cs_type maps every 64-bit signed type to "long", so
            // vector<long> and vector<long long> are two plans (the key is the
            // C++ type) that would mint one set of mb_vec_long_* symbols and
            // one MbLongVector class -- a duplicate symbol in the shim and
            // CS0101 in the C#. The first claimant keeps the plain name, so
            // every name that exists today is unchanged.
            vp.tag = claim_helper_tag(sanitize_symbol(vp.cs_elem));
            vp.cs_name = "Mb"
                       + std::string(1, static_cast<char>(std::toupper(vp.tag[0])))
                       + vp.tag.substr(1) + "Vector";
            vp.shim = vector_shim(vp);
            vector_key_[key] = vectors_.size();
            vectors_.push_back(std::move(vp));
            return &vectors_[vector_key_[key]];
        }
    }

    template<typename C>
    const MapPlan* map_plan() {
        using U = std::remove_cvref_t<C>;
        if constexpr (!is_wire_map<U>()) {
            return nullptr;
        } else {
            const std::string key = type_key<U>();
            const auto it = map_key_.find(key);
            if (it != map_key_.end()) return &maps_[it->second];

            using MK = map_kv<U>;
            using K = std::remove_cvref_t<typename MK::key>;
            using V = std::remove_cvref_t<typename MK::mapped>;
            constexpr Wire kw = wire_of<K>();
            constexpr Wire vw = wire_of<V>();
            static_assert(cpp_type_name<K>() != nullptr && cpp_type_name<V>() != nullptr,
                          "is_wire_map admitted a half cpp_type_name cannot spell");

            MapPlan mp;
            mp.cpp_key = cpp_type_name<K>();
            mp.cpp_val = cpp_type_name<V>();
            mp.ordered = MK::ordered;
            mp.cpp_map = std::string(mp.ordered ? "std::map<" : "std::unordered_map<")
                       + mp.cpp_key + ", " + mp.cpp_val + ">";
            mp.cs_key = cs_type(kw);
            mp.cs_val = cs_type(vw);
            mp.cs_key_wire = cs_wire_type(kw);
            mp.cs_val_wire = cs_wire_type(vw);
            mp.key_is_string = (kw == Wire::Utf8);
            mp.val_is_string = (vw == Wire::Utf8);
            mp.wire_key_c = mp.key_is_string ? "const char*" : c_type(kw);
            mp.wire_val_c = mp.val_is_string ? "const char*" : c_type(vw);

            // Registering the halves here is what makes the snapshot free: the
            // two view classes and their helper sets already exist, and a
            // module that also has a plain std::vector<K> member shares them.
            mp.keys = vector_plan<std::vector<K>>();
            mp.vals = vector_plan<std::vector<V>>();

            mp.tag = claim_helper_tag(sanitize_symbol(mp.cs_key + "_" + mp.cs_val));
            mp.cs_name = "Mb"
                       + std::string(1, static_cast<char>(std::toupper(mp.cs_key[0])))
                       + mp.cs_key.substr(1)
                       + std::string(1, static_cast<char>(std::toupper(mp.cs_val[0])))
                       + mp.cs_val.substr(1)
                       + (mp.ordered ? "Map" : "HashMap");
            mp.shim = map_shim(mp);
            map_key_[key] = maps_.size();
            maps_.push_back(std::move(mp));
            return &maps_[map_key_[key]];
        }
    }

    // One traversal, two owned vectors. Handing back keys and values as two
    // separate calls would let the map change between them and pair key i with
    // a value that is no longer its own; there is no way for the caller to
    // detect that, so the API does not offer it.
    static std::string map_shim(const MapPlan& mp) {
        std::ostringstream o;
        const std::string M = mp.cpp_map;
        const std::string pre = "mb_map_" + mp.tag;
        const std::string kw = mp.key_is_string ? "const char*" : mp.wire_key_c;
        const std::string vw = mp.val_is_string ? "const char*" : mp.wire_val_c;
        // A string arriving from C# can be null if the caller passed null; the
        // empty string is the only sane reading and matches the vector helpers.
        const std::string k_in = mp.key_is_string
            ? "std::string(k ? k : \"\")"
            : "static_cast<" + mp.cpp_key + ">(k)";
        const std::string v_in = mp.val_is_string
            ? "std::string(v ? v : \"\")"
            : "static_cast<" + mp.cpp_val + ">(v)";

        o << "MB_EXPORT int64_t " << pre << "_size(void* m) {\n"
          << "return static_cast<int64_t>(static_cast<" << M << "*>(m)->size());\n"
          << "}\n\n"
          << "MB_EXPORT int32_t " << pre << "_contains(void* m, " << kw << " k) {\n"
          << "mb_clear_error();\n"
          << "try { return static_cast<" << M << "*>(m)->count(" << k_in << ") ? 1 : 0; }\n"
          << "catch (const std::exception& ex) { mb_set_exception(ex); return 0; }\n"
          << "}\n\n";

        // A string value needs no out-parameter: a null pointer is already the
        // spelling of "absent", and an empty string is a non-null pointer to a
        // zero byte, so the two stay distinguishable.
        if (mp.val_is_string) {
            o << "MB_EXPORT const char* " << pre << "_get(void* m, " << kw << " k) {\n"
              << "mb_clear_error();\n"
              << "try {\n"
              << "auto* map = static_cast<" << M << "*>(m);\n"
              << "const auto it = map->find(" << k_in << ");\n"
              << "return it == map->end() ? nullptr : mb_dup(it->second);\n"
              << "} catch (const std::exception& ex) { mb_set_exception(ex); return nullptr; }\n"
              << "}\n\n";
        } else {
            o << "MB_EXPORT int32_t " << pre << "_get(void* m, " << kw << " k, "
              << mp.wire_val_c << "* out) {\n"
              << "mb_clear_error();\n"
              << "try {\n"
              << "auto* map = static_cast<" << M << "*>(m);\n"
              << "const auto it = map->find(" << k_in << ");\n"
              << "if (it == map->end()) return 0;\n"
              << "*out = static_cast<" << mp.wire_val_c << ">(it->second);\n"
              << "return 1;\n"
              << "} catch (const std::exception& ex) { mb_set_exception(ex); return 0; }\n"
              << "}\n\n";
        }

        o << "MB_EXPORT void " << pre << "_set(void* m, " << kw << " k, " << vw << " v) {\n"
          << "mb_clear_error();\n"
          << "try { static_cast<" << M << "*>(m)->insert_or_assign("
          << k_in << ", " << v_in << "); }\n"
          << "catch (const std::exception& ex) { mb_set_exception(ex); }\n"
          << "}\n\n"
          << "MB_EXPORT int32_t " << pre << "_erase(void* m, " << kw << " k) {\n"
          << "mb_clear_error();\n"
          << "try { return static_cast<int32_t>(static_cast<" << M << "*>(m)->erase("
          << k_in << ")); }\n"
          << "catch (const std::exception& ex) { mb_set_exception(ex); return 0; }\n"
          << "}\n\n"
          << "MB_EXPORT void " << pre << "_clear(void* m) {\n"
          << "static_cast<" << M << "*>(m)->clear();\n"
          << "}\n\n"
          << "// Both halves in one pass, so key i and value i are from the same\n"
          << "// state of the map. The caller owns both vectors and frees them\n"
          << "// through the vector helpers for their element types.\n"
          << "MB_EXPORT int32_t " << pre << "_snapshot(void* m, void** out_keys, void** out_vals) {\n"
          << "mb_clear_error();\n"
          << "try {\n"
          << "auto* map = static_cast<" << M << "*>(m);\n"
          << "auto ks = std::make_unique<std::vector<" << mp.cpp_key << ">>();\n"
          << "auto vs = std::make_unique<std::vector<" << mp.cpp_val << ">>();\n"
          << "ks->reserve(map->size());\n"
          << "vs->reserve(map->size());\n"
          << "for (const auto& kv : *map) {\n"
          << "ks->push_back(kv.first);\n"
          << "vs->push_back(kv.second);\n"
          << "}\n"
          << "*out_keys = static_cast<void*>(ks.release());\n"
          << "*out_vals = static_cast<void*>(vs.release());\n"
          << "return 1;\n"
          << "} catch (const std::exception& ex) { mb_set_exception(ex); return 0; }\n"
          << "}\n\n"
          << "// Only an owned copy is freed here. A borrowed member view points\n"
          << "// into its owner and must never reach this.\n"
          << "MB_EXPORT void " << pre << "_free(void* m) {\n"
          << "try { delete static_cast<" << M << "*>(m); }\n"
          << "catch (...) {}\n"
          << "}\n\n";
        return o.str();
    }

    // The element helpers. They take the vector by void*, so one set serves a
    // borrowed member and an owned copy alike -- the difference is only who
    // calls _free, and that is decided where the handle was handed out.
    static std::string vector_shim(const VectorPlan& vp) {
        std::ostringstream o;
        const std::string V = vp.cpp_vec;
        const std::string pre = "mb_vec_" + vp.tag;
        // The wire type and the element type are not interchangeable: a
        // std::vector<char> element crosses as int8_t, and naming the vector
        // std::vector<int8_t> would be a different type.
        const std::string wire = vp.is_string ? "const char*" : vp.wire_c;

        o << "MB_EXPORT int64_t " << pre << "_size(void* v) {\n"
          << "return static_cast<int64_t>(static_cast<" << V << "*>(v)->size());\n"
          << "}\n\n"
          << "MB_EXPORT " << wire << " " << pre << "_get(void* v, int64_t i) {\n"
          << "mb_clear_error();\n"
          << "auto* vec = static_cast<" << V << "*>(v);\n"
          << "if (i < 0 || static_cast<std::size_t>(i) >= vec->size()) {\n"
          << "mb_set_error(\"index out of range\", MB_ERR_OUT_OF_RANGE);\n"
          << "return " << (vp.is_string ? std::string("nullptr") : vp.wire_c + "{}") << ";\n"
          << "}\n"
          << "return " << (vp.is_string
                 ? "mb_dup((*vec)[static_cast<std::size_t>(i)])"
                 : "static_cast<" + vp.wire_c + ">((*vec)[static_cast<std::size_t>(i)])") << ";\n"
          << "}\n\n"
          << "MB_EXPORT void " << pre << "_set(void* v, int64_t i, " << wire << " x) {\n"
          << "mb_clear_error();\n"
          << "auto* vec = static_cast<" << V << "*>(v);\n"
          << "if (i < 0 || static_cast<std::size_t>(i) >= vec->size()) {\n"
          << "mb_set_error(\"index out of range\", MB_ERR_OUT_OF_RANGE);\n"
          << "return;\n"
          << "}\n"
          << "try { (*vec)[static_cast<std::size_t>(i)] = "
          << (vp.is_string ? std::string("(x ? x : \"\")")
                           : "static_cast<" + vp.cpp_elem + ">(x)") << "; }\n"
          << "catch (const std::exception& ex) { mb_set_exception(ex); }\n"
          << "}\n\n"
          << "MB_EXPORT void " << pre << "_add(void* v, " << wire << " x) {\n"
          << "mb_clear_error();\n"
          << "try { static_cast<" << V << "*>(v)->push_back("
          << (vp.is_string ? std::string("(x ? x : \"\")")
                           : "static_cast<" + vp.cpp_elem + ">(x)") << "); }\n"
          << "catch (const std::exception& ex) { mb_set_exception(ex); }\n"
          << "}\n\n"
          << "MB_EXPORT void " << pre << "_clear(void* v) {\n"
          << "static_cast<" << V << "*>(v)->clear();\n"
          << "}\n\n"
          << "// Only an owned copy is freed here. A borrowed member view holds a\n"
          << "// pointer into its owner and must never reach this.\n"
          << "MB_EXPORT void " << pre << "_free(void* v) {\n"
          << "try { delete static_cast<" << V << "*>(v); }\n"
          << "catch (...) {}\n"
          << "}\n\n";
        if (!vp.is_string) {
            o << "// The vector's own storage, for a bulk copy that does not pay a\n"
              << "// transition per element. Valid until the vector reallocates.\n"
              << "MB_EXPORT const " << vp.cpp_elem << "* " << pre << "_data(void* v) {\n"
              << "auto* vec = static_cast<" << V << "*>(v);\n"
              << "return vec->empty() ? nullptr : vec->data();\n"
              << "}\n\n";
        }

        // Fixed storage reached as a raw pointer, which is what lets ONE helper
        // set serve every N: the length travels as an argument instead of in
        // the symbol name, so std::array<T,3> and std::array<T,16> share these.
        // Nothing here owns anything, so there is deliberately no _free -- a
        // fixed view is always a window onto bytes someone else owns. These are
        // emitted for every registered element type whether or not an array
        // uses them, so the managed view class can declare both sets of imports
        // without ever naming a symbol that is not in the library.
        const std::string fx = "mb_fix_" + vp.tag;
        o << "MB_EXPORT " << wire << " " << fx << "_get(void* p, int64_t n, int64_t i) {\n"
          << "mb_clear_error();\n"
          << "auto* a = static_cast<" << vp.cpp_elem << "*>(p);\n"
          << "if (i < 0 || i >= n) {\n"
          << "mb_set_error(\"index out of range\", MB_ERR_OUT_OF_RANGE);\n"
          << "return " << (vp.is_string ? std::string("nullptr") : vp.wire_c + "{}") << ";\n"
          << "}\n"
          << "return " << (vp.is_string ? "mb_dup(a[i])"
                 : "static_cast<" + vp.wire_c + ">(a[i])") << ";\n"
          << "}\n\n"
          << "MB_EXPORT void " << fx << "_set(void* p, int64_t n, int64_t i, "
          << wire << " x) {\n"
          << "mb_clear_error();\n"
          << "auto* a = static_cast<" << vp.cpp_elem << "*>(p);\n"
          << "if (i < 0 || i >= n) {\n"
          << "mb_set_error(\"index out of range\", MB_ERR_OUT_OF_RANGE);\n"
          << "return;\n"
          << "}\n"
          << "try { a[i] = "
          << (vp.is_string ? std::string("(x ? x : \"\")")
                           : "static_cast<" + vp.cpp_elem + ">(x)") << "; }\n"
          << "catch (const std::exception& ex) { mb_set_exception(ex); }\n"
          << "}\n\n";
        return o.str();
    }

    // The managed view. Mutation is deliberately not IDictionary: that
    // interface promises Add/Remove/Count semantics over a snapshot, and this
    // is a window onto a live C++ map whose Count is a fresh native read every
    // time. IReadOnlyDictionary is the honest interface, and the mutators are
    // declared beside it so the compiler does not claim more than is true.
    static std::string map_cs(const MapPlan& mp) {
        std::ostringstream o;
        const std::string pre = "mb_map_" + mp.tag;
        const std::string K = mp.cs_key;
        const std::string V = mp.cs_val;
        const std::string N = mp.cs_name;
        const std::string KV = "KeyValuePair<" + K + ", " + V + ">";
        const std::string k_param = mp.key_is_string
            ? "[MarshalAs(UnmanagedType.LPUTF8Str)] string k"
            : mp.cs_key_wire + " k";
        const std::string k_arg = mp.key_is_string ? "key ?? string.Empty" : "key";

        o << "/// <summary>A view of a C++ " << (mp.ordered ? "std::map<" : "std::unordered_map<")
          << mp.cpp_key << ", " << mp.cpp_val << ">.</summary>\n"
          << "/// <remarks>Obtained from a member, this is the real map and writes\n"
          << "/// reach C++. Returned from a method, it is a copy you own and should\n"
          << "/// dispose. Count, ContainsKey and the indexer each read C++ when you\n"
          << "/// call them, so nothing here is a cached snapshot -- but Keys, Values\n"
          << "/// and enumeration take one, in a single pass, so that key i is always\n"
          << "/// paired with the value it had at that moment.</remarks>\n"
          << "public sealed class " << N << " : IReadOnlyDictionary<" << K << ", " << V
          << ">, IDisposable, IMbOwner {\n"
          << "private const string Lib = MbNative.Lib;\n\n"
          << "[DllImport(Lib, EntryPoint = \"" << pre << "_size\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern long MSize(H m);\n\n"
          << "[DllImport(Lib, EntryPoint = \"" << pre << "_contains\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern int MHas(H m, " << k_param << ");\n\n";

        if (mp.val_is_string) {
            o << "[DllImport(Lib, EntryPoint = \"" << pre << "_get\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
              << "private static extern IntPtr MGet(H m, " << k_param << ");\n\n";
        } else {
            o << "[DllImport(Lib, EntryPoint = \"" << pre << "_get\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
              << "private static extern int MGet(H m, " << k_param << ", out "
              << mp.cs_val_wire << " outv);\n\n";
        }

        o << "[DllImport(Lib, EntryPoint = \"" << pre << "_set\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern void MSet(H m, " << k_param << ", "
          << (mp.val_is_string ? "[MarshalAs(UnmanagedType.LPUTF8Str)] string v"
                               : mp.cs_val_wire + " v") << ");\n\n"
          << "[DllImport(Lib, EntryPoint = \"" << pre << "_erase\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern int MErase(H m, " << k_param << ");\n\n"
          << "[DllImport(Lib, EntryPoint = \"" << pre << "_clear\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern void MClear(H m);\n\n"
          << "[DllImport(Lib, EntryPoint = \"" << pre << "_snapshot\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern int MSnap(H m, out IntPtr keys, out IntPtr vals);\n\n"
          << "[DllImport(Lib, EntryPoint = \"" << pre << "_free\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern void MFree(IntPtr m);\n\n"
          << "internal sealed class H : SafeHandle {\n"
          << "internal H(IntPtr h, bool owns) : base(IntPtr.Zero, owns) {\n"
          << "SetHandle(h);\n"
          << "}\n"
          << "public override bool IsInvalid => handle == IntPtr.Zero;\n"
          << "protected override bool ReleaseHandle() {\n"
          << "MFree(handle);\n"
          << "return true;\n"
          << "}\n"
          << "}\n\n"
          << "private readonly H _h;\n"
          << "private readonly object _parent;\n"
          << "private readonly SafeHandle _pin;\n"
          << "private bool _pinned;\n"
          << "private volatile bool _disposed;\n\n"
          << "internal " << N << "(IntPtr m, bool owned, object parent) {\n"
          << "_h = new H(m, owned);\n"
          << "_parent = parent;\n"
          << "if (!owned && parent is IMbOwner o) {\n"
          << "_pin = o.MbHandle;\n"
          << "_pin.DangerousAddRef(ref _pinned);\n"
          << "}\n"
          << "}\n\n"
          << "SafeHandle IMbOwner.MbHandle => _h;\n\n"
          << "bool IMbOwner.MbAlive {\n"
          << "get {\n"
          << "if (_disposed || _h.IsInvalid || _h.IsClosed) return false;\n"
          << "return (_parent as IMbOwner)?.MbAlive != false;\n"
          << "}\n"
          << "}\n\n"
          << "private H P() {\n"
          << "if (_disposed || _h.IsInvalid || _h.IsClosed) {\n"
          << "throw new ObjectDisposedException(\"" << N << "\");\n"
          << "}\n"
          << "if ((_parent as IMbOwner)?.MbAlive == false) {\n"
          << "throw new ObjectDisposedException(\"" << N
          << "\", \"the object this map belongs to has been disposed\");\n"
          << "}\n"
          << "GC.KeepAlive(_parent);\n"
          << "return _h;\n"
          << "}\n\n"
          << "public int Count => checked((int)MSize(P()));\n\n"
          << "public bool ContainsKey(" << K << " key) {\n"
          << "var r = MHas(P(), " << k_arg << ");\n"
          << "MbNative.Throw(\"" << N << ".ContainsKey\");\n"
          << "return r != 0;\n"
          << "}\n\n";

        // The getter. For a string value the absent case is already a null
        // pointer, so there is no second return channel to keep in step; for
        // anything else the found flag and the out-parameter are separate and
        // the flag is the only thing that may be read first.
        if (mp.val_is_string) {
            o << "public bool TryGetValue(" << K << " key, out " << V << " value) {\n"
              << "var r = MGet(P(), " << k_arg << ");\n"
              << "MbNative.Throw(\"" << N << ".TryGetValue\");\n"
              << "if (r == IntPtr.Zero) {\n"
              << "value = null;\n"
              << "return false;\n"
              << "}\n"
              << "value = MbNative.TakeString(r);\n"
              << "return true;\n"
              << "}\n\n";
        } else {
            o << "public bool TryGetValue(" << K << " key, out " << V << " value) {\n"
              << "var found = MGet(P(), " << k_arg << ", out value);\n"
              << "MbNative.Throw(\"" << N << ".TryGetValue\");\n"
              << "if (found == 0) {\n"
              << "value = default(" << V << ");\n"
              << "return false;\n"
              << "}\n"
              << "return true;\n"
              << "}\n\n";
        }

        o << "/// <summary>The value for <paramref name=\"key\"/>.</summary>\n"
          << "/// <exception cref=\"KeyNotFoundException\">if the C++ map has no such\n"
          << "/// key. The setter is insert_or_assign, so it never throws for that\n"
          << "/// reason.</exception>\n"
          << "public " << V << " this[" << K << " key] {\n"
          << "get {\n"
          << V << " v;\n"
          << "if (!TryGetValue(key, out v)) {\n"
          << "throw new KeyNotFoundException(\"" << N << ": no such key\");\n"
          << "}\n"
          << "return v;\n"
          << "}\n"
          << "set {\n"
          << "MSet(P(), " << k_arg << ", value"
          << (mp.val_is_string ? " ?? string.Empty" : "") << ");\n"
          << "MbNative.Throw(\"" << N << "[]\");\n"
          << "}\n"
          << "}\n\n"
          << "/// <summary>Removes the key if present.</summary>\n"
          << "/// <returns>true if something was removed.</returns>\n"
          << "public bool Remove(" << K << " key) {\n"
          << "var n = MErase(P(), " << k_arg << ");\n"
          << "MbNative.Throw(\"" << N << ".Remove\");\n"
          << "return n != 0;\n"
          << "}\n\n"
          << "public void Clear() {\n"
          << "MClear(P());\n"
          << "}\n\n"
          // The two halves come back as owned vectors, read through the view
          // classes their element types already have, and are disposed here.
          // Nothing else in the generated code allocates a pair array.
          << "/// <summary>Every pair, taken in one pass over the C++ map.</summary>\n"
          << "public " << KV << "[] ToArray() {\n"
          << "IntPtr kp, vp;\n"
          << "if (MSnap(P(), out kp, out vp) == 0) {\n"
          << "throw MbNative.LastError(\"" << N << ".ToArray\");\n"
          << "}\n"
          << "using (var ks = new " << mp.keys->cs_name << "(kp, true, null))\n"
          << "using (var vs = new " << mp.vals->cs_name << "(vp, true, null)) {\n"
          << "var n = ks.Count;\n"
          << "var a = new " << KV << "[n];\n"
          << "for (int i = 0; i < n; i++) {\n"
          << "a[i] = new " << KV << "(ks[i], vs[i]);\n"
          << "}\n"
          << "return a;\n"
          << "}\n"
          << "}\n\n"
          << "public IEnumerable<" << K << "> Keys {\n"
          << "get {\n"
          << "var a = ToArray();\n"
          << "var r = new " << K << "[a.Length];\n"
          << "for (int i = 0; i < a.Length; i++) r[i] = a[i].Key;\n"
          << "return r;\n"
          << "}\n"
          << "}\n\n"
          << "public IEnumerable<" << V << "> Values {\n"
          << "get {\n"
          << "var a = ToArray();\n"
          << "var r = new " << V << "[a.Length];\n"
          << "for (int i = 0; i < a.Length; i++) r[i] = a[i].Value;\n"
          << "return r;\n"
          << "}\n"
          << "}\n\n"
          << "public IEnumerator<" << KV << "> GetEnumerator() {\n"
          << "foreach (var kv in ToArray()) yield return kv;\n"
          << "}\n\n"
          << "IEnumerator IEnumerable.GetEnumerator() {\n"
          << "return GetEnumerator();\n"
          << "}\n\n"
          << "public void Dispose() {\n"
          << "_disposed = true;\n"
          << "_h.Dispose();\n"
          << "if (_pinned) {\n"
          << "_pinned = false;\n"
          << "_pin.DangerousRelease();\n"
          << "}\n"
          << "}\n"
          << "}\n\n";
        return o.str();
    }

    // The managed view. A borrowed one points into its owner's vector and must
    // never free it; an owned one came from a method return and must. Both go
    // through a SafeHandle for the same reason the class wrappers do: with a
    // raw IntPtr the GC can release the handle while a call on it is still
    // running.
    static std::string vector_cs(const VectorPlan& vp) {
        std::ostringstream o;
        const std::string pre = "mb_vec_" + vp.tag;
        const std::string E = vp.cs_elem;
        const std::string W = vp.cs_wire;
        const std::string N = vp.cs_name;

        o << "/// <summary>A view of a C++ std::vector<" << vp.cpp_elem << ">.</summary>\n"
          << "/// <remarks>Obtained from a member, this is the real vector and writes\n"
          << "/// reach C++. Returned from a method, it is a copy you own and should\n"
          << "/// dispose. Count is read from C++ on every access, so a vector grown\n"
          << "/// behind your back is seen rather than cached.</remarks>\n"
          << "public sealed class " << N << " : IReadOnlyList<" << E << ">, IDisposable, IMbOwner {\n"
          << "private const string Lib = MbNative.Lib;\n\n"
          << "[DllImport(Lib, EntryPoint = \"" << pre << "_size\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern long VSize(H v);\n\n"
          << "[DllImport(Lib, EntryPoint = \"" << pre << "_get\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern " << W << " VGet(H v, long i);\n\n"
          << "[DllImport(Lib, EntryPoint = \"" << pre << "_set\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern void VSet(H v, long i, "
          << (vp.is_string ? "[MarshalAs(UnmanagedType.LPUTF8Str)] string" : W) << " x);\n\n"
          << "[DllImport(Lib, EntryPoint = \"" << pre << "_add\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern void VAdd(H v, "
          << (vp.is_string ? "[MarshalAs(UnmanagedType.LPUTF8Str)] string" : W) << " x);\n\n"
          << "[DllImport(Lib, EntryPoint = \"" << pre << "_clear\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern void VClear(H v);\n\n"
          << "[DllImport(Lib, EntryPoint = \"" << pre << "_free\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern void VFree(IntPtr v);\n\n"
          << (vp.is_string ? std::string()
              : std::string("[DllImport(Lib, EntryPoint = \"") + pre
                + "_data\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                + "private static extern IntPtr VData(H v);\n\n")
          // The fixed-storage pair. They take H rather than a bare IntPtr for
          // the same reason everything else does: a pointer crossing as an
          // IntPtr can be released by the GC mid-call. A fixed view's H is
          // constructed non-owning, so SafeHandle never tries to free it.
          << "[DllImport(Lib, EntryPoint = \"mb_fix_" << vp.tag
          << "_get\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern " << W << " FGet(H p, long n, long i);\n\n"
          << "[DllImport(Lib, EntryPoint = \"mb_fix_" << vp.tag
          << "_set\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "private static extern void FSet(H p, long n, long i, "
          << (vp.is_string ? "[MarshalAs(UnmanagedType.LPUTF8Str)] string" : W) << " x);\n\n"
          << "internal sealed class H : SafeHandle {\n"
          << "internal H(IntPtr h, bool owns) : base(IntPtr.Zero, owns) {\n"
          << "SetHandle(h);\n"
          << "}\n"
          << "public override bool IsInvalid => handle == IntPtr.Zero;\n"
          << "protected override bool ReleaseHandle() {\n"
          << "VFree(handle);\n"
          << "return true;\n"
          << "}\n"
          << "}\n\n"
          << "private readonly H _h;\n"
          << "private readonly object _parent;\n"
          << "private readonly SafeHandle _pin;\n"
          << "private bool _pinned;\n"
          << "private volatile bool _disposed;\n"
          << "// -1 for a std::vector, whose length is whatever C++ says now.\n"
          << "// Otherwise this is a std::array member and the length is part\n"
          << "// of its C++ type, so it is known here and never asked for.\n"
          << "private readonly long _fixed;\n\n"
          << "internal " << N << "(IntPtr v, bool owned, object parent) {\n"
          << "_h = new H(v, owned);\n"
          << "_parent = parent;\n"
          << "_fixed = -1;\n"
          << "if (!owned && parent is IMbOwner o) {\n"
          << "_pin = o.MbHandle;\n"
          << "_pin.DangerousAddRef(ref _pinned);\n"
          << "}\n"
          << "}\n\n"
          << "/// <summary>A view of fixed-length storage: a std::array member.\n"
          << "/// Reads and writes reach the owner, but the length cannot\n"
          << "/// change, so Add and Clear are not available.</summary>\n"
          << "internal " << N << "(IntPtr p, long n, object parent) {\n"
          << "_h = new H(p, false);\n"
          << "_parent = parent;\n"
          << "_fixed = n;\n"
          << "if (parent is IMbOwner o) {\n"
          << "_pin = o.MbHandle;\n"
          << "_pin.DangerousAddRef(ref _pinned);\n"
          << "}\n"
          << "}\n\n"
          << "SafeHandle IMbOwner.MbHandle => _h;\n\n"
          << "bool IMbOwner.MbAlive {\n"
          << "get {\n"
          << "if (_disposed || _h.IsInvalid || _h.IsClosed) return false;\n"
          << "return (_parent as IMbOwner)?.MbAlive != false;\n"
          << "}\n"
          << "}\n\n"
          << "// Returns the handle, not a pointer: every entry point below takes it\n"
          << "// typed so the marshaller ref-counts it across the call.\n"
          << "private H P() {\n"
          << "if (_disposed || _h.IsInvalid || _h.IsClosed) {\n"
          << "throw new ObjectDisposedException(\"" << N << "\");\n"
          << "}\n"
          << "// A borrowed vector points into its owner. The handle stays\n"
          << "// non-null after the owner is disposed; the storage does not.\n"
          << "if ((_parent as IMbOwner)?.MbAlive == false) {\n"
          << "throw new ObjectDisposedException(\"" << N
          << "\", \"the object this vector belongs to has been disposed\");\n"
          << "}\n"
          << "GC.KeepAlive(_parent);\n"
          << "return _h;\n"
          << "}\n\n"
          << "public int Count => _fixed >= 0 ? checked((int)_fixed)\n"
          << ": checked((int)VSize(P()));\n\n"
          << "/// <summary>True when this is a std::array member, whose length is\n"
          << "/// part of its C++ type and so cannot change.</summary>\n"
          << "public bool IsFixedSize => _fixed >= 0;\n\n"
          << "public " << E << " this[int i] {\n"
          << "get {\n"
          << "var r = _fixed >= 0 ? FGet(P(), _fixed, i) : VGet(P(), i);\n"
          << "MbNative.Throw(\"" << N << "[]\");\n"
          << "return " << (vp.is_string ? "MbNative.TakeString(r)" : "r") << ";\n"
          << "}\n"
          << "set {\n"
          << "if (_fixed >= 0) {\n"
          << "FSet(P(), _fixed, i, value"
          << (vp.is_string ? " ?? string.Empty" : "") << ");\n"
          << "} else {\n"
          << "VSet(P(), i, value" << (vp.is_string ? " ?? string.Empty" : "") << ");\n"
          << "}\n"
          << "MbNative.Throw(\"" << N << "[]\");\n"
          << "}\n"
          << "}\n\n"
          << "/// <exception cref=\"NotSupportedException\">on a std::array member,\n"
          << "/// whose length is fixed by its C++ type.</exception>\n"
          << "public void Add(" << E << " x) {\n"
          << "if (_fixed >= 0) {\n"
          << "throw new NotSupportedException(\"" << N
          << ": this is a fixed-length std::array member\");\n"
          << "}\n"
          << "VAdd(P(), x" << (vp.is_string ? " ?? string.Empty" : "") << ");\n"
          << "MbNative.Throw(\"" << N << ".Add\");\n"
          << "}\n\n"
          << "/// <exception cref=\"NotSupportedException\">on a std::array member.\n"
          << "/// </exception>\n"
          << "public void Clear() {\n"
          << "if (_fixed >= 0) {\n"
          << "throw new NotSupportedException(\"" << N
          << ": this is a fixed-length std::array member\");\n"
          << "}\n"
          << "VClear(P());\n"
          << "}\n\n"
          << "public " << E << "[] ToArray() {\n"
          << "var n = Count;\n"
          << "var a = new " << E << "[n];\n"
          << "if (_fixed >= 0) {\n"
          << "for (int i = 0; i < n; i++) a[i] = this[i];\n"
          << "return a;\n"
          << "}\n"
          << (vp.is_string
              ? std::string("for (int i = 0; i < n; i++) a[i] = this[i];\n")
              // One bulk copy out of the vector's own storage instead of a
              // P/Invoke per element. The shim already exported the data
              // pointer for this and nothing had declared it.
              : std::string("if (n > 0) {\n")
                + "var p = VData(P());\n"
                + "if (p == IntPtr.Zero) throw MbNative.LastError(\"" + N + ".ToArray\");\n"
                + "Marshal.Copy(p, a, 0, n);\n"
                + "}\n")
          << "return a;\n"
          << "}\n\n"
          << "public IEnumerator<" << E << "> GetEnumerator() {\n"
          << "var n = Count;\n"
          << "for (int i = 0; i < n; i++) yield return this[i];\n"
          << "}\n\n"
          << "IEnumerator IEnumerable.GetEnumerator() {\n"
          << "return GetEnumerator();\n"
          << "}\n\n"
          << "public void Dispose() {\n"
          << "_disposed = true;\n"
          << "_h.Dispose();\n"
          << "if (_pinned) {\n"
          << "_pinned = false;\n"
          << "_pin.DangerousRelease();\n"
          << "}\n"
          << "}\n"
          << "}\n\n";
        return o.str();
    }

    template<typename M>
    const ClassRef* class_ref() const {
        using U = std::remove_cvref_t<M>;
        if constexpr (core::NestedBindable<U>) {
            const auto it = class_key_.find(type_key<U>());
            if (it != class_key_.end()) return &it->second;
        }
        return nullptr;
    }

    template<typename M>
    bool object_unregistered() const {
        using U = std::remove_cvref_t<M>;
        // An optional or a vector is a class by NestedBindable's definition and
        // is not an opaque handle, so it must not be judged by this.
        if constexpr (is_wire_optional<U>() || is_wire_vector<U>()) return false;
        else if constexpr (core::NestedBindable<U>) return class_ref<M>() == nullptr;
        return false;
    }

    template<typename M>
    const EnumPlan* enum_plan() const {
        using U = std::remove_cvref_t<M>;
        if constexpr (std::is_enum_v<U>) {
            const auto it = enum_key_.find(type_key<U>());
            if (it != enum_key_.end()) return &enums_[it->second];
        }
        return nullptr;
    }

    // Find-only, for the const paths. Registration happens in vector_plan
    // during shim emission, which always runs before the C# body that needs
    // the name.
    template<typename V>
    const VectorPlan* vector_find() const {
        using U = std::remove_cvref_t<V>;
        if constexpr (!is_wire_vector<U>()) {
            return nullptr;
        } else {
            const auto it = vector_key_.find(type_key<U>());
            return it == vector_key_.end() ? nullptr : &vectors_[it->second];
        }
    }

    template<typename C>
    const MapPlan* map_find() const {
        using U = std::remove_cvref_t<C>;
        if constexpr (!is_wire_map<U>()) {
            return nullptr;
        } else {
            const auto it = map_key_.find(type_key<U>());
            return it == map_key_.end() ? nullptr : &maps_[it->second];
        }
    }

    template<typename M>
    std::string cs_name_for(Wire w) const {
        if (w == Wire::Opt) {
            const OptPlan op = opt_plan<M>();
            if (!op.cs_surface.empty()) return op.cs_surface;
        }
        if (const VectorPlan* v = vector_find<M>()) return v->cs_name;
        // A std::array surfaces as the view class of its element type, which
        // is the same class a std::vector<E> uses -- that sharing is the point.
        if constexpr (is_wire_array<M>()) {
            using AE = array_element<std::remove_cvref_t<M>>;
            using E = std::remove_cvref_t<typename AE::type>;
            if (const VectorPlan* a = vector_find<std::vector<E>>()) return a->cs_name;
        }
        if (const MapPlan* m = map_find<M>()) return m->cs_name;
        if (const ClassRef* c = class_ref<M>()) return c->cs_name;
        const EnumPlan* e = enum_plan<M>();
        return e ? e->cs_name : std::string(cs_type(w));
    }

    // Casts for the two directions across the wire. Only an enum needs one:
    // bool and string are converted by name elsewhere, and an arithmetic type
    // is already the type C# declared.
    template<typename M>
    std::string cs_cast_out(Wire w) const {
        const EnumPlan* e = enum_plan<M>();
        return e ? "(" + e->cs_name + ")" : std::string();
    }

    template<typename M>
    std::string cs_cast_in(Wire w) const {
        const EnumPlan* e = enum_plan<M>();
        return e ? "(" + e->underlying + ")" : std::string();
    }

    // The C++ spelling the shim needs to forward a parameter, so that the
    // argument at the call site is an EXACT match for the overload the
    // generator chose.
    //
    // For an arithmetic parameter this is not cosmetic. Without a cast the
    // shim hands C++ the raw wire variable and lets overload resolution
    // convert, so wherever two C++ overloads collapse to one Wire -- two
    // same-width integer types, or an enum against its underlying type --
    // resolution re-picks at the call site and takes the exact match, which is
    // whichever overload the generator REFUSED. The run then reports one
    // overload bound while the library calls the other, and nothing visible
    // from C# disagrees: `mb_S_iprobe(void* self, int64_t a0)` forwarding a
    // bare `a0` calls `iprobe(long)` on LP64, not the `iprobe(long long)` the
    // skip message says was kept.
    //
    // cpp_type_name, not display_string_of: the table tells long from long
    // long, which is the entire point here, and a reflected spelling would
    // print an alias and differs between compilers.
    template<typename M>
    std::string cpp_cast(Wire) const {
        if (const EnumPlan* e = enum_plan<M>()) {
            return "static_cast<" + e->cpp_name + ">";
        }
        using U = std::remove_cvref_t<M>;
        if constexpr (std::is_arithmetic_v<U> && !std::is_same_v<U, bool>
                      && cpp_type_name<U>() != nullptr) {
            return std::string("static_cast<") + cpp_type_name<U>() + ">";
        } else {
            return std::string();
        }
    }

    // An enum nobody registered would silently become a bare integer in the
    // generated C#, losing the one thing that made it worth binding.
    template<typename M>
    bool enum_unregistered() const {
        using U = std::remove_cvref_t<M>;
        if constexpr (std::is_enum_v<U>) return enum_plan<M>() == nullptr;
        return false;
    }

    // The declared name, or nullptr for a parameter C++ left unnamed (legal,
    // and ordinary in an interface's pure virtuals).
    //
    // std::meta::info is consteval-only, so none of this can take an info as a
    // runtime argument: the lookup happens entirely at compile time and only
    // the resulting pointer crosses into the emitter.
    //
    // This project's Python stubs carry the real C++ parameter names and say so
    // in writing. C# was emitting a0, a1 -- the one backend whose output a human
    // reads directly was the one hiding them.
    template<typename T, std::size_t I, std::size_t P>
    static consteval const char* method_param_ident() {
        constexpr auto func = get_member_function<T, I>();
        auto params = std::meta::parameters_of(func);
        if (std::meta::has_identifier(params[P])) {
            return std::meta::identifier_of(params[P]).data();
        }
        return nullptr;
    }

    template<typename T, std::size_t I, std::size_t P>
    static consteval const char* static_param_ident() {
        constexpr auto func = get_static_member_function<T, I>();
        auto params = std::meta::parameters_of(func);
        if (std::meta::has_identifier(params[P])) {
            return std::meta::identifier_of(params[P]).data();
        }
        return nullptr;
    }

    template<typename T, std::size_t Index, std::size_t P>
    static consteval const char* ctor_param_ident() {
        auto params = std::meta::parameters_of(nth_ctor<T, Index>());
        if (std::meta::has_identifier(params[P])) {
            return std::meta::identifier_of(params[P]).data();
        }
        return nullptr;
    }

    template<typename T, std::size_t I>
    static consteval const char* member_owner() {
        constexpr auto m = get_data_member_with_bases<T, I>();
        auto owner = std::meta::parent_of(m);
        if (std::meta::has_identifier(owner)) {
            return std::meta::identifier_of(owner).data();
        }
        return nullptr;
    }

    template<typename T, std::size_t I>
    static consteval const char* method_owner() {
        constexpr auto f = get_member_function<T, I>();
        auto owner = std::meta::parent_of(f);
        if (std::meta::has_identifier(owner)) {
            return std::meta::identifier_of(owner).data();
        }
        return nullptr;
    }

    // "Owner::name", or just "name" when the declaring class has no name to
    // spell (an anonymous base is not reachable this way and would be a
    // different bug).
    static std::string qualified(const char* owner, const std::string& name) {
        return owner ? std::string(owner) + "::" + name : name;
    }

    // Locals the generated method body declares for itself: `r` for a return
    // value, `p` for a constructed handle, `_has` for an optional's flag, `i`
    // and <name>_arr in the statements that materialise a list argument. C#
    // puts parameters and locals in one scope, so a parameter called r --
    // `int add(int r)` is ordinary C++ -- is CS0128 against `var r = ...`, and
    // the module does not compile.
    //
    // Renaming the parameter is the safe direction, and it is only the C#
    // spelling: the shim uses cpp_arg_name, so the C ABI and the .h keep the
    // name the C++ declared. No prefix would be reliably collision-proof here
    // -- a C++ parameter may legally be called _mb_r as easily as r -- so this
    // compares against the actual list.
    static bool is_generated_local(const std::string& n) {
        static const std::set<std::string> taken = {"r", "p", "i", "_has"};
        if (taken.count(n)) return true;
        return n.size() > 4 && n.compare(n.size() - 4, 4, "_arr") == 0;
    }

    static std::string arg_name(const char* ident, std::size_t pos) {
        std::string out = ident ? escape_cs_keyword(std::string(ident))
                                : "a" + std::to_string(pos);
        while (is_generated_local(out)) out += "_";
        return out;
    }

    // The same parameter as C++ spells it. arg_name escapes a C# keyword to
    // @name, which is not a C++ identifier, so the shim and the .h need this
    // one instead -- every C# keyword in the list is a legal C++ parameter
    // name, and `in`, `out`, `base`, `ref`, `object` and `string` are ordinary
    // ones to meet.
    static std::string cpp_arg_name(const char* ident, std::size_t pos) {
        return ident ? std::string(ident) : "a" + std::to_string(pos);
    }

    // The C# name for a C++ member, renamed only if it would collide with the
    // generated surface or with a sibling. Called once per C++ name: an
    // overload set shares one C# name on purpose, because C# overloads
    // natively and renaming the second one would invent an API.
    // A native symbol nobody else in this class has taken. The suffix is on
    // the symbol only: the C# surface is unaffected, because the generated
    // DllImport names the entry point explicitly.
    static std::string claim_symbol(ClassPlan& p, const std::string& desired) {
        std::string out = desired;
        while (p.symbols.count(out)) out += "_";
        p.symbols.insert(out);
        return out;
    }

    // Module-wide, unlike claim_symbol, because a helper set is shared by every
    // class that mentions the element type. Not static: the ledger has to reset
    // between Emitters or a second module in one process would start numbering
    // where the first left off.
    std::string claim_helper_tag(const std::string& base) {
        auto it = helper_tags_.find(base);
        if (it == helper_tags_.end()) {
            helper_tags_.emplace(base, 1);
            return base;
        }
        return base + std::to_string(++it->second);
    }

    // A C# property X compiles to hidden get_X and set_X methods, so declaring
    // a property `x` and a method `get_x` is "a member is already reserved" --
    // a C# error the native symbol ledger cannot see, because the two names do
    // not collide on the C++ side at all. Properties are emitted before
    // methods, so reserving the accessor spellings here is enough.
    static std::string claim_property_name(ClassPlan& p, const std::string& cpp_name) {
        const std::string out = claim_cs_name(p, cpp_name);
        const std::string bare = (!out.empty() && out.front() == '@')
                               ? out.substr(1) : out;
        p.taken_names.insert("get_" + bare);
        p.taken_names.insert("set_" + bare);
        return out;
    }

    static std::string claim_cs_name(ClassPlan& p, const std::string& cpp_name) {
        const auto seen = p.cs_name_of.find(cpp_name);
        if (seen != p.cs_name_of.end()) return seen->second;

        const std::string desired = cs_member_name(cpp_name);
        std::string out = desired;
        while (cs_name_is_reserved(out) || p.taken_names.count(out)) out += "_";
        if (out != desired) p.renamed.emplace_back(cpp_name, out);
        p.taken_names.insert(out);
        p.cs_name_of[cpp_name] = out;
        return out;
    }

    static std::string sanitize_symbol(const std::string& n) {
        std::string out;
        for (char ch : n) out += (std::isalnum(static_cast<unsigned char>(ch)) ? ch : '_');
        return out;
    }

    // ---- lifetime -----------------------------------------------------

    template<typename T>
    void emit_lifetime(ClassPlan& p, std::ostringstream& shim,
                       std::ostringstream& externs, std::ostringstream& body) {
        const std::string& cpp = p.cpp_name;
        const std::string& sym = p.symbol;
        // Claimed before anything else, so a C++ member called `new` or
        // `delete`-ish cannot take them out from under the lifetime entry
        // points that every other generated call depends on.
        claim_symbol(p, "mb_" + sym + "_new");
        claim_symbol(p, "mb_" + sym + "_delete");

        if (p.default_constructible) {
            shim << "MB_EXPORT void* mb_" << sym << "_new(void) {\n"
                 << "mb_clear_error();\n"
                 << "try { return new " << cpp << "(); }\n"
                 << "catch (const std::exception& ex) { mb_set_exception(ex); }\n"
                 << "catch (...) { mb_set_error(\"" << cpp << "() threw\"); }\n"
                 << "return nullptr;\n"
                 << "}\n\n";
            externs << "[DllImport(Lib, EntryPoint = \"mb_" << sym
                    << "_new\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                    << "private static extern IntPtr New();\n\n";
        } else {
            p.skipped.push_back({p.cs_name, "<constructor>",
                                 "no default constructor, so C# cannot create one"});
        }

        // The one entry point that was not catching, against the shim's own
        // stated invariant. A throwing destructor crossing extern "C" is
        // undefined behaviour, and swallowing is right here specifically: this
        // runs from a critical finalizer, there is no caller able to act on it,
        // and terminating the process is worse than leaking.
        shim << "MB_EXPORT void mb_" << sym << "_delete(void* self) {\n"
             << "try { delete static_cast<" << cpp << "*>(self); }\n"
             << "catch (...) {}\n"
             << "}\n\n";
        externs << "[DllImport(Lib, EntryPoint = \"mb_" << sym
                << "_delete\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                << "private static extern void Delete(IntPtr self);\n\n";

        // A SafeHandle rather than a raw IntPtr, and not for tidiness: with an
        // IntPtr field the last read of `this` is inside the argument list, so
        // the JIT may report the object dead and the finalizer may run Delete
        // while the native call is still on the stack. Passing a SafeHandle
        // makes the runtime ref-count it for the duration of the call, which
        // closes that window. It also makes Dispose idempotent and
        // thread-safe, and gives the handle a critical finalizer.
        body << "/// <summary>Owns the native pointer. Ref-counted across every call,\n"
             << "/// so the GC cannot release it while one is still running.</summary>\n"
             << "public sealed class " << p.handle << " : SafeHandle {\n"
             << "internal " << p.handle << "(IntPtr h, bool owns) : base(IntPtr.Zero, owns) {\n"
             << "SetHandle(h);\n"
             << "}\n"
             << "public override bool IsInvalid => handle == IntPtr.Zero;\n"
             << "protected override bool ReleaseHandle() {\n"
             << "Delete(handle);\n"
             << "return true;\n"
             << "}\n"
             << "}\n\n"
             << "private readonly " << p.handle << " _h;\n"
             << "// A borrowed handle points inside an object someone else owns.\n"
             << "// Holding that owner here is what stops the GC collecting it\n"
             << "// while this view is still alive -- the same strong parent\n"
             << "// reference the Python backend's vector views carry.\n"
             << "private readonly object _parent;\n"
             << "private readonly SafeHandle _pin;\n"
             << "private bool _pinned;\n"
             << "// Separate from the handle on purpose: with a ref outstanding the\n"
             << "// handle is deliberately not closed, so it cannot answer whether\n"
             << "// the owner was disposed, only that the memory is still there.\n"
             << "private volatile bool _disposed;\n\n"
             << "/// <summary>Wraps a pointer this object does not own.</summary>\n"
             << "internal " << p.cs_name << "(IntPtr self, bool owned, object parent) {\n"
             << "_h = new " << p.handle << "(self, owned);\n"
             << "_parent = parent;\n"
             << "// Pinned for this wrapper's whole lifetime, not just around\n"
             << "// one call: checking the owner and then calling is a race, and\n"
             << "// a ref held here means a call already in flight cannot read\n"
             << "// freed memory even if the owner is disposed mid-call.\n"
             << "if (!owned && parent is IMbOwner o) {\n"
             << "_pin = o.MbHandle;\n"
             << "_pin.DangerousAddRef(ref _pinned);\n"
             << "}\n"
             << "}\n\n";

        if (p.default_constructible) {
            body << "public " << p.cs_name << "() {\n"
                 << "var p = New();\n"
                 << "if (p == IntPtr.Zero) throw MbNative.LastError(\"" << p.cs_name << "()\");\n"
                 << "_h = new " << p.handle << "(p, true);\n"
                 << "_parent = null;\n"
                 << "}\n\n";
        }

        body << "SafeHandle IMbOwner.MbHandle => _h;\n\n"
             << "bool IMbOwner.MbAlive {\n"
             << "get {\n"
             << "if (_disposed || _h.IsInvalid || _h.IsClosed) return false;\n"
             << "return (_parent as IMbOwner)?.MbAlive != false;\n"
             << "}\n"
             << "}\n\n"
             << "private " << p.handle << " Check() {\n"
             << "if (_disposed || _h.IsInvalid || _h.IsClosed) {\n"
             << "throw new ObjectDisposedException(\"" << p.cs_name << "\");\n"
             << "}\n"
             << "// A borrowed handle is still \"valid\" after its owner is disposed,\n"
             << "// because nothing wrote to it -- the memory it points at is simply\n"
             << "// gone. Only the owner can answer that.\n"
             << "if ((_parent as IMbOwner)?.MbAlive == false) {\n"
             << "throw new ObjectDisposedException(\"" << p.cs_name
             << "\", \"the object this view belongs to has been disposed\");\n"
             << "}\n"
             << "// Not ceremony. _parent is otherwise written and never read,\n"
             << "// which is a CS0414 warning and an error in any project built\n"
             << "// with warnings-as-errors -- and the read is exactly the\n"
             << "// guarantee wanted: the owner stays reachable for at least as\n"
             << "// long as the call that borrows from it.\n"
             << "GC.KeepAlive(_parent);\n"
             << "return _h;\n"
             << "}\n\n"
             << "public void Dispose() {\n"
             << "_disposed = true;\n"
             << "_h.Dispose();\n"
             << "if (_pinned) {\n"
             << "_pinned = false;\n"
             << "_pin.DangerousRelease();\n"
             << "}\n"
             << "}\n\n"
             << "/// <summary>This object's handle, for passing it to another binding's\n"
             << "/// entry point. Returns the SafeHandle rather than a raw pointer so the\n"
             << "/// marshaller ref-counts it across that call too -- an argument passed\n"
             << "/// as a bare IntPtr can be released by the GC mid-call, which is\n"
             << "/// exactly what typing `self` as the handle prevents.</summary>\n"
             << "internal " << p.handle << " Borrow() {\n"
             << "return Check();\n"
             << "}\n\n";
    }

    // ---- data members -------------------------------------------------

    template<typename T>
    void emit_members(ClassPlan& p, std::ostringstream& shim,
                      std::ostringstream& externs, std::ostringstream& body) {
        constexpr std::size_t n = get_data_member_with_bases_count<T>();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (emit_member<T, Is>(p, shim, externs, body), ...);
        }(std::make_index_sequence<n>{});
    }

    template<typename T, std::size_t I>
    void emit_member(ClassPlan& p, std::ostringstream& shim,
                     std::ostringstream& externs, std::ostringstream& body) {
        constexpr auto member = get_data_member_with_bases<T, I>();
        constexpr const char* owner_id = member_owner<T, I>();
        using Raw = typename [:std::meta::type_of(member):];
        using M = std::remove_cvref_t<Raw>;
        constexpr Wire w = wire_of<M>();
        const std::string name(std::meta::identifier_of(member));

        if (object_unregistered<M>()) {
            constexpr const char* tn =
                std::meta::display_string_of(std::meta::type_of(member)).data();
            p.skipped.push_back({p.cs_name, name,
                std::string("its type ") + tn + " is a class this module does "
                "not bind, so there is no C# type to hand back"});
            return;
        }
        if (enum_unregistered<M>()) {
            p.skipped.push_back({p.cs_name, name,
                "its enum type was not bound, so this would have crossed as a "
                "bare integer and lost the only thing worth binding"});
            return;
        }
        if constexpr (!wire_ok(w)) {
            constexpr const char* tname =
                std::meta::display_string_of(std::meta::type_of(member)).data();
            p.skipped.push_back({p.cs_name, name,
                std::string("type ") + tname
                + " does not cross the C ABI yet. What does: arithmetic types, "
                  "bool, std::string, a bound enum, a bound class, std::vector "
                  "or std::array of a scalar or string, std::map and "
                  "std::unordered_map with a scalar or string key and value, "
                  "and std::optional of a scalar, enum or string"});
            return;
        } else if constexpr ((w == Wire::Vector || w == Wire::Array
                              || w == Wire::Map || w == Wire::Object)
                             && std::is_const_v<Raw>) {
            // Taking the address of a const member gives a const T*, and
            // static_cast cannot turn that into void*. The scalar branches
            // below bind a const member read-only, which is the honest answer
            // there because the value is copied out. A vector or nested-object
            // member is different: it crosses as a pointer INTO the owner
            // precisely so that writes reach the owner, so there is no
            // read-only version of it to hand back without a read-only view
            // type that does not exist yet.
            //
            // Reporting it is what matters. Before this guard the run printed
            // "0 member(s) or method(s) left out" and then failed inside the
            // C++ compiler on the generated shim, which the consumer cannot
            // act on without editing the const off their own header.
            constexpr const char* ctn =
                std::meta::display_string_of(std::meta::type_of(member)).data();
            p.skipped.push_back({p.cs_name, name,
                std::string("it is const (") + ctn + "); a "
                + (w == Wire::Vector ? "vector"
                   : w == Wire::Array ? "fixed array"
                   : w == Wire::Map ? "map" : "nested object")
                + " member crosses as a writable view into the owner, which a "
                  "const member cannot provide"});
            return;
        } else {
            const bool writable = !std::is_const_v<Raw>;
            const std::string prop = claim_property_name(p, name);
            const std::string sym = claim_symbol(p, "mb_" + p.symbol + "_get_" + name);
            const char* ct = c_type(w);

            // A class-typed member is handed back as a pointer INTO the owner,
            // never a copy: writing through it has to reach the real
            // subobject, which is the whole reason to bind it. The managed
            // wrapper is marked not-owning so its handle will not free it, and
            // carries a reference to the owner so the owner cannot be
            // collected first.
            if constexpr (w == Wire::Opt) {
                const OptPlan op = opt_plan<M>();
                if (!opt_ok<M>()) {
                    p.skipped.push_back({p.cs_name, name,
                        "an optional of a type that does not cross"});
                    return;
                }
                const std::string acc = "static_cast<" + p.cpp_name + "*>(self)->"
                                      + qualified(owner_id, name);
                const std::string prop3 = claim_property_name(p, name);
                const bool writable3 = !std::is_const_v<Raw>;

                if (op.is_string) {
                    // A null pointer is already the spelling of "no value", so
                    // a string optional needs no separate flag on the wire.
                    shim << "MB_EXPORT const char* " << sym << "(void* self) {\n"
                         << "mb_clear_error();\n"
                         << "auto& o = " << acc << ";\n"
                         << "if (!o.has_value()) return nullptr;\n"
                         << "try { return mb_dup(*o); }\n"
                         << "catch (...) { mb_set_error(\"" << name
                         << " getter threw\"); return nullptr; }\n"
                         << "}\n\n";
                    externs << "[DllImport(Lib, EntryPoint = \"" << sym
                            << "\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                            << "private static extern IntPtr Get_" << name
                            << "(" << p.handle << " self);\n\n";
                } else {
                    shim << "MB_EXPORT " << op.c_wire << " " << sym
                         << "(void* self, int32_t* has) {\n"
                         << "auto& o = " << acc << ";\n"
                         << "*has = o.has_value() ? 1 : 0;\n"
                         << "return o.has_value() ? static_cast<" << op.c_wire
                         << ">(*o) : " << op.c_wire << "{};\n"
                         << "}\n\n";
                    externs << "[DllImport(Lib, EntryPoint = \"" << sym
                            << "\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                            << "private static extern " << op.cs_wire << " Get_" << name
                            << "(" << p.handle << " self, out int has);\n\n";
                }

                if (writable3) {
                    const std::string ssym = claim_symbol(
                        p, "mb_" + p.symbol + "_set_" + name);
                    if (op.is_string) {
                        shim << "MB_EXPORT void " << ssym
                             << "(void* self, const char* v) {\n"
                             << "mb_clear_error();\n"
                             << "try {\n"
                             << "if (v) " << acc << " = std::string(v);\n"
                             << "else " << acc << ".reset();\n"
                             << "} catch (const std::exception& ex) { mb_set_exception(ex); }\n"
                             << "}\n\n";
                        externs << "[DllImport(Lib, EntryPoint = \"" << ssym
                                << "\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                                << "private static extern void Set_" << name
                                << "(" << p.handle
                                << " self, [MarshalAs(UnmanagedType.LPUTF8Str)] string v);\n\n";
                    } else {
                        shim << "MB_EXPORT void " << ssym << "(void* self, int32_t has, "
                             << op.c_wire << " v) {\n"
                             << "if (has) " << acc << " = static_cast<"
                             << op.cpp_inner << ">(v);\n"
                             << "else " << acc << ".reset();\n"
                             << "}\n\n";
                        externs << "[DllImport(Lib, EntryPoint = \"" << ssym
                                << "\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                                << "private static extern void Set_" << name
                                << "(" << p.handle << " self, int has, "
                                << op.cs_wire << " v);\n\n";
                    }
                }

                body << "/// <summary>A C++ std::optional: null here is nullopt there.</summary>\n"
                     << "public " << op.cs_surface << " " << prop3 << " {\n";
                if (op.is_string) {
                    body << "get {\n"
                         << "var r = Get_" << name << "(Check());\n"
                         << "MbNative.Throw(\"" << name << "\");\n"
                         << "return r == IntPtr.Zero ? null : MbNative.TakeString(r);\n"
                         << "}\n";
                    if (writable3) {
                        body << "set => Set_" << name << "(Check(), value);\n";
                    }
                } else {
                    const std::string bare = op.cs_surface.substr(0, op.cs_surface.size() - 1);
                    body << "get {\n"
                         << "int has;\n"
                         << "var r = Get_" << name << "(Check(), out has);\n"
                         << "return has != 0 ? (" << op.cs_surface << ")("
                         << bare << ")r : null;\n"
                         << "}\n";
                    if (writable3) {
                        body << "set => Set_" << name << "(Check(), value.HasValue ? 1 : 0, "
                             << "value.HasValue ? (" << op.cs_wire << ")value.Value : default("
                             << op.cs_wire << "));\n";
                    }
                }
                body << "}\n\n";
                return;
            }

            if constexpr (w == Wire::Vector) {
                const VectorPlan* vp = vector_plan<M>();
                shim << "MB_EXPORT void* " << sym << "(void* self) {\n"
                     << "return static_cast<void*>(&static_cast<" << p.cpp_name
                     << "*>(self)->" << qualified(owner_id, name) << ");\n"
                     << "}\n\n";
                externs << "[DllImport(Lib, EntryPoint = \"" << sym
                        << "\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                        << "private static extern IntPtr Get_" << name
                        << "(" << p.handle << " self);\n\n";
                const std::string vprop = claim_property_name(p, name);
                body << "private " << vp->cs_name << " _view_" << name << ";\n"
                     << "/// <summary>The C++ vector itself, not a copy: Add and the\n"
                     << "/// indexer reach the owning object.</summary>\n"
                     << "/// <remarks>Cached for the same reason as a member object view:\n"
                     << "/// the vector sits at a fixed offset inside the owner. Count is\n"
                     << "/// still read from C++ on every access, so growth is seen.</remarks>\n"
                     << "public " << vp->cs_name << " " << vprop << " {\n"
                     << "get {\n"
                     << "if (_view_" << name << " == null) {\n"
                     << "_view_" << name << " = new " << vp->cs_name << "(Get_"
                     << name << "(Check()), false, this);\n"
                     << "}\n"
                     << "return _view_" << name << ";\n"
                     << "}\n"
                     << "}\n\n";
                return;
            }

            if constexpr (w == Wire::Array) {
                // One plan per element type, shared with std::vector<E>: the
                // helpers take a raw E* and the length as an argument, so every
                // N reuses them and nothing here is per-N but the number
                // itself, which the C++ type already carries.
                using AE = array_element<M>;
                using E = std::remove_cvref_t<typename AE::type>;
                const VectorPlan* ap = vector_plan<std::vector<E>>();
                shim << "MB_EXPORT void* " << sym << "(void* self) {\n"
                     << "return static_cast<void*>(static_cast<" << p.cpp_name
                     << "*>(self)->" << qualified(owner_id, name) << ".data());\n"
                     << "}\n\n";
                externs << "[DllImport(Lib, EntryPoint = \"" << sym
                        << "\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                        << "private static extern IntPtr Get_" << name
                        << "(" << p.handle << " self);\n\n";
                const std::string aprop = claim_property_name(p, name);
                body << "private " << ap->cs_name << " _view_" << name << ";\n"
                     << "/// <summary>The C++ array itself, not a copy: the indexer\n"
                     << "/// setter reaches the owning object. Its length is part of\n"
                     << "/// the C++ type, so Add and Clear raise\n"
                     << "/// NotSupportedException.</summary>\n"
                     << "public " << ap->cs_name << " " << aprop << " {\n"
                     << "get {\n"
                     << "if (_view_" << name << " == null) {\n"
                     << "_view_" << name << " = new " << ap->cs_name << "(Get_"
                     << name << "(Check()), " << AE::size << "L, this);\n"
                     << "}\n"
                     << "return _view_" << name << ";\n"
                     << "}\n"
                     << "}\n\n";
                return;
            }

            if constexpr (w == Wire::Map) {
                const MapPlan* mp = map_plan<M>();
                shim << "MB_EXPORT void* " << sym << "(void* self) {\n"
                     << "return static_cast<void*>(&static_cast<" << p.cpp_name
                     << "*>(self)->" << qualified(owner_id, name) << ");\n"
                     << "}\n\n";
                externs << "[DllImport(Lib, EntryPoint = \"" << sym
                        << "\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                        << "private static extern IntPtr Get_" << name
                        << "(" << p.handle << " self);\n\n";
                const std::string mprop = claim_property_name(p, name);
                body << "private " << mp->cs_name << " _view_" << name << ";\n"
                     << "/// <summary>The C++ map itself, not a copy: the indexer\n"
                     << "/// setter, Remove and Clear reach the owning object.</summary>\n"
                     << "/// <remarks>Cached for the same reason as a member vector\n"
                     << "/// view: the map sits at a fixed offset inside the owner.\n"
                     << "/// Count is still read from C++ on every access.</remarks>\n"
                     << "public " << mp->cs_name << " " << mprop << " {\n"
                     << "get {\n"
                     << "if (_view_" << name << " == null) {\n"
                     << "_view_" << name << " = new " << mp->cs_name << "(Get_"
                     << name << "(Check()), false, this);\n"
                     << "}\n"
                     << "return _view_" << name << ";\n"
                     << "}\n"
                     << "}\n\n";
                return;
            }

            if constexpr (w == Wire::Object) {
                const ClassRef* cr = class_ref<M>();
                shim << "MB_EXPORT void* " << sym << "(void* self) {\n"
                     << "return static_cast<void*>(&static_cast<" << p.cpp_name
                     << "*>(self)->" << qualified(owner_id, name) << ");\n"
                     << "}\n\n";
                externs << "[DllImport(Lib, EntryPoint = \"" << sym
                        << "\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                        << "private static extern IntPtr Get_" << name
                        << "(" << p.handle << " self);\n\n";
                const std::string prop2 = claim_property_name(p, name);
                body << "private " << cr->cs_name << " _view_" << name << ";\n"
                     << "/// <summary>A view of the C++ member, not a copy: writes reach "
                     << "the owning object.</summary>\n"
                     << "/// <remarks>Cached. The member sits at a fixed offset inside the\n"
                     << "/// owner, so the pointer cannot change while the owner lives, and\n"
                     << "/// building a fresh wrapper per read was only GC pressure.</remarks>\n"
                     << "public " << cr->cs_name << " " << prop2 << " {\n"
                     << "get {\n"
                     << "if (_view_" << name << " == null) {\n"
                     << "_view_" << name << " = new " << cr->cs_name << "(Get_"
                     << name << "(Check()), false, this);\n"
                     << "}\n"
                     << "return _view_" << name << ";\n"
                     << "}\n"
                     << "}\n\n";
                // No setter: assigning a whole subobject would need the C++
                // copy-assignment, and C# `obj.inner = other` reads as a
                // reference rebind. Mutate through the view instead.
                return;
            }

            shim << "MB_EXPORT " << ct << " " << sym << "(void* self) {\n";
            if constexpr (w == Wire::Utf8) {
                shim << "mb_clear_error();\n"
                     << "try { return mb_dup(static_cast<" << p.cpp_name
                     << "*>(self)->" << qualified(owner_id, name) << "); }\n"
                     << "catch (...) { mb_set_error(\"" << name << " getter threw\"); return nullptr; }\n";
            } else if constexpr (w == Wire::Bool) {
                shim << "return static_cast<" << p.cpp_name << "*>(self)->"
                     << qualified(owner_id, name) << " ? 1 : 0;\n";
            } else {
                shim << "return static_cast<" << ct << ">(static_cast<"
                     << p.cpp_name << "*>(self)->" << qualified(owner_id, name) << ");\n";
            }
            shim << "}\n\n";

            externs << "[DllImport(Lib, EntryPoint = \"" << sym
                    << "\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                    << "private static extern " << cs_wire_type(w)
                    << " Get_" << name << "(" << p.handle << " self);\n\n";

            if (writable) {
                const std::string ssym = claim_symbol(p, "mb_" + p.symbol + "_set_" + name);
                shim << "MB_EXPORT void " << ssym << "(void* self, " << ct << " value) {\n";
                if constexpr (w == Wire::Utf8) {
                    shim << "mb_clear_error();\n"
                         << "try { static_cast<" << p.cpp_name << "*>(self)->"
                         << qualified(owner_id, name) << " = value ? value : \"\"; }\n"
                         << "catch (...) { mb_set_error(\"" << name << " setter threw\"); }\n";
                } else if constexpr (w == Wire::Bool) {
                    shim << "static_cast<" << p.cpp_name << "*>(self)->"
                         << qualified(owner_id, name) << " = (value != 0);\n";
                } else {
                    // decltype rather than a spelled-out type: a member whose
                    // type is a nested typedef has no name valid at namespace
                    // scope in the shim, and this needs none.
                    shim << "static_cast<" << p.cpp_name << "*>(self)->"
                         << qualified(owner_id, name) << " = static_cast<decltype(static_cast<"
                         << p.cpp_name << "*>(self)->" << qualified(owner_id, name)
                         << ")>(value);\n";
                }
                shim << "}\n\n";
                externs << "[DllImport(Lib, EntryPoint = \"" << ssym
                        << "\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                        << "private static extern void Set_" << name
                        << "(" << p.handle << " self, " << cs_param_type(w) << " value);\n\n";
            }

            const std::string surface = cs_name_for<M>(w);
            const std::string out_cast = cs_cast_out<M>(w);
            const std::string in_cast = cs_cast_in<M>(w);
            body << "public " << surface << " " << prop << " {\n";
            if constexpr (w == Wire::Utf8) {
                body << "get {\n"
                     << "var r = Get_" << name << "(Check());\n"
                     << "MbNative.Throw(\"" << name << "\");\n"
                     << "return MbNative.TakeString(r);\n"
                     << "}\n";
            } else if constexpr (w == Wire::Bool) {
                body << "get => Get_" << name << "(Check()) != 0;\n";
            } else {
                body << "get => " << out_cast << "Get_" << name << "(Check());\n";
            }
            if (writable) {
                if constexpr (w == Wire::Utf8) {
                    body << "set {\n"
                         << "Set_" << name << "(Check(), value ?? \"\");\n"
                         << "MbNative.Throw(\"" << name << "\");\n"
                         << "}\n";
                } else if constexpr (w == Wire::Bool) {
                    body << "set => Set_" << name << "(Check(), value ? 1 : 0);\n";
                } else {
                    body << "set => Set_" << name << "(Check(), " << in_cast
                         << "value);\n";
                }
            }
            body << "}\n\n";
        }
    }

    // ---- upcasts to a bound base --------------------------------------
    //
    // A generated class is sealed and has no C# base type, so without this a
    // Derived cannot be passed where a Base is expected -- which rules out
    // binding a class hierarchy, and most real C++ libraries are one. Lua and
    // JavaScript both have upcast machinery; this is C#'s.
    //
    // The pointer adjustment happens in C++, where the compiler knows the
    // offsets, rather than by assuming a Derived* is already a Base*: that
    // assumption holds for single non-virtual inheritance and is silently
    // wrong for anything else.
    //
    // The result is a BORROWED view of the base subobject inside the derived
    // object, so it carries the same owner pin every other view does.

    template<typename T>
    static consteval std::size_t base_count() {
        return std::meta::bases_of(^^T, std::meta::access_context::current()).size();
    }

    template<typename T, std::size_t I>
    static consteval auto nth_base_type() {
        return std::meta::type_of(
            std::meta::bases_of(^^T, std::meta::access_context::current())[I]);
    }

    template<typename T>
    void emit_upcasts(ClassPlan& p, std::ostringstream& shim,
                      std::ostringstream& externs, std::ostringstream& body) {
        constexpr std::size_t n = base_count<T>();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (emit_upcast<T, Is>(p, shim, externs, body), ...);
        }(std::make_index_sequence<n>{});
    }

    template<typename T, std::size_t I>
    void emit_upcast(ClassPlan& p, std::ostringstream& shim,
                     std::ostringstream& externs, std::ostringstream& body) {
        using B = std::remove_cvref_t<typename [:nth_base_type<T, I>():]>;
        if constexpr (!core::NestedBindable<B>) {
            return;
        } else {
            const ClassRef* br = class_ref<B>();
            if (!br) {
                // A base this module does not bind has no C# type to convert
                // to. Saying so beats a Derived that silently cannot be passed.
                p.skipped.push_back({p.cs_name, "(base " + std::to_string(I) + ")",
                    "a base class this module does not bind, so there is no C# "
                    "type to convert to"});
                return;
            }
            const std::string sym = claim_symbol(
                p, "mb_" + p.symbol + "_as_" + sanitize_symbol(br->cs_name));
            const std::string method = claim_cs_name(p, "As" + br->cs_name);

            shim << "MB_EXPORT void* " << sym << "(void* self) {\n"
                 << "return static_cast<void*>(static_cast<" << br->cpp_name
                 << "*>(static_cast<" << p.cpp_name << "*>(self)));\n"
                 << "}\n\n";
            externs << "[DllImport(Lib, EntryPoint = \"" << sym
                    << "\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                    << "private static extern IntPtr " << method << "Raw("
                    << p.handle << " self);\n\n";
            body << "/// <summary>This object seen as its <c>" << br->cpp_name
                 << "</c> base.</summary>\n"
                 << "/// <remarks>A view of the base subobject, not a copy, with the\n"
                 << "/// pointer adjusted by C++ rather than assumed. Writes through it\n"
                 << "/// reach this object, and it is only valid while this object is.</remarks>\n"
                 << "public " << br->cs_name << " " << method << "() {\n"
                 << "return new " << br->cs_name << "(" << method
                 << "Raw(Check()), false, this);\n"
                 << "}\n\n"
                 << "/// <summary>So a " << p.cs_name << " can be passed where a "
                 << br->cs_name << " is expected.</summary>\n"
                 << "public static implicit operator " << br->cs_name
                 << "(" << p.cs_name << " value) {\n"
                 << "if (value == null) throw new ArgumentNullException(nameof(value));\n"
                 << "return value." << method << "();\n"
                 << "}\n\n";
        }
    }

    // ---- parameterized constructors -----------------------------------
    //
    // The default constructor keeps the bare mb_<C>_new symbol; every other
    // one is indexed. C# overloads constructors natively, so the managed side
    // needs nothing but distinct native symbols -- the same arrangement the
    // methods use, for the same reason.

    template<typename T>
    void emit_constructors(ClassPlan& p, std::ostringstream& shim,
                           std::ostringstream& externs, std::ostringstream& body) {
        constexpr std::size_t n = ctor_count<T>();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (emit_constructor<T, Is>(p, shim, externs, body), ...);
        }(std::make_index_sequence<n>{});
    }

    // Copy and move constructors are left out deliberately: C# has no
    // spelling that would distinguish `new Foo(other)` as a copy, and binding
    // one would make an innocuous-looking call duplicate a C++ object.
    template<typename T>
    static consteval std::size_t ctor_count() {
        std::size_t n = 0;
        for (auto c : std::meta::members_of(^^T, std::meta::access_context::current())) {
            if (bindable_ctor(c)) ++n;
        }
        return n;
    }

    static consteval bool bindable_ctor(std::meta::info c) {
        return std::meta::is_constructor(c)
            && !std::meta::is_deleted(c)
            && !std::meta::is_special_member_function(c)
            && !std::meta::parameters_of(c).empty();
    }

    template<typename T, std::size_t Index>
    static consteval auto nth_ctor() {
        std::size_t i = 0;
        for (auto c : std::meta::members_of(^^T, std::meta::access_context::current())) {
            if (bindable_ctor(c)) {
                if (i == Index) return c;
                ++i;
            }
        }
        return std::meta::info{};
    }

    template<typename T, std::size_t Index>
    static consteval std::size_t ctor_argc() {
        return std::meta::parameters_of(nth_ctor<T, Index>()).size();
    }

    template<typename T, std::size_t Index, std::size_t P>
    static consteval auto ctor_param_type() {
        return std::meta::type_of(std::meta::parameters_of(nth_ctor<T, Index>())[P]);
    }

    template<typename T, std::size_t Index, std::size_t... Ps>
    static consteval bool ctor_params_ok(std::index_sequence<Ps...>) {
        return (simple_param_wire_ok(wire_of<std::remove_cvref_t<
                    typename [:ctor_param_type<T, Index, Ps>():]>>()) && ... && true);
    }

    template<typename T, std::size_t Index, std::size_t... Ps>
    bool ctor_param_enum_unregistered(std::index_sequence<Ps...>) const {
        bool bad = false;
        ((bad = bad || enum_unregistered<std::remove_cvref_t<
              typename [:ctor_param_type<T, Index, Ps>():]>>()), ...);
        return bad;
    }

    template<typename T, std::size_t Index>
    void emit_constructor(ClassPlan& p, std::ostringstream& shim,
                          std::ostringstream& externs, std::ostringstream& body) {
        constexpr std::size_t argc = ctor_argc<T, Index>();
        if constexpr (!ctor_params_ok<T, Index>(std::make_index_sequence<argc>{})) {
            p.skipped.push_back({p.cs_name,
                p.cs_name + "(" + std::to_string(argc) + " args)",
                "a constructor parameter type does not cross the C ABI yet. A "
                "constructor takes arithmetic types, bool, std::string and bound "
                "enums; a std::vector, a bound class or a std::optional parameter "
                "needs extra wire slots and C# statements before the call that only "
                "an instance method emits today. Construct the object and then "
                "assign the member, or add a setter"});
            return;
        } else if (ctor_param_enum_unregistered<T, Index>(std::make_index_sequence<argc>{})) {
            p.skipped.push_back({p.cs_name,
                p.cs_name + "(" + std::to_string(argc) + " args)",
                "a constructor parameter is an enum that was not bound"});
            return;
        } else {
            const std::string sym = claim_symbol(p, "mb_" + p.symbol + "_new_" + std::to_string(Index));
            const std::string ext_name = "New_" + std::to_string(Index);

            std::string c_params, cs_ext_params, cs_sig_params;
            std::string call_args, fwd_args, cs_types;
            [&]<std::size_t... Ps>(std::index_sequence<Ps...>) {
                ([&] {
                    using PT = std::remove_cvref_t<typename [:ctor_param_type<T, Index, Ps>():]>;
                    constexpr Wire pw = wire_of<PT>();
                    constexpr const char* an_id = ctor_param_ident<T, Index, Ps>();
                    const std::string an = arg_name(an_id, Ps);
                    const std::string cn = cpp_arg_name(an_id, Ps);
                    const std::string sep = c_params.empty() ? "" : ", ";
                    c_params += sep + c_type(pw) + " " + cn;
                    cs_ext_params += sep + cs_param_type(pw) + " " + an;
                    cs_sig_params += sep + cs_name_for<PT>(pw) + " " + an;
                    cs_types += (cs_types.empty() ? "" : ",") + cs_name_for<PT>(pw);
                    if (!call_args.empty()) call_args += ", ";
                    if constexpr (pw == Wire::Bool) {
                        fwd_args += std::string(fwd_args.empty() ? "" : ", ") + "(" + cn + " != 0)";
                        call_args += an + " ? 1 : 0";
                    } else if constexpr (pw == Wire::Utf8) {
                        fwd_args += std::string(fwd_args.empty() ? "" : ", ") + "(" + cn + " ? " + cn + " : \"\")";
                        call_args += an + " ?? \"\"";
                    } else {
                        const std::string cast = cpp_cast<PT>(pw);
                        fwd_args += std::string(fwd_args.empty() ? "" : ", ")
                                  + (cast.empty() ? cn : cast + "(" + cn + ")");
                        call_args += cs_cast_in<PT>(pw) + an;
                    }
                }(), ...);
            }(std::make_index_sequence<argc>{});

            const std::string public_sig = "ctor(" + cs_types + ")";
            if (std::count(p.public_sigs.begin(), p.public_sigs.end(), public_sig)) {
                p.skipped.push_back({p.cs_name, p.cs_name + "(" + cs_types + ")",
                    "an earlier constructor already has that C# signature"});
                return;
            }
            p.public_sigs.push_back(public_sig);

            shim << "MB_EXPORT void* " << sym << "(" << c_params << ") {\n"
                 << "mb_clear_error();\n"
                 << "try { return new " << p.cpp_name << "(" << fwd_args << "); }\n"
                 << "catch (const std::exception& ex) { mb_set_exception(ex); }\n"
                 << "catch (...) { mb_set_error(\"" << p.cpp_name
                 << " constructor threw\"); }\n"
                 << "return nullptr;\n"
                 << "}\n\n";

            externs << "[DllImport(Lib, EntryPoint = \"" << sym
                    << "\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                    << "private static extern IntPtr " << ext_name
                    << "(" << cs_ext_params << ");\n\n";

            body << "public " << p.cs_name << "(" << cs_sig_params << ") {\n"
                 << "var p = " << ext_name << "(" << call_args << ");\n"
                 << "if (p == IntPtr.Zero) throw MbNative.LastError(\""
                 << p.cs_name << " constructor\");\n"
                 << "_h = new " << p.handle << "(p, true);\n"
                 << "_parent = null;\n"
                 << "}\n\n";
        }
    }

    // ---- static methods -----------------------------------------------
    //
    // A static method has no instance, so its entry point takes no self and
    // the C# side is `public static`. The symbol carries an _s_ infix because
    // a class may declare a static and an instance method under one name, and
    // the two would otherwise export the same symbol and fail to link.

    template<typename T>
    void emit_static_methods(ClassPlan& p, std::ostringstream& shim,
                             std::ostringstream& externs, std::ostringstream& body) {
        constexpr std::size_t n = get_static_member_function_count<T>();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (emit_static_method<T, Is>(p, shim, externs, body), ...);
        }(std::make_index_sequence<n>{});
    }

    template<typename T, std::size_t I>
    void emit_static_method(ClassPlan& p, std::ostringstream& shim,
                            std::ostringstream& externs, std::ostringstream& body) {
        constexpr auto func = get_static_member_function<T, I>();
        using Ret = std::remove_cvref_t<typename [:get_static_method_return_type<T, I>():]>;
        constexpr Wire rw = wire_of<Ret>();
        constexpr std::size_t argc = get_static_method_param_count<T, I>();
        const std::string name(std::meta::identifier_of(func));

        if constexpr (!simple_ret_wire_ok(rw)) {
            constexpr const char* rname =
                std::meta::display_string_of(get_static_method_return_type<T, I>()).data();
            p.skipped.push_back({p.cs_name, "static " + name + "()",
                std::string("returns ") + rname + ", which a static method cannot "
                "return yet. A value returned by value is the caller's copy, so it "
                "has to be heap-copied behind a handle before it crosses, and only "
                "an instance method emits that copy -- as an instance method this "
                "return works"});
            return;
        } else if constexpr (!static_params_ok<T, I>(std::make_index_sequence<argc>{})) {
            p.skipped.push_back({p.cs_name, "static " + name + "()",
                "a parameter type does not cross the C ABI yet. A static method "
                "takes arithmetic types, bool, std::string and bound enums; a "
                "std::vector, a bound class or a std::optional parameter needs "
                "extra wire slots and C# statements before the call that only an "
                "instance method emits today -- as an instance method this "
                "parameter works"});
            return;
        } else if (enum_unregistered<Ret>()) {
            p.skipped.push_back({p.cs_name, "static " + name + "()",
                "returns an enum that was not bound"});
            return;
        } else if (static_param_enum_unregistered<T, I>(std::make_index_sequence<argc>{})) {
            p.skipped.push_back({p.cs_name, "static " + name + "()",
                "takes an enum that was not bound"});
            return;
        } else {
            const std::size_t seen = std::count(p.emitted_statics.begin(),
                                                p.emitted_statics.end(), name);
            const std::string suffix = seen ? "_" + std::to_string(seen) : "";
            const std::string sym = claim_symbol(p, "mb_" + p.symbol + "_s_" + name + suffix);
            const std::string ext_name = "SCall_" + name + suffix;
            p.emitted_statics.push_back(name);

            std::string c_params, cs_ext_params, cs_sig_params;
            std::string call_args, fwd_args, cs_types;
            [&]<std::size_t... Ps>(std::index_sequence<Ps...>) {
                ([&] {
                    using PT = std::remove_cvref_t<
                        typename [:get_static_method_param_type<T, I, Ps>():]>;
                    constexpr Wire pw = wire_of<PT>();
                    constexpr const char* an_id = static_param_ident<T, I, Ps>();
                    const std::string an = arg_name(an_id, Ps);
                    const std::string cn = cpp_arg_name(an_id, Ps);
                    const std::string sep = c_params.empty() ? "" : ", ";
                    c_params += sep + c_type(pw) + " " + cn;
                    cs_ext_params += sep + cs_param_type(pw) + " " + an;
                    cs_sig_params += sep + cs_name_for<PT>(pw) + " " + an;
                    cs_types += (cs_types.empty() ? "" : ",") + cs_name_for<PT>(pw);
                    if (!call_args.empty()) call_args += ", ";
                    if constexpr (pw == Wire::Bool) {
                        fwd_args += std::string(fwd_args.empty() ? "" : ", ") + "(" + cn + " != 0)";
                        call_args += an + " ? 1 : 0";
                    } else if constexpr (pw == Wire::Utf8) {
                        fwd_args += std::string(fwd_args.empty() ? "" : ", ") + "(" + cn + " ? " + cn + " : \"\")";
                        call_args += an + " ?? \"\"";
                    } else {
                        const std::string cast = cpp_cast<PT>(pw);
                        fwd_args += std::string(fwd_args.empty() ? "" : ", ")
                                  + (cast.empty() ? cn : cast + "(" + cn + ")");
                        call_args += cs_cast_in<PT>(pw) + an;
                    }
                }(), ...);
            }(std::make_index_sequence<argc>{});

            const std::string public_sig = "static " + name + "(" + cs_types + ")";
            if (std::count(p.public_sigs.begin(), p.public_sigs.end(), public_sig)) {
                p.skipped.push_back({p.cs_name, "static " + name + "()",
                    "an earlier overload already has the C# signature " + public_sig});
                return;
            }
            p.public_sigs.push_back(public_sig);

            const std::string invoke = p.cpp_name + "::" + name + "(" + fwd_args + ")";
            shim << "MB_EXPORT " << c_type(rw) << " " << sym
                 << "(" << (c_params.empty() ? std::string("void") : c_params) << ") {\n"
                 << "mb_clear_error();\n"
                 << "try {\n";
            if constexpr (rw == Wire::None) {
                shim << invoke << ";\n";
            } else if constexpr (rw == Wire::Utf8) {
                shim << "return mb_dup(" << invoke << ");\n";
            } else if constexpr (rw == Wire::Bool) {
                shim << "return " << invoke << " ? 1 : 0;\n";
            } else {
                shim << "return static_cast<" << c_type(rw) << ">(" << invoke << ");\n";
            }
            shim << "} catch (const std::exception& ex) {\n"
                 << "mb_set_exception(ex);\n"
                 << "} catch (...) {\n"
                 << "mb_set_error(\"" << p.cpp_name << "::" << name
                 << " threw a non-std exception\");\n"
                 << "}\n";
            if constexpr (rw != Wire::None) {
                if constexpr (rw == Wire::Opt) {
                    const OptPlan rop = opt_plan<Ret>();
                    shim << "return " << (rop.is_string ? std::string("nullptr")
                                                        : rop.c_wire + "{}") << ";\n";
                } else {
                    shim << "return " << zero_of(rw) << ";\n";
                }
            }
            shim << "}\n\n";

            externs << "[DllImport(Lib, EntryPoint = \"" << sym
                    << "\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                    << "private static extern " << cs_wire_type(rw) << " "
                    << ext_name << "(" << cs_ext_params << ");\n\n";

            body << "public static " << cs_name_for<Ret>(rw) << " "
                 << claim_cs_name(p, name) << "(" << cs_sig_params << ") {\n";
            const std::string call = ext_name + "(" + call_args + ")";
            if constexpr (rw == Wire::None) {
                body << call << ";\n" << "MbNative.Throw(\"" << name << "\");\n";
            } else if constexpr (rw == Wire::Utf8) {
                body << "var r = " << call << ";\n"
                     << "MbNative.Throw(\"" << name << "\");\n"
                     << "return MbNative.TakeString(r);\n";
            } else if constexpr (rw == Wire::Bool) {
                body << "var r = " << call << ";\n"
                     << "MbNative.Throw(\"" << name << "\");\n"
                     << "return r != 0;\n";
            } else {
                body << "var r = " << call << ";\n"
                     << "MbNative.Throw(\"" << name << "\");\n"
                     << "return " << cs_cast_out<Ret>(rw) << "r;\n";
            }
            body << "}\n\n";
        }
    }

    template<typename T, std::size_t I, std::size_t... Ps>
    static consteval bool static_params_ok(std::index_sequence<Ps...>) {
        return (simple_param_wire_ok(wire_of<std::remove_cvref_t<
                    typename [:get_static_method_param_type<T, I, Ps>():]>>()) && ... && true);
    }

    // ---- methods ------------------------------------------------------

    template<typename T>
    void emit_methods(ClassPlan& p, std::ostringstream& shim,
                      std::ostringstream& externs, std::ostringstream& body) {
        constexpr std::size_t n = get_member_function_count<T>();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (emit_method<T, Is>(p, shim, externs, body), ...);
        }(std::make_index_sequence<n>{});
    }

    template<typename T, std::size_t I>
    void emit_method(ClassPlan& p, std::ostringstream& shim,
                     std::ostringstream& externs, std::ostringstream& body) {
        constexpr auto func = get_member_function<T, I>();
        constexpr const char* owner_id = method_owner<T, I>();
        using Ret = std::remove_cvref_t<typename [:get_method_return_type<T, I>():]>;
        constexpr Wire rw = wire_of<Ret>();
        constexpr std::size_t argc = get_method_param_count<T, I>();
        const std::string name(std::meta::identifier_of(func));

        if constexpr (!ret_wire_ok(rw)) {
            constexpr const char* rname =
                std::meta::display_string_of(get_method_return_type<T, I>()).data();
            p.skipped.push_back({p.cs_name, name + "()",
                std::string("returns ") + rname
                + ", which does not cross the C ABI yet"});
            return;
        } else if constexpr (!params_ok<T, I>(std::make_index_sequence<argc>{})) {
            p.skipped.push_back({p.cs_name, name + "()",
                "a parameter type does not cross the C ABI yet"});
            return;
        } else if (method_has_string_vector_param<T, I>(std::make_index_sequence<argc>{})) {
            p.skipped.push_back({p.cs_name, name + "()",
                "takes a container of strings; a vector of a scalar crosses "
                "inward as a pointer and a length, a vector of strings would "
                "need an array of char* and does not yet"});
            return;
        } else if (object_unregistered<Ret>()) {
            p.skipped.push_back({p.cs_name, name + "()",
                "returns a class this module does not bind"});
            return;
        } else if (enum_unregistered<Ret>()) {
            p.skipped.push_back({p.cs_name, name + "()",
                "returns an enum that was not bound, so it would have crossed "
                "as a bare integer"});
            return;
        } else if (method_param_object_unregistered<T, I>(std::make_index_sequence<argc>{})) {
            p.skipped.push_back({p.cs_name, name + "()",
                "takes a class this module does not bind"});
            return;
        } else if (method_param_enum_unregistered<T, I>(std::make_index_sequence<argc>{})) {
            p.skipped.push_back({p.cs_name, name + "()",
                "takes an enum that was not bound; the shim cannot name the "
                "type to forward it"});
            return;
        } else {
            const char* rct = c_type(rw);

            // C# overloads natively, so an overload set needs nothing special
            // on the managed side -- only the exported symbol has to be
            // unique, and so does the private extern, because two overloads
            // can collapse to one wire signature (foo(bool) and foo(int) are
            // distinct in C# and both int32 on the wire).
            const std::size_t seen = std::count(p.emitted_methods.begin(),
                                                p.emitted_methods.end(), name);
            const std::string suffix = seen ? "_" + std::to_string(seen) : "";
            const std::string sym = claim_symbol(p, "mb_" + p.symbol + "_" + name + suffix);
            const std::string ext_name = "Call_" + name + suffix;
            p.emitted_methods.push_back(name);

            std::string c_params = "void* self";
            std::string cs_ext_params = p.handle + " self";
            std::string cs_sig_params;
            std::string call_args;
            std::string fwd_args;
            // The public C# signature, used to refuse a second overload that
            // would be indistinguishable to a C# caller even though C++ told
            // them apart -- long and long long are both `long` here.
            std::string cs_types;
            // Statements the C# body needs before the call, for an argument
            // that has to be materialised as an array first.
            std::string vec_prologue;
            [&]<std::size_t... Ps>(std::index_sequence<Ps...>) {
                ([&] {
                    constexpr Wire pw = wire_of<
                        std::remove_cvref_t<typename [:get_method_param_type<T, I, Ps>():]>>();
                    constexpr const char* an_id = method_param_ident<T, I, Ps>();
                    const std::string an = arg_name(an_id, Ps);
                    const std::string cn = cpp_arg_name(an_id, Ps);
                    using PT = std::remove_cvref_t<
                        typename [:get_method_param_type<T, I, Ps>():]>;
                    if constexpr (pw == Wire::Opt) {
                        const OptPlan pop = opt_plan<PT>();
                        if (pop.is_string) {
                            // A null pointer is already nullopt, so no flag.
                            c_params += std::string(", const char* ") + cn;
                            cs_ext_params += std::string(", [MarshalAs(UnmanagedType.LPUTF8Str)] string ") + an;
                            fwd_args += std::string(fwd_args.empty() ? "" : ", ")
                                      + "(" + cn + " ? " + pop.cpp_opt
                                      + "(std::string(" + cn + ")) : " + pop.cpp_opt + "{})";
                            if (!call_args.empty()) call_args += ", ";
                            call_args += an;
                        } else {
                            c_params += std::string(", int32_t ") + cn + "_has, "
                                      + pop.c_wire + " " + cn;
                            cs_ext_params += std::string(", int ") + an + "_has, "
                                           + pop.cs_wire + " " + an;
                            fwd_args += std::string(fwd_args.empty() ? "" : ", ")
                                      + "(" + cn + "_has ? " + pop.cpp_opt
                                      + "(static_cast<" + pop.cpp_inner + ">(" + cn
                                      + ")) : " + pop.cpp_opt + "{})";
                            if (!call_args.empty()) call_args += ", ";
                            call_args += an + ".HasValue ? 1 : 0, " + an
                                       + ".HasValue ? (" + pop.cs_wire + ")" + an
                                       + ".Value : default(" + pop.cs_wire + ")";
                        }
                        if (!cs_sig_params.empty()) cs_sig_params += ", ";
                        cs_sig_params += pop.cs_surface + " " + an;
                        cs_types += (cs_types.empty() ? "" : ",") + pop.cs_surface;
                        return;
                    }
                    if constexpr (pw == Wire::Vector) {
                        const VectorPlan* pv = vector_plan<PT>();
                        // Two wire parameters for one C# argument: the shim
                        // builds the vector from the range, so nothing is
                        // allocated on this side and nothing has to be freed.
                        c_params += std::string(", const ") + pv->cpp_elem + "* "
                                  + cn + "_data, int64_t " + cn + "_n";
                        cs_ext_params += std::string(", [In] ") + pv->cs_elem + "[] "
                                       + an + "_data, long " + an + "_n";
                        if (!cs_sig_params.empty()) cs_sig_params += ", ";
                        cs_sig_params += "IReadOnlyList<" + pv->cs_elem + "> " + an;
                        cs_types += (cs_types.empty() ? "" : ",")
                                  + ("IReadOnlyList<" + pv->cs_elem + ">");
                        fwd_args += std::string(fwd_args.empty() ? "" : ", ")
                                  + pv->cpp_vec + "(" + cn + "_data, " + cn
                                  + "_data + " + cn + "_n)";
                        if (!call_args.empty()) call_args += ", ";
                        call_args += an + "_arr, " + an + "_arr.LongLength";
                        // Materialised once before the call. Our own vector
                        // view implements IReadOnlyList<T>, so passing another
                        // object's member straight through works.
                        vec_prologue += "var " + an + "_arr = " + an + " as "
                                      + pv->cs_elem + "[];\n"
                                      + "if (" + an + "_arr == null) {\n"
                                      + an + "_arr = new " + pv->cs_elem + "["
                                      + an + ".Count];\n"
                                      + "for (int i = 0; i < " + an + "_arr.Length; i++) "
                                      + an + "_arr[i] = " + an + "[i];\n"
                                      + "}\n";
                        return;
                    }
                    c_params += std::string(", ") + c_type(pw) + " " + cn;
                    cs_ext_params += std::string(", ") + cs_param_type(pw) + " " + an;
                    if (!cs_sig_params.empty()) cs_sig_params += ", ";
                    cs_sig_params += cs_name_for<PT>(pw) + " " + an;
                    cs_types += (cs_types.empty() ? "" : ",") + cs_name_for<PT>(pw);
                    if (!call_args.empty()) call_args += ", ";
                    if constexpr (pw == Wire::Bool) {
                        fwd_args += std::string(fwd_args.empty() ? "" : ", ") + "(" + cn + " != 0)";
                        call_args += an + " ? 1 : 0";
                    } else if constexpr (pw == Wire::Utf8) {
                        fwd_args += std::string(fwd_args.empty() ? "" : ", ") + "(" + cn + " ? " + cn + " : \"\")";
                        call_args += an + " ?? \"\"";   // LPUTF8Str marshals it
                    } else if constexpr (pw == Wire::Object) {
                        const ClassRef* pr = class_ref<PT>();
                        fwd_args += std::string(fwd_args.empty() ? "" : ", ")
                                  + "(*static_cast<" + (pr ? pr->cpp_name : "") + "*>(" + cn + "))";
                        // Named rather than IntPtr, so the marshaller holds a
                        // reference for the duration of the call.
                        cs_ext_params = cs_ext_params.substr(
                            0, cs_ext_params.size() - (std::string(cs_param_type(pw)) + " " + an).size())
                            + (pr ? pr->cs_name + "." + pr->handle : std::string("IntPtr")) + " " + an;
                        call_args += "MbNative.NotNull(" + an + ", \"" + an + "\").Borrow()";
                    } else {
                        const std::string cast = cpp_cast<PT>(pw);
                        fwd_args += std::string(fwd_args.empty() ? "" : ", ")
                                  + (cast.empty() ? cn : cast + "(" + cn + ")");
                        call_args += cs_cast_in<PT>(pw) + an;
                    }
                }(), ...);
            }(std::make_index_sequence<argc>{});

            // Checked before anything is emitted: refusing after the shim
            // was written left a live entry point in the library that no
            // generated C# calls, and that C++ overload resolution would
            // answer from the wrong overload.
            const std::string public_sig = name + "(" + cs_types + ")";
            if (std::count(p.public_sigs.begin(), p.public_sigs.end(), public_sig)) {
                p.skipped.push_back({p.cs_name, name + "()",
                    "an earlier overload already has the C# signature "
                    + public_sig + "; C++ distinguishes these parameter types "
                    "and C# does not"});
                return;
            }
            p.public_sigs.push_back(public_sig);

            const std::string invoke = std::string("static_cast<") + p.cpp_name
                                     + "*>(self)->" + qualified(owner_id, name)
                                     + "(" + fwd_args + ")";

            // An optional return needs the inner type on the wire, plus a
            // has-value out-param unless a null pointer can carry it. c_type
            // has no spelling for Wire::Opt, and streaming its nullptr is what
            // segfaulted the generator before this existed.
            std::string ret_c = rct ? std::string(rct) : std::string();
            std::string ret_cs = cs_wire_type(rw) ? std::string(cs_wire_type(rw))
                                                  : std::string();
            std::string opt_out_c, opt_out_cs, opt_out_call;
            if constexpr (rw == Wire::Opt) {
                const OptPlan rop = opt_plan<Ret>();
                ret_c = rop.c_wire;
                ret_cs = rop.cs_wire;
                if (!rop.is_string) {
                    opt_out_c = std::string(", int32_t* has");
                    opt_out_cs = ", out int has";
                    opt_out_call = ", out _has";
                }
            }

            shim << "MB_EXPORT " << ret_c << " " << sym << "("
                 << c_params << opt_out_c << ") {\n"
                 << "mb_clear_error();\n"
                 << "try {\n";
            if constexpr (rw == Wire::None) {
                shim << invoke << ";\n";
            } else if constexpr (rw == Wire::Utf8) {
                shim << "return mb_dup(" << invoke << ");\n";
            } else if constexpr (rw == Wire::Bool) {
                shim << "return " << invoke << " ? 1 : 0;\n";
            } else if constexpr (rw == Wire::Opt) {
                const OptPlan rop = opt_plan<Ret>();
                if (rop.is_string) {
                    shim << "{ auto o = " << invoke << ";\n"
                         << "return o.has_value() ? mb_dup(*o) : nullptr; }\n";
                } else {
                    shim << "{ auto o = " << invoke << ";\n"
                         << "*has = o.has_value() ? 1 : 0;\n"
                         << "return o.has_value() ? static_cast<" << rop.c_wire
                         << ">(*o) : " << rop.c_wire << "{}; }\n";
                }
            } else if constexpr (rw == Wire::Vector) {
                // Same reasoning as a returned object: the vector is a value,
                // so the caller gets a heap copy it owns and frees.
                shim << "return static_cast<void*>(new "
                     << std::string(vector_plan<Ret>() ? vector_plan<Ret>()->cpp_vec : "")
                     << "(" << invoke << "));\n";
            } else if constexpr (rw == Wire::Map) {
                shim << "return static_cast<void*>(new "
                     << std::string(map_plan<Ret>() ? map_plan<Ret>()->cpp_map : "")
                     << "(" << invoke << "));\n";
            } else if constexpr (rw == Wire::Object) {
                // Returned by value, so the only correct thing is a copy the
                // caller owns. A pointer to the temporary would dangle the
                // moment this function returned.
                shim << "return static_cast<void*>(new "
                     << std::string(class_ref<Ret>() ? class_ref<Ret>()->cpp_name : "")
                     << "(" << invoke << "));\n";
            } else {
                shim << "return static_cast<" << rct << ">(" << invoke << ");\n";
            }
            shim << "} catch (const std::exception& ex) {\n"
                 << "mb_set_exception(ex);\n"
                 << "} catch (...) {\n"
                 << "mb_set_error(\"" << p.cpp_name << "::" << name
                 << " threw a non-std exception\");\n"
                 << "}\n";
            if constexpr (rw != Wire::None) {
                shim << "return " << zero_of(rw) << ";\n";
            }
            shim << "}\n\n";

            externs << "[DllImport(Lib, EntryPoint = \"" << sym
                    << "\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
                    << "private static extern " << ret_cs << " "
                    << ext_name << "(" << cs_ext_params << opt_out_cs << ");\n\n";

            body << "public " << cs_name_for<Ret>(rw) << " " << claim_cs_name(p, name)
                 << "(" << cs_sig_params << ") {\n"
                 << vec_prologue;
            const std::string call = ext_name + "(Check()"
                                   + (call_args.empty() ? "" : ", " + call_args)
                                   + opt_out_call + ")";
            if constexpr (rw == Wire::None) {
                body << call << ";\n"
                     << "MbNative.Throw(\"" << name << "\");\n";
            } else if constexpr (rw == Wire::Utf8) {
                body << "var r = " << call << ";\n"
                     << "MbNative.Throw(\"" << name << "\");\n"
                     << "return MbNative.TakeString(r);\n";
            } else if constexpr (rw == Wire::Bool) {
                body << "var r = " << call << ";\n"
                     << "MbNative.Throw(\"" << name << "\");\n"
                     << "return r != 0;\n";
            } else if constexpr (rw == Wire::Opt) {
                const OptPlan rop = opt_plan<Ret>();
                if (!rop.is_string) body << "int _has;\n";
                body << "var r = " << call << ";\n"
                     << "MbNative.Throw(\"" << name << "\");\n";
                if (rop.is_string) {
                    body << "return r == IntPtr.Zero ? null : MbNative.TakeString(r);\n";
                } else {
                    const std::string bare =
                        rop.cs_surface.substr(0, rop.cs_surface.size() - 1);
                    body << "return _has != 0 ? (" << rop.cs_surface << ")("
                         << bare << ")r : null;\n";
                }
            } else if constexpr (rw == Wire::Vector) {
                body << "var r = " << call << ";\n"
                     << "MbNative.Throw(\"" << name << "\");\n"
                     << "if (r == IntPtr.Zero) throw MbNative.LastError(\"" << name << "\");\n"
                     << "return new " << vector_plan<Ret>()->cs_name << "(r, true, null);\n";
            } else if constexpr (rw == Wire::Map) {
                body << "var r = " << call << ";\n"
                     << "MbNative.Throw(\"" << name << "\");\n"
                     << "if (r == IntPtr.Zero) throw MbNative.LastError(\"" << name << "\");\n"
                     << "return new " << map_plan<Ret>()->cs_name << "(r, true, null);\n";
            } else if constexpr (rw == Wire::Object) {
                body << "var r = " << call << ";\n"
                     << "MbNative.Throw(\"" << name << "\");\n"
                     << "if (r == IntPtr.Zero) throw MbNative.LastError(\"" << name << "\");\n"
                     << "return new " << cs_name_for<Ret>(rw) << "(r, true, null);\n";
            } else {
                body << "var r = " << call << ";\n"
                     << "MbNative.Throw(\"" << name << "\");\n"
                     << "return " << cs_cast_out<Ret>(rw) << "r;\n";
            }
            body << "}\n\n";
        }
    }

    // Only the string-element case is still refused. is_wire_vector already
    // kept out vector<bool> and vectors of unbound classes.
    template<typename V>
    static consteval bool is_string_vector() {
        using U = std::remove_cvref_t<V>;
        if constexpr (!is_wire_vector<U>()) return false;
        else return wire_of<std::remove_cvref_t<typename vector_element<U>::type>>()
                  == Wire::Utf8;
    }

    template<typename T, std::size_t I, std::size_t... Ps>
    static consteval bool method_has_string_vector_param(std::index_sequence<Ps...>) {
        return (is_string_vector<std::remove_cvref_t<
                    typename [:get_method_param_type<T, I, Ps>():]>>()
                || ... || false);
    }

    template<typename T, std::size_t I, std::size_t... Ps>
    bool method_param_object_unregistered(std::index_sequence<Ps...>) const {
        bool bad = false;
        ((bad = bad || object_unregistered<std::remove_cvref_t<
              typename [:get_method_param_type<T, I, Ps>():]>>()), ...);
        return bad;
    }

    template<typename T, std::size_t I, std::size_t... Ps>
    bool method_param_enum_unregistered(std::index_sequence<Ps...>) const {
        bool bad = false;
        ((bad = bad || enum_unregistered<std::remove_cvref_t<
              typename [:get_method_param_type<T, I, Ps>():]>>()), ...);
        return bad;
    }

    template<typename T, std::size_t I, std::size_t... Ps>
    bool static_param_enum_unregistered(std::index_sequence<Ps...>) const {
        bool bad = false;
        ((bad = bad || enum_unregistered<std::remove_cvref_t<
              typename [:get_static_method_param_type<T, I, Ps>():]>>()), ...);
        return bad;
    }

    template<typename T, std::size_t I, std::size_t... Ps>
    static consteval bool params_ok(std::index_sequence<Ps...>) {
        return (param_wire_ok(wire_of<std::remove_cvref_t<
                    typename [:get_method_param_type<T, I, Ps>():]>>()) && ... && true);
    }

    static constexpr const char* zero_of(Wire w) {
        switch (w) {
            case Wire::Utf8: return "nullptr";
            case Wire::Object: return "nullptr";
            case Wire::Vector: return "nullptr";
            case Wire::Array:  return "nullptr";
            case Wire::Map:    return "nullptr";
            case Wire::F32:
            case Wire::F64:  return "0.0";
            default:         return "0";
        }
    }

    // ---- files --------------------------------------------------------

    std::string shim_source() const {
        std::ostringstream o;
        o << "// Generated by mirror_bridge from C++26 reflection. Do not edit.\n"
          << "//\n"
          << "// This is the flat C ABI the C# side calls through P/Invoke. It is a\n"
          << "// separate translation unit from the reflection pass that wrote it, so it\n"
          << "// compiles with an ordinary compiler and no reflection support.\n\n"
          << "#include <array>\n#include <cstdint>\n#include <cstdlib>\n"
          << "#include <cstring>\n#include <exception>\n#include <memory>\n"
          << "#include <new>\n#include <stdexcept>\n#include <string>\n"
          << "#include <typeinfo>\n#include <vector>\n\n";
        for (const auto& h : headers_) o << "#include \"" << h << "\"\n";
        o << "\n"
          << "#if defined(_WIN32)\n"
          << "#  define MB_EXPORT extern \"C\" __declspec(dllexport)\n"
          << "#else\n"
          << "#  define MB_EXPORT extern \"C\" __attribute__((visibility(\"default\")))\n"
          << "#endif\n\n"
          << "// A C++ exception crossing extern \"C\" is undefined behaviour, so every\n"
          << "// entry point below catches and records instead. The managed side reads\n"
          << "// this after each call that can throw and raises a real exception, which\n"
          << "// keeps the failure visible rather than turning it into a zero.\n"
          << "namespace {\n"
          << "thread_local std::string g_error;\n"
          << "}\n\n"
          << "// Which C++ exception it was, so the managed side can raise a type a\n"
          << "// caller can catch rather than one InvalidOperationException for\n"
          << "// everything. The ladder mirrors the Python backend's.\n"
          << "enum MbErrKind : int32_t {\n"
          << "    MB_ERR_NONE = 0, MB_ERR_OTHER = 1, MB_ERR_INVALID_ARG = 2,\n"
          << "    MB_ERR_OUT_OF_RANGE = 3, MB_ERR_OVERFLOW = 4, MB_ERR_DOMAIN = 5,\n"
          << "    MB_ERR_LENGTH = 6, MB_ERR_LOGIC = 7, MB_ERR_RUNTIME = 8,\n"
          << "    MB_ERR_BAD_ALLOC = 9, MB_ERR_BAD_CAST = 10\n"
          << "};\n\n"
          << "namespace { thread_local int32_t g_error_kind = MB_ERR_NONE; }\n\n"
          << "static void mb_set_error(const char* what, int32_t kind = MB_ERR_OTHER) {\n"
          << "    try { g_error = what ? what : \"unknown error\"; } catch (...) {}\n"
          << "    g_error_kind = kind;\n"
          << "}\n\n"
          << "static void mb_set_exception(const std::exception& ex) {\n"
          << "    int32_t kind = MB_ERR_OTHER;\n"
          << "    if (dynamic_cast<const std::invalid_argument*>(&ex))      kind = MB_ERR_INVALID_ARG;\n"
          << "    else if (dynamic_cast<const std::out_of_range*>(&ex))     kind = MB_ERR_OUT_OF_RANGE;\n"
          << "    else if (dynamic_cast<const std::overflow_error*>(&ex))   kind = MB_ERR_OVERFLOW;\n"
          << "    else if (dynamic_cast<const std::domain_error*>(&ex))     kind = MB_ERR_DOMAIN;\n"
          << "    else if (dynamic_cast<const std::length_error*>(&ex))     kind = MB_ERR_LENGTH;\n"
          << "    else if (dynamic_cast<const std::bad_alloc*>(&ex))        kind = MB_ERR_BAD_ALLOC;\n"
          << "    else if (dynamic_cast<const std::bad_cast*>(&ex))         kind = MB_ERR_BAD_CAST;\n"
          << "    else if (dynamic_cast<const std::logic_error*>(&ex))      kind = MB_ERR_LOGIC;\n"
          << "    else if (dynamic_cast<const std::runtime_error*>(&ex))    kind = MB_ERR_RUNTIME;\n"
          << "    mb_set_error(ex.what(), kind);\n"
          << "}\n\n"
          << "MB_EXPORT int32_t mb_last_error_kind(void) { return g_error_kind; }\n\n"
          << "// The address of this thread's error slot, so the managed side can\n"
          << "// test for a pending error with a memory read instead of a second\n"
          << "// P/Invoke transition. Without it every generated call paid for two\n"
          << "// transitions -- the call itself and the check afterwards -- which on\n"
          << "// a trivial getter is 100% overhead.\n"
          << "MB_EXPORT int32_t* mb_error_slot(void) { return &g_error_kind; }\n\n"
          << "MB_EXPORT const char* mb_last_error(void) {\n"
          << "    return g_error.empty() ? nullptr : g_error.c_str();\n"
          << "}\n\n"
          << "MB_EXPORT void mb_clear_error(void) {\n"
          << "    g_error.clear();\n"
          << "    g_error_kind = MB_ERR_NONE;\n"
          << "}\n\n"
          << "// Every entry point that can record an error clears first, so what\n"
          << "// mb_last_error returns belongs to the call that just returned. Without\n"
          << "// this, an error left by an unchecked call -- a string property accessor,\n"
          << "// which has no natural place to raise -- surfaced as the next method's\n"
          << "// failure instead.\n"
          << "// KNOWN LIMIT: the wire is a NUL-terminated const char*, so a\n"
          << "// std::string containing an embedded NUL is truncated at it in both\n"
          << "// directions. Lua and JavaScript carry the length and do not. It\n"
          << "// matters for callers using std::string as a byte buffer; carrying\n"
          << "// the length here is the fix.\n"
          << "// The managed caller owns every string it receives and frees it here.\n"
          << "// Returning a pointer into the live std::string would be one allocation\n"
          << "// cheaper and would dangle as soon as anything mutated the object.\n"
          << "static const char* mb_dup(const std::string& s) {\n"
          << "    char* out = static_cast<char*>(std::malloc(s.size() + 1));\n"
          << "    // Without recording this, a failed allocation reaches the\n"
          << "    // managed side as a successful null string instead of an\n"
          << "    // error, which is the one case that would make a non-null\n"
          << "    // string annotation a lie.\n"
          << "    if (!out) { mb_set_error(\"out of memory copying a string\"); return nullptr; }\n"
          << "    std::memcpy(out, s.c_str(), s.size() + 1);\n"
          << "    return out;\n"
          << "}\n\n"
          << "MB_EXPORT void mb_free_string(const char* p) {\n"
          << "    std::free(const_cast<char*>(p));\n"
          << "}\n\n";
        for (const auto& v : vectors_) {
            o << "// ---- std::vector<" << v.cpp_elem << "> element access ----\n\n"
              << reindent_braced(v.shim);
        }
        for (const auto& m : maps_) {
            o << "// ---- " << m.cpp_map << " access ----\n\n"
              << reindent_braced(m.shim);
        }
        for (const auto& c : classes_) {
            o << "// ---- " << c.cpp_name << " ----\n\n"
              << reindent_braced(c.shim);
        }
        return o.str();
    }

    std::string cs_source() const {
        std::ostringstream o;
        o << "// Generated by mirror_bridge from C++26 reflection. Do not edit.\n"
          << "//\n"
          << "// Every name here came from the C++ declaration it binds, so renaming a\n"
          << "// member in C++ renames the property on the next build. There is no list\n"
          << "// to keep in step because there is no second list.\n\n"
          << "using System;\n"
          << "using System.Collections;\n"
          << "using System.Collections.Generic;\n"
          << "using System.Runtime.InteropServices;\n\n"
          << "namespace " << namespace_ << " {\n\n"
          << "// A borrowed view points inside an object someone else owns. Holding a\n"
          << "// reference to that owner stops the GC collecting it, but says nothing\n"
          << "// about Dispose: a disposed owner has already deleted the C++ object, and\n"
          << "// the view's pointer is then dangling. Python's equivalent views need only\n"
          << "// the reference because Python has no explicit dispose; C# does, so the\n"
          << "// view has to be able to ask.\n"
          << "//\n"
          << "// Implemented explicitly, so it stays off the public surface, and checked\n"
          << "// up the whole chain: a view of a view is only as alive as its root.\n"
          << "internal interface IMbOwner {\n"
          << "    bool MbAlive { get; }\n"
          << "    SafeHandle MbHandle { get; }\n"
          << "}\n\n"
          << "internal static class MbNative {\n"
          << "    internal const string Lib = \"" << module_ << "\";\n\n"
          << "    [DllImport(Lib, EntryPoint = \"mb_last_error\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "    private static extern IntPtr LastErrorPtr();\n\n"
          << "    [DllImport(Lib, EntryPoint = \"mb_last_error_kind\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "    private static extern int LastErrorKind();\n\n"
          << "    [DllImport(Lib, EntryPoint = \"mb_clear_error\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "    private static extern void ClearError();\n\n"
          << "    [DllImport(Lib, EntryPoint = \"mb_free_string\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "    private static extern void FreeString(IntPtr p);\n\n"
          << "    /// <summary>Copies a native UTF-8 string and frees the native copy.</summary>\n"
          << "    /// <remarks>A null pointer only reaches here when no error was\n"
          << "    /// recorded, which the shim treats as an empty string rather than a\n"
          << "    /// failure; an allocation that fails records an error and is raised\n"
          << "    /// before this is called.</remarks>\n"
          << "    internal static string TakeString(IntPtr p) {\n"
          << "        if (p == IntPtr.Zero) return string.Empty;\n"
          << "        try { return Marshal.PtrToStringUTF8(p) ?? string.Empty; }\n"
          << "        finally { FreeString(p); }\n"
          << "    }\n\n"
          << "    /// <summary>The exception a C++ throw becomes.</summary>\n"
          << "    /// <remarks>The kind comes from a dynamic_cast ladder in the shim, so\n"
          << "    /// a caller can catch ArgumentException for std::invalid_argument\n"
          << "    /// instead of one InvalidOperationException for everything.</remarks>\n"
          << "    internal static Exception LastError(string where) {\n"
          << "        var msg = Marshal.PtrToStringUTF8(LastErrorPtr());\n"
          << "        var kind = LastErrorKind();\n"
          << "        ClearError();\n"
          << "        var text = msg == null ? where + \" failed in C++\" : where + \": \" + msg;\n"
          << "        switch (kind) {\n"
          << "            case 2:  return new ArgumentException(text);\n"
          << "            case 3:  return new ArgumentOutOfRangeException(null, text);\n"
          << "            case 4:  return new OverflowException(text);\n"
          << "            case 5:  return new ArgumentException(text);\n"
          << "            case 6:  return new ArgumentException(text);\n"
          << "            case 9:  return new OutOfMemoryException(text);\n"
          << "            case 10: return new InvalidCastException(text);\n"
          << "            default: return new InvalidOperationException(text);\n"
          << "        }\n"
          << "    }\n\n"
          << "    [DllImport(Lib, EntryPoint = \"mb_error_slot\", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]\n"
          << "    private static extern IntPtr ErrorSlot();\n\n"
          << "    // The slot is thread-local on the native side, so this is cached\n"
          << "    // per managed thread. One transition per thread for the address,\n"
          << "    // then a plain memory read per call.\n"
          << "    [ThreadStatic] private static IntPtr _slot;\n\n"
          << "    /// <summary>Names the parameter when a class argument is null, instead\n"
          << "    /// of letting it dereference inside the generated method.</summary>\n"
          << "    internal static T NotNull<T>(T value, string name) where T : class {\n"
          << "        if (value == null) throw new ArgumentNullException(name);\n"
          << "        return value;\n"
          << "    }\n\n"
          << "    /// <summary>Raises whatever the last native call recorded, if any.</summary>\n"
          << "    /// <remarks>Reads the error slot directly rather than calling back\n"
          << "    /// into the library: a second P/Invoke per call would double the\n"
          << "    /// cost of every trivial method.</remarks>\n"
          << "    internal static void Throw(string where) {\n"
          << "        var s = _slot;\n"
          << "        if (s == IntPtr.Zero) {\n"
          << "            s = ErrorSlot();\n"
          << "            _slot = s;\n"
          << "        }\n"
          << "        if (Marshal.ReadInt32(s) != 0) throw LastError(where);\n"
          << "    }\n"
          << "}\n\n";
        for (const auto& v : vectors_) {
            o << reindent_braced(vector_cs(v));
        }
        for (const auto& m : maps_) {
            o << reindent_braced(map_cs(m));
        }
        // Enums first, so a class referring to one is reading a type already
        // declared above it. C# does not require that, but a reader does.
        for (const auto& e : enums_) {
            std::ostringstream en;
            en << "/// <summary>Binds the C++ enumeration <c>" << e.cpp_name
               << "</c>.</summary>\n"
               << "public enum " << e.cs_name << " : " << e.underlying << " {\n";
            for (const auto& m : e.members) {
                en << m.first << " = " << m.second << ",\n";
            }
            en << "}\n\n";
            o << reindent_braced(en.str());
        }
        for (const auto& c : classes_) {
            std::ostringstream cls;
            cls << "/// <summary>Binds the C++ class <c>" << c.cpp_name << "</c>.</summary>\n"
                << "public sealed class " << c.cs_name << " : IDisposable, IMbOwner {\n"
                << "private const string Lib = MbNative.Lib;\n\n"
                << c.externs
                << c.body;
            if (!c.renamed.empty()) {
                cls << "// Renamed, because the C++ name is taken by the generated surface:\n";
                for (const auto& r : c.renamed) {
                    cls << "//   " << r.first << " -> " << r.second << "\n";
                }
                cls << "\n";
            }
            if (!c.skipped.empty()) {
                cls << "// Left out of this binding, and why:\n";
                for (const auto& s : c.skipped) {
                    cls << "//   " << s.name << " -- " << s.reason << "\n";
                }
                cls << "\n";
            }
            cls << "}\n\n";
            o << reindent_braced(cls.str());
        }
        o << "}  // namespace " << namespace_ << "\n";
        return o.str();
    }
};

}  // namespace csharp
}  // namespace mirror_bridge
