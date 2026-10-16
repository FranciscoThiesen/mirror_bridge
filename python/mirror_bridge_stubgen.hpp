#pragma once

// ============================================================================
// Mirror Bridge - Python Type Stub (.pyi) Generation
// ============================================================================
//
// Generates Python type stubs for IDE autocompletion and type checking.
// Metadata is collected during bind_class() and can be exported to .pyi files.
//
// Usage:
//   MIRROR_BRIDGE_MODULE(mymodule,
//       mirror_bridge::bind_class<MyClass>(m, "MyClass");
//       mirror_bridge::bind_class<Other>(m, "Other");
//
//       // Generate stubs
//       mirror_bridge::stubgen::write_stubs("mymodule.pyi");
//   )
//
// ============================================================================

#include <meta>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <fstream>
#include <type_traits>
#include <typeinfo>
#include <cxxabi.h>

namespace mirror_bridge {

// ============================================================================
// What a numeric vector actually becomes on the way out
// ============================================================================
//
// Lives here, above stubgen, because two things have to agree about it and
// they are in different headers: to_python, which builds the array.array,
// and type_hint, which has to say so in the .pyi. The stub said list[float]
// for years while the runtime handed back array('d'), which a type checker
// then approved every misuse of.
//
// There are two more encodings of a nearly identical mapping in
// mirror_bridge_python.hpp -- get_buffer_format, for the buffer protocol,
// and buffer_format_matches, for the way back in. They cover different
// element sets for different reasons. Do not add a fourth.
//
// tests/bulk_transfer/test_bulk.py holds the Python-side table of which
// accessor gets which typecode, so a new element type added here wants a row
// there as well.

template<typename T>
struct is_std_vector : std::false_type {};
template<typename T, typename A>
struct is_std_vector<std::vector<T, A>> : std::true_type {};

// The array module's typecode for an element type, or nullptr where it has
// none. An element type absent from this table is not a bug: it falls
// through to the per-element list path, which is correct and slower. long
// double, int8_t and the character types are all in that position.
template<typename T>
constexpr const char* array_typecode_for() {
    if constexpr (std::is_same_v<T, float>) return "f";
    else if constexpr (std::is_same_v<T, double>) return "d";
    else if constexpr (std::is_same_v<T, std::int32_t> ||
                       (std::is_same_v<T, int> && sizeof(int) == 4)) return "i";
    else if constexpr (std::is_same_v<T, std::int64_t> ||
                       std::is_same_v<T, long long>) return "q";
    else if constexpr (std::is_same_v<T, std::uint32_t> ||
                       (std::is_same_v<T, unsigned int> && sizeof(unsigned int) == 4)) return "I";
    else if constexpr (std::is_same_v<T, std::uint64_t> ||
                       std::is_same_v<T, unsigned long long>) return "Q";
    else if constexpr (std::is_same_v<T, std::int16_t> || std::is_same_v<T, short>) return "h";
    else if constexpr (std::is_same_v<T, std::uint16_t> ||
                       std::is_same_v<T, unsigned short>) return "H";
    else return nullptr;
}

// Whether a container crosses as an array.array rather than as a list.
//
// A class template rather than a variable template on purpose: `&&` in a
// variable template's initialiser short-circuits evaluation, not
// substitution, so naming C::value_type there is a hard error for every C
// that has no value_type rather than a false.
template<typename C, typename = void>
struct crosses_as_array : std::false_type {};

template<typename C>
struct crosses_as_array<C, std::void_t<typename C::value_type>>
    : std::bool_constant<
          is_std_vector<C>::value &&
          std::is_arithmetic_v<typename C::value_type> &&
          !std::is_same_v<typename C::value_type, unsigned char> &&
          !std::is_same_v<typename C::value_type, bool> &&
          array_typecode_for<typename C::value_type>() != nullptr> {};

template<typename C>
inline constexpr bool crosses_as_array_v = crosses_as_array<std::remove_cvref_t<C>>::value;

// bytes has exactly one producer, the non-template to_python(const
// std::vector<unsigned char>&). std::vector<char>, std::vector<std::byte>
// and std::array<unsigned char, N> all come back as lists of int, whatever
// the stub used to claim.
template<typename C>
inline constexpr bool crosses_as_bytes_v =
    std::is_same_v<std::remove_cvref_t<C>, std::vector<unsigned char>>;

// The enumerators of E, reached the way the rest of this project reaches a
// reflection range: define_static_array plus an index sequence. A pack used
// only inside a splice is not expandable on GCC, which is why there is no
// expand() anywhere in this codebase.
//
// Here rather than beside bind_enum for the same reason as the array
// decision above: this header is included first, and the stub generator
// needs these to write the member list.
// A C++ name that is a Python keyword cannot be written down in Python
// source. Here rather than inside the stub registry because bind_enum needs
// the same answer at runtime and two keyword lists would be one list too
// many.
inline bool is_python_keyword(const std::string& s) {
    static const std::set<std::string> kw = {
        "False", "None", "True", "and", "as", "assert", "async", "await",
        "break", "class", "continue", "def", "del", "elif", "else",
        "except", "finally", "for", "from", "global", "if", "import",
        "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise",
        "return", "try", "while", "with", "yield"
    };
    return kw.count(s) != 0;
}

namespace detail {

template<typename E>
consteval auto enumerators_of_array() {
    return std::define_static_array(std::meta::enumerators_of(^^E));
}

template<typename E, std::size_t I>
consteval std::string_view enumerator_name() {
    return std::meta::identifier_of(enumerators_of_array<E>()[I]);
}

template<typename E, std::size_t I>
consteval long long enumerator_value() {
    return static_cast<long long>([:enumerators_of_array<E>()[I]:]);
}

}  // namespace detail

// The (python name, value) pairs for E, with every name writable in Python
// source.
//
// A keyword gets a trailing underscore, which is the convention Python's own
// style guide names for exactly this ("single_trailing_underscore_: used by
// convention to avoid conflicts with Python keyword"). `enum class
// Permission { None, Read }` is the ordinary case and it would otherwise be
// reachable only through Permission["None"]: `Permission.None` is a syntax
// error, not an AttributeError, so there is no spelling for a caller to
// find. More underscores are added until the name is free, for the enum
// that declares both `None` and `None_`.
template<typename E>
inline std::vector<std::pair<std::string, long long>> enum_members() {
    constexpr std::size_t count = detail::enumerators_of_array<E>().size();
    std::vector<std::pair<std::string, long long>> out;
    out.reserve(count);

    std::set<std::string> taken;
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (taken.insert(std::string(detail::enumerator_name<E, Is>())), ...);
    }(std::make_index_sequence<count>{});

    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        auto one = [&](std::string name, long long value) {
            if (is_python_keyword(name)) {
                do { name += "_"; } while (taken.count(name) != 0);
                taken.insert(name);
            }
            out.emplace_back(std::move(name), value);
        };
        (one(std::string(detail::enumerator_name<E, Is>()),
             detail::enumerator_value<E, Is>()), ...);
    }(std::make_index_sequence<count>{});
    return out;
}

