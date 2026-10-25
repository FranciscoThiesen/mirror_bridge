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

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
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
    Unsupported
};

template<typename T>
consteval Wire wire_of() {
    using U = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<U, void>) return Wire::None;
    else if constexpr (std::is_same_v<U, bool>) return Wire::Bool;
    else if constexpr (StringLike<U>) return Wire::Utf8;
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
        default:         return nullptr;
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
        default:         return nullptr;
    }
}

// A string parameter is declared differently from a string return. Marshalling
// a string in is free to us: the CLR allocates a native UTF-8 buffer, passes
// it, and releases it. A string coming out cannot be a managed string, because
// then nothing would free the native copy, so it arrives as IntPtr and
// Native.TakeString copies and frees it.
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

struct ClassPlan {
    std::string cpp_name;     // fully qualified, as C++ spells it
    std::string cs_name;      // the C# class name
    std::string symbol;       // the C ABI infix, e.g. "Widget"
    std::string header;
    bool default_constructible = false;
    std::string shim;         // extern "C" definitions
    std::string externs;      // C# DllImport declarations
    std::string body;         // C# properties and methods
    std::vector<Skipped> skipped;
    // One exported symbol per name, so a second overload cannot be bound.
    std::vector<std::string> emitted_methods;
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

class Emitter {
public:
    Emitter(std::string module, std::string ns)
        : module_(std::move(module)), namespace_(std::move(ns)) {}

    // cpp_name is passed in rather than read off the reflection. The only
    // spelling certainly valid in the shim's own translation unit is the fully
    // qualified one the caller already has: display_string_of gives an
    // unqualified name, and on an alias it gives the alias, which is how the
    // first shim came out declaring remove_cvref_t<Device>.
    template<Bindable T>
    void add_class(const char* cs_name, const char* header, const char* cpp_name) {
        ClassPlan p;
        p.cpp_name = cpp_name;
        p.cs_name = cs_name;
        p.symbol = sanitize_symbol(cs_name);
        p.header = header ? header : "";
        p.default_constructible = std::is_default_constructible_v<T>;

        std::ostringstream shim, externs, body;
        emit_lifetime<T>(p, shim, externs, body);
        emit_members<T>(p, shim, externs, body);
        emit_methods<T>(p, shim, externs, body);

        p.shim = shim.str();
        p.externs = externs.str();
        p.body = body.str();
        if (!p.header.empty()) headers_.insert(p.header);
        classes_.push_back(std::move(p));
    }