namespace stubgen {

// ============================================================================
// Stub Metadata Storage
// ============================================================================

struct ParameterInfo {
    std::string name;
    std::string type_hint;
    // The parameter has a C++ default argument, so the call is legal without
    // it. Rendered as `= ...`: a stub that demands every parameter makes a
    // type checker reject calls the module accepts, which for SQLite::Database
    // means rejecting `Database(path)` -- the first line anyone writes.
    bool has_default = false;
};

struct MethodInfo {
    std::string name;
    std::vector<ParameterInfo> parameters;
    std::string return_type;
    bool is_static = false;
    // Whether the runtime entry point refuses keywords. Inferring this from
    // is_static was right until a generated batch sibling arrived: it is an
    // instance method registered METH_O, which takes exactly one positional
    // argument and no keyword at all.
    bool positional_only = false;
};

struct PropertyInfo {
    std::string name;
    std::string type_hint;   // what reading it gives
    std::string setter_hint; // what assigning to it takes, where that differs
    bool readonly = false;
};

struct ConstructorInfo {
    std::vector<ParameterInfo> parameters;
};

struct EnumInfo {
    std::string name;
    std::vector<std::pair<std::string, long long>> members;
};

struct ClassInfo {
    std::string name;
    std::vector<ConstructorInfo> constructors;
    std::vector<PropertyInfo> properties;
    std::vector<MethodInfo> methods;
};

// Global stub registry (per module)
class StubRegistry {
public:
    static StubRegistry& instance() {
        static StubRegistry registry;
        return registry;
    }

    void add_class(const ClassInfo& info) {
        classes_[info.name] = info;
    }

    ClassInfo& get_or_create_class(const std::string& name) {
        if (classes_.find(name) == classes_.end()) {
            classes_[name] = ClassInfo{name, {}, {}, {}};
        }
        return classes_[name];
    }

    void add_function(const MethodInfo& info) {
        functions_.push_back(info);
    }

    void add_enum(const EnumInfo& info) {
        enums_[info.name] = info;
    }

    // A bound class is known to the stub by its Python name; type_hint<T>()
    // refers to classes through a typeid placeholder (see type_hint) that
    // generate() resolves against this table, so the hint is right no matter
    // whether the class was bound before or after the method mentioning it.
    void register_type_name(const char* typeid_name, const std::string& py_name) {
        type_names_[typeid_name] = py_name;
    }

    static std::string type_placeholder(const char* typeid_name) {
        return std::string("\x01") + typeid_name + "\x02";
    }

    // An enum is a placeholder too, but with a different answer when it is
    // not bound. A class that was never bound has no Python name and the
    // fallback guesses one from the mangled name; an enum that was never
    // bound genuinely is an int on both sides of the boundary, so saying so
    // is right rather than a guess.
    static std::string enum_placeholder(const char* typeid_name) {
        return std::string("\x01") + typeid_name + "\x03";
    }

    std::string generate(const std::string& module_name) const {
        return resolve_placeholders(generate_raw(module_name));
    }

    bool write_to_file(const std::string& module_name, const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        file << generate(module_name);
        return file.good();
    }

    void clear() {
        classes_.clear();
        enums_.clear();
        functions_.clear();
        type_names_.clear();
    }

private:
    StubRegistry() = default;

    std::string generate_raw(const std::string& module_name) const {
        // The body is rendered first so the import line can be chosen from
        // what it actually contains. An import a module does not use would
        // move every committed baseline for no change in surface.
        std::ostringstream body;
        render_body(body);
        const std::string rendered = body.str();

        std::ostringstream ss;

        // Module docstring
        ss << "\"\"\"Type stubs for " << module_name << " module.\n\n";
        ss << "Auto-generated by Mirror Bridge stubgen.\n";
        ss << "\"\"\"\n\n";

        // Imports. Iterator is added only when a class in this module
        // declares __iter__, so a module with no ranges keeps the stub it
        // had — a committed baseline should move when the surface does and
        // not before.
        ss << "from __future__ import annotations\n";
        // Aliased: the stub drops C++ class names into this same namespace,
        // and a class called `array` would otherwise silently shadow the
        // import and change what every annotation under it means. The
        // unaliased typing names on the next line have the same hazard and
        // predate this.
        if (rendered.find("_array[") != std::string::npos) {
            ss << "from array import array as _array\n";
        }
        if (rendered.find("_IntEnum") != std::string::npos) {
            ss << "from enum import IntEnum as _IntEnum\n";
        }
        ss << "from typing import Any, List, Tuple, Optional, Union, overload";
        if (any_class_iterates()) ss << ", Iterator";
        ss << "\n\n";

        ss << rendered;
        return ss.str();
    }