    bool write(const std::string& out_dir) const {
        const std::string shim_path = out_dir + "/" + module_ + "_shim.cpp";
        const std::string cs_path = out_dir + "/" + module_ + ".cs";
        std::ofstream shim(shim_path), cs(cs_path);
        if (!shim || !cs) {
            std::fprintf(stderr, "mirror_bridge csharp: cannot write into %s\n",
                         out_dir.c_str());
            return false;
        }
        shim << shim_source();
        cs << cs_source();
        if (!shim || !cs) {
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
    std::set<std::string> headers_;

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

        if (p.default_constructible) {
            shim << "MB_EXPORT void* mb_" << sym << "_new(void) {\n"
                 << "try { return new " << cpp << "(); }\n"
                 << "catch (...) { mb_set_error(\"" << cpp << "() threw\"); return nullptr; }\n"
                 << "}\n\n";
            externs << "[DllImport(Lib, EntryPoint = \"mb_" << sym
                    << "_new\", CallingConvention = CallingConvention.Cdecl)]\n"
                    << "private static extern IntPtr New();\n\n";
        } else {
            p.skipped.push_back({p.cs_name, "<constructor>",
                                 "no default constructor, so C# cannot create one"});
        }

        shim << "MB_EXPORT void mb_" << sym << "_delete(void* self) {\n"
             << "delete static_cast<" << cpp << "*>(self);\n"
             << "}\n\n";
        externs << "[DllImport(Lib, EntryPoint = \"mb_" << sym
                << "_delete\", CallingConvention = CallingConvention.Cdecl)]\n"
                << "private static extern void Delete(IntPtr self);\n\n";

        body << "private IntPtr _self;\n"
             << "private readonly bool _owned;\n\n"
             << "/// <summary>Wraps a pointer this object does not own.</summary>\n"
             << "internal " << p.cs_name << "(IntPtr self, bool owned) {\n"
             << "_self = self;\n"
             << "_owned = owned;\n"
             << "}\n\n";

        if (p.default_constructible) {
            body << "public " << p.cs_name << "() {\n"
                 << "_self = New();\n"
                 << "_owned = true;\n"
                 << "if (_self == IntPtr.Zero) throw Native.LastError(\""
                 << p.cs_name << "()\");\n"
                 << "}\n\n";
        }

        body << "public IntPtr Handle => _self;\n\n"
             << "private IntPtr Check() {\n"
             << "if (_self == IntPtr.Zero) {\n"
             << "throw new ObjectDisposedException(\"" << p.cs_name << "\");\n"
             << "}\n"
             << "return _self;\n"
             << "}\n\n"
             << "public void Dispose() {\n"
             << "if (_self != IntPtr.Zero && _owned) Delete(_self);\n"
             << "_self = IntPtr.Zero;\n"
             << "GC.SuppressFinalize(this);\n"
             << "}\n\n"
             << "~" << p.cs_name << "() {\n"
             << "if (_self != IntPtr.Zero && _owned) Delete(_self);\n"
             << "}\n\n";
    }

    // ---- data members -------------------------------------------------

    template<typename T>
    void emit_members(ClassPlan& p, std::ostringstream& shim,
                      std::ostringstream& externs, std::ostringstream& body) {
        constexpr std::size_t n = get_data_member_count<T>();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (emit_member<T, Is>(p, shim, externs, body), ...);
        }(std::make_index_sequence<n>{});
    }