    void render_body(std::ostringstream& ss) const {
        // Enums first: a class whose member is one names it in an
        // annotation, and `from __future__ import annotations` makes that
        // lazy, but a reader wants the definition above the use anyway.
        for (const auto& [name, info] : enums_) {
            ss << "class " << name << "(_IntEnum):\n";
            if (info.members.empty()) {
                ss << "    pass\n\n";
                continue;
            }
            for (const auto& [member, value] : info.members) {
                ss << "    " << member << " = " << value << "\n";
            }
            ss << "\n";
        }

        // Classes
        for (const auto& [name, info] : classes_) {
            ss << generate_class_stub(info) << "\n";
        }

        // Module-level functions
        for (const auto& func : functions_) {
            if (is_python_keyword(func.name)) {
                ss << omission_note("", "function", func.name);
                continue;
            }
            ss << "def " << func.name << "("
               << render_parameters(func.parameters, false, false)
               << ") -> " << func.return_type << ": ...\n";
        }
    }

    // Replace every "\x01<typeid name>\x02" with the Python name of that class.
    // Unbound classes keep the old heuristic: their own simple name when it
    // looks like a class, Any otherwise; a template specialization that was
    // never bound has no Python name at all.
    std::string resolve_placeholders(std::string text) const {
        std::string::size_type start = 0;
        while ((start = text.find('\x01', start)) != std::string::npos) {
            auto end = text.find_first_of("\x02\x03", start);
            if (end == std::string::npos) break;
            const bool is_enum = text[end] == '\x03';
            std::string typeid_name = text.substr(start + 1, end - start - 1);
            std::string replacement;
            auto it = type_names_.find(typeid_name);
            if (it != type_names_.end()) {
                replacement = it->second;
            } else {
                replacement = is_enum ? "int" : fallback_class_name(typeid_name);
            }
            text.replace(start, end - start + 1, replacement);
            start += replacement.size();
        }
        return text;
    }

    static std::string fallback_class_name(const std::string& typeid_name);

    bool any_class_iterates() const {
        for (const auto& [name, info] : classes_) {
            for (const auto& method : info.methods) {
                if (method.name == "__iter__") return true;
            }
        }
        return false;
    }

    // Python's grammar reserves these, so none of them can appear in a stub as
    // a name. The runtime binding is unaffected: CPython looks members up by
    // string, so a C++ member called `pass` is still reachable through
    // getattr(). It is only the *declaration* that cannot be written down.
    static bool is_python_keyword(const std::string& s) {
        return mirror_bridge::is_python_keyword(s);
    }

    // Render a parameter list, renaming anything that cannot be spelled as a
    // keyword argument and marking it positional-only.
    //
    // A parameter named `lambda` or `self` is still accepted by the runtime
    // through f(**{"lambda": x}), but no Python source can write `lambda=x`,
    // and a stub cannot declare it either. Renaming alone would be a lie,
    // because the invented name is not a keyword the module answers to. PEP
    // 570's `/` says precisely the true thing — these arguments are reachable
    // by position only — which leaves the substituted name as documentation.
    // `/` applies to everything before it, so it goes after the *last*
    // unnameable parameter.
    static std::string render_parameters(const std::vector<ParameterInfo>& params,
                                         bool leading_self, bool accepts_keywords) {
        std::vector<std::string> names(params.size());
        std::set<std::string> taken;
        std::ptrdiff_t positional_only_through = accepts_keywords
            ? -1
            : static_cast<std::ptrdiff_t>(params.size()) - 1;

        if (leading_self) taken.insert("self");

        // Reserve the names the runtime really answers to before inventing
        // any. Doing it in one pass let a substitute claim a later
        // parameter's real name: `f(double lambda, double lambda_)` rendered
        // as `lambda_, lambda__`, and `lambda__` is a keyword the module
        // does not have.
        for (size_t i = 0; i < params.size(); ++i) {
            const std::string& name = params[i].name;
            const bool unnameable =
                name.empty() || is_python_keyword(name) || (leading_self && name == "self");
            if (!unnameable) {
                names[i] = name;
                taken.insert(name);
            }
        }

        for (size_t i = 0; i < params.size(); ++i) {
            if (!names[i].empty()) continue;
            // No keyword reaches this one, whether because it is spelled like
            // a Python keyword, because it collides with the receiver, or
            // because the C++ declaration never named it.
            positional_only_through =
                std::max(positional_only_through, static_cast<std::ptrdiff_t>(i));
            std::string name = params[i].name.empty()
                                   ? ("arg" + std::to_string(i))
                                   : (params[i].name + "_");
            while (taken.count(name)) name += "_";
            taken.insert(name);
            names[i] = name;
        }

        std::ostringstream ss;
        if (leading_self) ss << "self";
        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0 || leading_self) ss << ", ";
            ss << names[i] << ": " << params[i].type_hint;
            // Before the `/`, not after: `x: float = ..., /` parses and
            // `x: float, / = ...` does not. Losing this renders every
            // defaulted parameter as required, which a type checker then
            // enforces against calls the module accepts.
            if (params[i].has_default) ss << " = ...";
            if (static_cast<std::ptrdiff_t>(i) == positional_only_through) ss << ", /";
        }
        return ss.str();
    }

    // A member whose own name is a keyword has no declaration form at all, so
    // the stub records why it is absent instead of emitting something a type
    // checker will reject.
    static std::string omission_note(const std::string& indent,
                                     const std::string& kind,
                                     const std::string& name) {
        return indent + "# " + kind + " \"" + name + "\" omitted: the C++ name is a Python "
               "keyword and\n" + indent + "# cannot be declared in a stub. Reach it with "
               "getattr(obj, \"" + name + "\").\n";
    }

    std::string generate_class_stub(const ClassInfo& info) const {
        std::ostringstream ss;

        if (is_python_keyword(info.name)) {
            return omission_note("", "class", info.name);
        }

        ss << "class " << info.name << ":\n";

        // Constructors
        if (info.constructors.empty()) {
            ss << "    def __init__(self) -> None: ...\n";
        } else if (info.constructors.size() == 1) {
            ss << "    def __init__("
               << render_parameters(info.constructors[0].parameters, true, true)
               << ") -> None: ...\n";
        } else {
            // Multiple constructors - use @overload
            for (const auto& ctor : info.constructors) {
                ss << "    @overload\n";
                ss << "    def __init__("
                   << render_parameters(ctor.parameters, true, true)
                   << ") -> None: ...\n";
            }
        }

        // Properties
        for (const auto& prop : info.properties) {
            if (is_python_keyword(prop.name)) {
                ss << omission_note("    ", "attribute", prop.name);
                continue;
            }
            if (prop.readonly) {
                ss << "    @property\n";
                ss << "    def " << prop.name << "(self) -> " << prop.type_hint << ": ...\n";
            } else if (prop.setter_hint.empty() || prop.setter_hint == prop.type_hint) {
                ss << "    " << prop.name << ": " << prop.type_hint << "\n";
            } else {
                // Reading and assigning genuinely differ here, which a bare
                // annotation cannot say: a std::vector<double> member reads
                // back as an array.array and accepts a list on the way in.
                // Only written out where they differ, so an ordinary member
                // stays an ordinary annotation and `mirror_bridge api` does
                // not report every class in every baseline as changed.
                ss << "    @property\n";
                ss << "    def " << prop.name << "(self) -> " << prop.type_hint << ": ...\n";
                ss << "    @" << prop.name << ".setter\n";
                ss << "    def " << prop.name << "(self, value: " << prop.setter_hint
                   << ") -> None: ...\n";
            }
        }

        // Methods. Overloaded names need @overload or type checkers reject
        // the duplicate definitions.
        std::map<std::string, int> name_counts;
        for (const auto& method : info.methods) {
            ++name_counts[method.name];
        }
        for (const auto& method : info.methods) {
            if (is_python_keyword(method.name)) {
                ss << omission_note("    ", "method", method.name);
                continue;
            }
            if (name_counts[method.name] > 1) {
                ss << "    @overload\n";
            }
            if (method.is_static) {
                ss << "    @staticmethod\n";
                ss << "    def " << method.name << "("
                   << render_parameters(method.parameters, false, false);
            } else {
                ss << "    def " << method.name << "("
                   << render_parameters(method.parameters, true,
                                        !method.positional_only);
            }
            ss << ") -> " << method.return_type << ": ...\n";
        }

        // Empty class needs pass
        if (info.constructors.empty() && info.properties.empty() && info.methods.empty()) {
            ss << "    pass\n";
        }

        return ss.str();
    }

    std::map<std::string, ClassInfo> classes_;
    std::map<std::string, EnumInfo> enums_;
    std::vector<MethodInfo> functions_;
    std::map<std::string, std::string> type_names_;
};