    template<typename T, std::size_t I>
    void emit_member(ClassPlan& p, std::ostringstream& shim,
                     std::ostringstream& externs, std::ostringstream& body) {
        constexpr auto member = get_data_member<T, I>();
        using Raw = typename [:std::meta::type_of(member):];
        using M = std::remove_cvref_t<Raw>;
        constexpr Wire w = wire_of<M>();
        const std::string name(std::meta::identifier_of(member));

        if constexpr (!wire_ok(w)) {
            constexpr const char* tname =
                std::meta::display_string_of(std::meta::type_of(member)).data();
            p.skipped.push_back({p.cs_name, name,
                std::string("type ") + tname
                + " does not cross the C ABI yet; scalars, bool and std::string do"});
            return;
        } else {
            const bool writable = !std::is_const_v<Raw>;
            const std::string prop = cs_member_name(name);
            const std::string sym = "mb_" + p.symbol + "_get_" + name;
            const char* ct = c_type(w);

            shim << "MB_EXPORT " << ct << " " << sym << "(void* self) {\n";
            if constexpr (w == Wire::Utf8) {
                shim << "g_error.clear();\n"
                     << "try { return mb_dup(static_cast<" << p.cpp_name
                     << "*>(self)->" << name << "); }\n"
                     << "catch (...) { mb_set_error(\"" << name << " getter threw\"); return nullptr; }\n";
            } else if constexpr (w == Wire::Bool) {
                shim << "return static_cast<" << p.cpp_name << "*>(self)->"
                     << name << " ? 1 : 0;\n";
            } else {
                shim << "return static_cast<" << ct << ">(static_cast<"
                     << p.cpp_name << "*>(self)->" << name << ");\n";
            }
            shim << "}\n\n";

            externs << "[DllImport(Lib, EntryPoint = \"" << sym
                    << "\", CallingConvention = CallingConvention.Cdecl)]\n"
                    << "private static extern " << cs_wire_type(w)
                    << " Get_" << name << "(IntPtr self);\n\n";

            if (writable) {
                const std::string ssym = "mb_" + p.symbol + "_set_" + name;
                shim << "MB_EXPORT void " << ssym << "(void* self, " << ct << " value) {\n";
                if constexpr (w == Wire::Utf8) {
                    shim << "g_error.clear();\n"
                         << "try { static_cast<" << p.cpp_name << "*>(self)->"
                         << name << " = value ? value : \"\"; }\n"
                         << "catch (...) { mb_set_error(\"" << name << " setter threw\"); }\n";
                } else if constexpr (w == Wire::Bool) {
                    shim << "static_cast<" << p.cpp_name << "*>(self)->"
                         << name << " = (value != 0);\n";
                } else {
                    // decltype rather than a spelled-out type: a member whose
                    // type is a nested typedef has no name valid at namespace
                    // scope in the shim, and this needs none.
                    shim << "static_cast<" << p.cpp_name << "*>(self)->"
                         << name << " = static_cast<decltype(static_cast<"
                         << p.cpp_name << "*>(self)->" << name << ")>(value);\n";
                }
                shim << "}\n\n";
                externs << "[DllImport(Lib, EntryPoint = \"" << ssym
                        << "\", CallingConvention = CallingConvention.Cdecl)]\n"
                        << "private static extern void Set_" << name
                        << "(IntPtr self, " << cs_param_type(w) << " value);\n\n";
            }

            body << "public " << cs_type(w) << " " << prop << " {\n";
            if constexpr (w == Wire::Utf8) {
                body << "get {\n"
                     << "var r = Get_" << name << "(Check());\n"
                     << "Native.Throw(\"" << name << "\");\n"
                     << "return Native.TakeString(r);\n"
                     << "}\n";
            } else if constexpr (w == Wire::Bool) {
                body << "get => Get_" << name << "(Check()) != 0;\n";
            } else {
                body << "get => Get_" << name << "(Check());\n";
            }
            if (writable) {
                if constexpr (w == Wire::Utf8) {
                    body << "set {\n"
                         << "Set_" << name << "(Check(), value ?? \"\");\n"
                         << "Native.Throw(\"" << name << "\");\n"
                         << "}\n";
                } else if constexpr (w == Wire::Bool) {
                    body << "set => Set_" << name << "(Check(), value ? 1 : 0);\n";
                } else {
                    body << "set => Set_" << name << "(Check(), value);\n";
                }
            }
            body << "}\n\n";
        }
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
        using Ret = std::remove_cvref_t<typename [:get_method_return_type<T, I>():]>;
        constexpr Wire rw = wire_of<Ret>();
        constexpr std::size_t argc = get_method_param_count<T, I>();
        const std::string name(std::meta::identifier_of(func));

        if constexpr (!wire_ok(rw)) {
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
        } else {
            const std::string sym = "mb_" + p.symbol + "_" + name;
            const char* rct = c_type(rw);

            // A name bound twice would emit the same symbol twice and fail to
            // link. Overloads are the realistic cause and they are reported
            // rather than silently merged.
            for (const auto& prior : p.emitted_methods) {
                if (prior == name) {
                    p.skipped.push_back({p.cs_name, name + "()",
                        "overloaded: C# P/Invoke needs one symbol per name, so "
                        "only the first overload is bound"});
                    return;
                }
            }
            p.emitted_methods.push_back(name);

            std::string c_params = "void* self";
            std::string cs_ext_params = "IntPtr self";
            std::string cs_sig_params;
            std::string call_args;
            std::string fwd_args;
            [&]<std::size_t... Ps>(std::index_sequence<Ps...>) {
                ([&] {
                    constexpr Wire pw = wire_of<
                        std::remove_cvref_t<typename [:get_method_param_type<T, I, Ps>():]>>();
                    const std::string an = "a" + std::to_string(Ps);
                    c_params += std::string(", ") + c_type(pw) + " " + an;
                    cs_ext_params += std::string(", ") + cs_param_type(pw) + " " + an;
                    if (!cs_sig_params.empty()) cs_sig_params += ", ";
                    cs_sig_params += std::string(cs_type(pw)) + " " + an;
                    if (!call_args.empty()) call_args += ", ";
                    if constexpr (pw == Wire::Bool) {
                        fwd_args += std::string(fwd_args.empty() ? "" : ", ") + "(" + an + " != 0)";
                        call_args += an + " ? 1 : 0";
                    } else if constexpr (pw == Wire::Utf8) {
                        fwd_args += std::string(fwd_args.empty() ? "" : ", ") + "(" + an + " ? " + an + " : \"\")";
                        call_args += an + " ?? \"\"";   // LPUTF8Str marshals it
                    } else {
                        fwd_args += std::string(fwd_args.empty() ? "" : ", ") + an;
                        call_args += an;
                    }
                }(), ...);
            }(std::make_index_sequence<argc>{});

            const std::string invoke = std::string("static_cast<") + p.cpp_name
                                     + "*>(self)->" + name + "(" + fwd_args + ")";

            shim << "MB_EXPORT " << rct << " " << sym << "(" << c_params << ") {\n"
                 << "g_error.clear();\n"
                 << "try {\n";
            if constexpr (rw == Wire::None) {
                shim << invoke << ";\n";
            } else if constexpr (rw == Wire::Utf8) {
                shim << "return mb_dup(" << invoke << ");\n";
            } else if constexpr (rw == Wire::Bool) {
                shim << "return " << invoke << " ? 1 : 0;\n";
            } else {
                shim << "return static_cast<" << rct << ">(" << invoke << ");\n";
            }
            shim << "} catch (const std::exception& ex) {\n"
                 << "mb_set_error(ex.what());\n"
                 << "} catch (...) {\n"
                 << "mb_set_error(\"" << p.cpp_name << "::" << name
                 << " threw a non-std exception\");\n"
                 << "}\n";
            if constexpr (rw != Wire::None) {
                shim << "return " << zero_of(rw) << ";\n";
            }
            shim << "}\n\n";

            externs << "[DllImport(Lib, EntryPoint = \"" << sym
                    << "\", CallingConvention = CallingConvention.Cdecl)]\n"
                    << "private static extern " << cs_wire_type(rw) << " Call_"
                    << name << "(" << cs_ext_params << ");\n\n";

            body << "public " << cs_type(rw) << " " << cs_member_name(name)
                 << "(" << cs_sig_params << ") {\n";
            const std::string call = std::string("Call_") + name + "(Check()"
                                   + (call_args.empty() ? "" : ", " + call_args) + ")";
            if constexpr (rw == Wire::None) {
                body << call << ";\n"
                     << "Native.Throw(\"" << name << "\");\n";
            } else if constexpr (rw == Wire::Utf8) {
                body << "var r = " << call << ";\n"
                     << "Native.Throw(\"" << name << "\");\n"
                     << "return Native.TakeString(r);\n";
            } else if constexpr (rw == Wire::Bool) {
                body << "var r = " << call << ";\n"
                     << "Native.Throw(\"" << name << "\");\n"
                     << "return r != 0;\n";
            } else {
                body << "var r = " << call << ";\n"
                     << "Native.Throw(\"" << name << "\");\n"
                     << "return r;\n";
            }
            body << "}\n\n";
        }
    }