// ============================================================================
// Type Hint Helpers
// ============================================================================

// Demangle C++ type names
inline std::string demangle(const char* mangled) {
    int status = 0;
    char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    if (status == 0 && demangled) {
        std::string result(demangled);
        free(demangled);
        return result;
    }
    return mangled;
}

// Where in a signature a hint is going. Returns and parameters do not have
// the same answer: a std::vector<double> comes back as an array.array and
// goes in as a list, a tuple, an array.array or anything else with the
// sequence protocol. One string cannot be true in both places, and asking it
// to be is how the stub came to promise list[float] for an array('d').
enum class Position { Return, Param };

// Convert C++ type to Python type hint
template<typename T, Position P = Position::Return>
std::string type_hint() {
    using CleanT = std::remove_cvref_t<T>;

    // Primitive types
    if constexpr (std::is_same_v<CleanT, bool>) {
        return "bool";
    } else if constexpr (std::is_same_v<CleanT, char>) {
        // char is arithmetic and signed, so to_python sends it through
        // PyLong_FromLong and from_python reads it with PyLong_AsLong. It
        // crosses as an int in both directions, whatever it looks like in
        // C++, and docs/reference/type-conversion.md has always said so.
        return "int";
    } else if constexpr (std::is_integral_v<CleanT>) {
        return "int";
    } else if constexpr (std::is_enum_v<CleanT>) {
        // std::is_integral is false for enums, so without this an enum fell
        // through to the "it must be a class" path and the stub annotated it
        // with the C++ enum's own name — a name the module never defines,
        // which makes the whole .pyi unusable to a type checker.
        //
        // A module that binds the enum does define a name for it, and an
        // IntEnum member is an int, so naming it is both true and more
        // useful. One that does not still gets "int", because that is
        // exactly what crosses (PyLong_FromLong / PyLong_Check).
        return StubRegistry::enum_placeholder(typeid(CleanT).name());
    } else if constexpr (std::is_floating_point_v<CleanT>) {
        return "float";
    } else if constexpr (std::is_same_v<CleanT, std::string> ||
                         std::is_same_v<CleanT, std::string_view> ||
                         std::is_same_v<CleanT, const char*>) {
        return "str";
    } else if constexpr (std::is_void_v<CleanT>) {
        return "None";
    }

    // Eigen fixed-size vectors/matrices convert to (nested) Python lists.
    // Detected structurally so this header stays Eigen-independent.
    if constexpr (requires { CleanT::RowsAtCompileTime; CleanT::ColsAtCompileTime;
                             typename CleanT::Scalar; }) {
        std::string scalar =
            std::is_floating_point_v<typename CleanT::Scalar> ? "float" : "int";
        if constexpr (CleanT::ColsAtCompileTime == 1 || CleanT::RowsAtCompileTime == 1) {
            return "list[" + scalar + "]";
        } else {
            return "list[list[" + scalar + "]]";
        }
    }

    // Maps -> dict[K, V] (checked before generic containers: maps iterate too)
    if constexpr (requires { typename CleanT::key_type; typename CleanT::mapped_type; }) {
        return "dict[" + type_hint<typename CleanT::key_type, P>() + ", " +
               type_hint<typename CleanT::mapped_type, P>() + "]";
    }

    // Sets -> set[T]
    if constexpr (requires { typename CleanT::key_type; } &&
                  !requires { typename CleanT::mapped_type; }) {
        return "set[" + type_hint<typename CleanT::key_type, P>() + "]";
    }

    // bytes has exactly one producer and one consumer, both non-template
    // overloads taking std::vector<unsigned char>, and the consumer also
    // accepts a list. A std::vector<char>, a std::vector<std::byte> and a
    // std::array<unsigned char, N> all come back as lists of int, which the
    // old test here claimed otherwise about.
    if constexpr (crosses_as_bytes_v<CleanT>) {
        if constexpr (P == Position::Return) {
            return "bytes";
        } else {
            return "bytes | list[int]";
        }
    }

    if constexpr (requires { typename CleanT::value_type; }) {
        using ValueT = typename CleanT::value_type;
        if constexpr (requires { std::declval<CleanT>().begin(); std::declval<CleanT>().end(); }) {
            // A numeric vector is returned as an array.array, so that is
            // what a return says. A parameter keeps list[T]: the ingest side
            // takes a list, a tuple, an array.array or any buffer, but there
            // is no spelling for that which is not also a lie. Sequence[int]
            // is the obvious candidate and admits bytes, which from_python
            // rejects outright, so it would approve a call that fails.
            if constexpr (P == Position::Return && crosses_as_array_v<CleanT>) {
                return "_array[" + type_hint<ValueT, P>() + "]";
            }
            return "list[" + type_hint<ValueT, P>() + "]";
        }
    }

    // std::optional -> T | None
    if constexpr (requires { typename CleanT::value_type; std::declval<CleanT>().has_value(); }) {
        return type_hint<typename CleanT::value_type, P>() + " | None";
    }

    // std::pair/tuple -> tuple[...]
    if constexpr (requires { typename std::tuple_size<CleanT>::type; }) {
        if constexpr (std::tuple_size_v<CleanT> == 2) {
            return "tuple[" + type_hint<std::tuple_element_t<0, CleanT>, P>() + ", " +
                   type_hint<std::tuple_element_t<1, CleanT>, P>() + "]";
        }
    }

    // Default: a class. Its Python name is whatever bind_class registered it
    // as (Vec3f for Vector3<float>), which may not be known yet, so leave a
    // placeholder for generate() to resolve.
    return StubRegistry::type_placeholder(typeid(CleanT).name());
}

template<typename T>
std::string return_hint() { return type_hint<T, Position::Return>(); }

template<typename T>
std::string param_hint() { return type_hint<T, Position::Param>(); }

inline std::string StubRegistry::fallback_class_name(const std::string& typeid_name) {
    std::string name = demangle(typeid_name.c_str());
    if (name.find('<') != std::string::npos) {
        return "Any";
    }
    auto pos = name.rfind("::");
    if (pos != std::string::npos) {
        name = name.substr(pos + 2);
    }
    if (!name.empty() && std::isupper(static_cast<unsigned char>(name[0]))) {
        return name;
    }
    return "Any";
}

// ============================================================================
// Stub Collection During Binding
// ============================================================================

// Called during bind_class to register class metadata
template<typename T>
void register_class_stub(const char* name) {
    ClassInfo& info = StubRegistry::instance().get_or_create_class(name);
    info.name = name;
    StubRegistry::instance().register_type_name(typeid(std::remove_cvref_t<T>).name(), name);
}

// Register a module-level function
template<typename ReturnT, typename... Args>
void register_function_stub(const char* name, const std::vector<std::string>& param_names = {}) {
    MethodInfo func;
    func.name = name;
    func.return_type = return_hint<ReturnT>();

    std::vector<std::string> type_hints = {param_hint<Args>()...};
    for (size_t i = 0; i < type_hints.size(); ++i) {
        std::string pname = i < param_names.size() ? param_names[i] : std::string();
        func.parameters.push_back({pname, type_hints[i]});
    }

    StubRegistry::instance().add_function(func);
}