    template<typename T, std::size_t I, std::size_t... Ps>
    static consteval bool params_ok(std::index_sequence<Ps...>) {
        return (wire_ok(wire_of<std::remove_cvref_t<
                    typename [:get_method_param_type<T, I, Ps>():]>>()) && ... && true);
    }

    static constexpr const char* zero_of(Wire w) {
        switch (w) {
            case Wire::Utf8: return "nullptr";
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
          << "#include <cstdint>\n#include <cstdlib>\n#include <cstring>\n"
          << "#include <exception>\n#include <new>\n#include <string>\n\n";
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
          << "static void mb_set_error(const char* what) {\n"
          << "    try { g_error = what ? what : \"unknown error\"; } catch (...) {}\n"
          << "}\n\n"
          << "MB_EXPORT const char* mb_last_error(void) {\n"
          << "    return g_error.empty() ? nullptr : g_error.c_str();\n"
          << "}\n\n"
          << "MB_EXPORT void mb_clear_error(void) { g_error.clear(); }\n\n"
          << "// Every entry point that can record an error clears first, so what\n"
          << "// mb_last_error returns belongs to the call that just returned. Without\n"
          << "// this, an error left by an unchecked call -- a string property accessor,\n"
          << "// which has no natural place to raise -- surfaced as the next method's\n"
          << "// failure instead.\n"
          << "// The managed caller owns every string it receives and frees it here.\n"
          << "// Returning a pointer into the live std::string would be one allocation\n"
          << "// cheaper and would dangle as soon as anything mutated the object.\n"
          << "static const char* mb_dup(const std::string& s) {\n"
          << "    char* out = static_cast<char*>(std::malloc(s.size() + 1));\n"
          << "    if (!out) return nullptr;\n"
          << "    std::memcpy(out, s.c_str(), s.size() + 1);\n"
          << "    return out;\n"
          << "}\n\n"
          << "MB_EXPORT void mb_free_string(const char* p) {\n"
          << "    std::free(const_cast<char*>(p));\n"
          << "}\n\n";
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
          << "using System.Runtime.InteropServices;\n\n"
          << "namespace " << namespace_ << " {\n\n"
          << "internal static class Native {\n"
          << "    internal const string Lib = \"" << module_ << "\";\n\n"
          << "    [DllImport(Lib, EntryPoint = \"mb_last_error\", CallingConvention = CallingConvention.Cdecl)]\n"
          << "    private static extern IntPtr LastErrorPtr();\n\n"
          << "    [DllImport(Lib, EntryPoint = \"mb_clear_error\", CallingConvention = CallingConvention.Cdecl)]\n"
          << "    private static extern void ClearError();\n\n"
          << "    [DllImport(Lib, EntryPoint = \"mb_free_string\", CallingConvention = CallingConvention.Cdecl)]\n"
          << "    private static extern void FreeString(IntPtr p);\n\n"
          << "    /// <summary>Copies a native UTF-8 string and frees the native copy.</summary>\n"
          << "    internal static string TakeString(IntPtr p) {\n"
          << "        if (p == IntPtr.Zero) return null;\n"
          << "        try { return Marshal.PtrToStringUTF8(p); }\n"
          << "        finally { FreeString(p); }\n"
          << "    }\n\n"
          << "    internal static Exception LastError(string where) {\n"
          << "        var msg = Marshal.PtrToStringUTF8(LastErrorPtr());\n"
          << "        ClearError();\n"
          << "        return new InvalidOperationException(\n"
          << "            msg == null ? where + \" failed in C++\" : where + \": \" + msg);\n"
          << "    }\n\n"
          << "    /// <summary>Raises whatever the last native call recorded, if any.</summary>\n"
          << "    internal static void Throw(string where) {\n"
          << "        if (LastErrorPtr() != IntPtr.Zero) throw LastError(where);\n"
          << "    }\n"
          << "}\n\n";
        for (const auto& c : classes_) {
            std::ostringstream cls;
            cls << "/// <summary>Binds the C++ class <c>" << c.cpp_name << "</c>.</summary>\n"
                << "public sealed class " << c.cs_name << " : IDisposable {\n"
                << "private const string Lib = Native.Lib;\n\n"
                << c.externs
                << c.body;
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