// Register an enum, from the same reflection bind_enum built it from.
template<typename E>
void register_enum_stub(const char* name) {
    StubRegistry::instance().register_type_name(typeid(E).name(), name);
    EnumInfo info;
    info.name = name;
    info.members = mirror_bridge::enum_members<E>();
    StubRegistry::instance().add_enum(info);
}

// Register a property
template<typename T, typename MemberT>
void register_property_stub(const char* class_name, const char* prop_name, bool readonly = false) {
    ClassInfo& info = StubRegistry::instance().get_or_create_class(class_name);
    info.properties.push_back({prop_name, return_hint<MemberT>(),
                               param_hint<MemberT>(), readonly});
}

// Register a method. Parameter names may be empty strings (unnamed params
// in the C++ declaration) — those fall back to argN.
template<typename ReturnT, typename... Args>
void register_method_stub(const char* class_name, const char* method_name,
                          const std::vector<std::string>& param_names = {},
                          bool is_static = false,
                          std::size_t min_required = static_cast<std::size_t>(-1),
                          bool positional_only = false) {
    ClassInfo& info = StubRegistry::instance().get_or_create_class(class_name);

    MethodInfo method;
    method.name = method_name;
    method.return_type = return_hint<ReturnT>();
    method.is_static = is_static;
    method.positional_only = positional_only;

    // Generate parameters
    std::vector<std::string> type_hints = {param_hint<Args>()...};
    for (size_t i = 0; i < type_hints.size(); ++i) {
        std::string pname = i < param_names.size() ? param_names[i] : std::string();
        // The third field is has_default. render_parameters reads it; without
        // it every defaulted parameter renders as required.
        method.parameters.push_back({pname, type_hints[i], i >= min_required});
    }

    info.methods.push_back(method);
}

// A protocol method whose Python signature is known but whose C++ side is
// not a reflected member function: __len__ comes from size(), __iter__ from
// begin()/end(). Without these the stub would say a class is not iterable
// while the binding iterates it, which is exactly the drift the generated
// .pyi exists to rule out.
inline void register_dunder_stub(const char* class_name, const char* method_name,
                                 const std::string& return_type) {
    ClassInfo& info = StubRegistry::instance().get_or_create_class(class_name);
    info.methods.push_back({method_name, {}, return_type, false});
}

// The runtime always permits zero-arg construction of default-constructible
// classes (py_init's nargs==0 branch), whether or not a default constructor
// was reflected — keep the stub in agreement.
inline void register_default_constructor_if_missing(const char* class_name) {
    ClassInfo& info = StubRegistry::instance().get_or_create_class(class_name);
    for (const auto& ctor : info.constructors) {
        if (ctor.parameters.empty()) return;
    }
    info.constructors.push_back({});
}

// Register a constructor
template<typename... Args>
void register_constructor_stub(const char* class_name,
                               const std::vector<std::string>& param_names = {},
                               std::size_t min_required = static_cast<std::size_t>(-1)) {
    ClassInfo& info = StubRegistry::instance().get_or_create_class(class_name);

    ConstructorInfo ctor;
    std::vector<std::string> type_hints = {param_hint<Args>()...};
    for (size_t i = 0; i < type_hints.size(); ++i) {
        std::string pname = i < param_names.size() ? param_names[i] : std::string();
        ctor.parameters.push_back({pname, type_hints[i], i >= min_required});
    }

    info.constructors.push_back(ctor);
}

// ============================================================================
// Public API
// ============================================================================

// Write stubs to file
inline bool write_stubs(const char* module_name, const char* filename) {
    return StubRegistry::instance().write_to_file(module_name, filename);
}

// Get stub content as string
inline std::string generate_stubs(const char* module_name) {
    return StubRegistry::instance().generate(module_name);
}

// Clear registry (useful for testing)
inline void clear_registry() {
    StubRegistry::instance().clear();
}

} // namespace stubgen
} // namespace mirror_bridge
