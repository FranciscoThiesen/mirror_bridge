#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge Rust - Rust FFI Bindings for C++ Code via C++26 Reflection
// ═══════════════════════════════════════════════════════════════════════════
//
// Generates Rust FFI bindings from C++ classes using C++26 reflection:
//   1. extern "C" wrapper functions for each method/constructor/property
//   2. C header with opaque handle types
//   3. Safe Rust wrapper types with idiomatic API and Drop
//
// The generated output is three files:
//   - <class>_ffi.h   : C header with extern "C" function declarations
//   - <class>_ffi.cpp : C++ implementation forwarding to actual class
//   - <class>.rs      : Rust FFI declarations + safe wrapper struct
//
// Usage:
//   #include "rust/mirror_bridge_rust.hpp"
//   auto output = mirror_bridge::rust::generate_bindings<MyClass>("MyClass");

#include "../core/mirror_bridge_core.hpp"
#include <string>
#include <sstream>
#include <vector>

namespace mirror_bridge {
namespace rust {

using namespace mirror_bridge::core;

// ============================================================================
// C++ Type to Rust Type Mapping
// ============================================================================

// Maps C++ types to their Rust primitive equivalents.
// Returns nullptr for complex types that need special handling.
template<typename T>
consteval const char* rust_type_name() {
    using U = std::remove_cvref_t<T>;

    // bool is mapped to i32 on the FFI boundary (C int), then converted
    // to Rust bool in the safe wrapper via != 0.
    if constexpr (std::is_same_v<U, bool>) return "i32";
    else if constexpr (std::is_same_v<U, char>) return "i8";
    else if constexpr (std::is_same_v<U, int8_t>) return "i8";
    else if constexpr (std::is_same_v<U, uint8_t>) return "u8";
    else if constexpr (std::is_same_v<U, int16_t>) return "i16";
    else if constexpr (std::is_same_v<U, uint16_t>) return "u16";
    else if constexpr (std::is_same_v<U, int32_t> || (std::is_same_v<U, int> && sizeof(int) == 4)) return "i32";
    else if constexpr (std::is_same_v<U, uint32_t> || (std::is_same_v<U, unsigned int> && sizeof(unsigned int) == 4)) return "u32";
    else if constexpr (std::is_same_v<U, int64_t> || std::is_same_v<U, long long>) return "i64";
    else if constexpr (std::is_same_v<U, uint64_t> || std::is_same_v<U, unsigned long long>) return "u64";
    else if constexpr (std::is_same_v<U, float>) return "f32";
    else if constexpr (std::is_same_v<U, double>) return "f64";
    else return nullptr;
}

// The safe Rust type for property accessors (differs from FFI type for bool)
template<typename T>
consteval const char* rust_safe_type_name() {
    using U = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<U, bool>) return "bool";
    else return rust_type_name<U>();
}

// Map C++ type to C FFI type name (for the extern "C" header)
template<typename T>
std::string c_ffi_type_name() {
    using U = std::remove_cvref_t<T>;

    if constexpr (std::is_same_v<U, bool>) return "int";
    else if constexpr (std::is_same_v<U, int8_t> || std::is_same_v<U, char>) return "int8_t";
    else if constexpr (std::is_same_v<U, uint8_t>) return "uint8_t";
    else if constexpr (std::is_same_v<U, int16_t>) return "int16_t";
    else if constexpr (std::is_same_v<U, uint16_t>) return "uint16_t";
    else if constexpr (std::is_same_v<U, int32_t> || (std::is_same_v<U, int> && sizeof(int) == 4)) return "int32_t";
    else if constexpr (std::is_same_v<U, uint32_t>) return "uint32_t";
    else if constexpr (std::is_same_v<U, int64_t> || std::is_same_v<U, long long>) return "int64_t";
    else if constexpr (std::is_same_v<U, uint64_t>) return "uint64_t";
    else if constexpr (std::is_same_v<U, float>) return "float";
    else if constexpr (std::is_same_v<U, double>) return "double";
    else if constexpr (std::is_same_v<U, std::string>) return "const char*";
    else return "void*";
}

// Rust type for FFI extern declarations (matches C ABI exactly)
template<typename T>
std::string rust_ffi_type() {
    using U = std::remove_cvref_t<T>;

    constexpr const char* primitive = rust_type_name<U>();
    if constexpr (primitive != nullptr) {
        return primitive;
    } else if constexpr (std::is_same_v<U, std::string>) {
        return "*const std::os::raw::c_char";
    } else {
        return "*mut std::ffi::c_void";
    }
}

// ============================================================================
// Rust Keyword Escaping
// ============================================================================

// Rust reserved keywords that can't be used as identifiers.
// Uses raw identifier syntax r#name to escape them.
inline std::string escape_rust_keyword(const std::string& name) {
    // Strict keywords (https://doc.rust-lang.org/reference/keywords.html)
    static const char* keywords[] = {
        "as", "async", "await", "break", "const", "continue", "crate",
        "dyn", "else", "enum", "extern", "false", "fn", "for", "if",
        "impl", "in", "let", "loop", "match", "mod", "move", "mut",
        "pub", "ref", "return", "self", "Self", "static", "struct",
        "super", "trait", "true", "type", "unsafe", "use", "where",
        "while", "yield", "abstract", "become", "box", "do", "final",
        "macro", "override", "priv", "try", "typeof", "unsized",
        "virtual",
    };
    for (const char* kw : keywords) {
        if (name == kw) {
            return "r#" + name;
        }
    }
    return name;
}

// ============================================================================
// Binding Output
// ============================================================================

struct BindingOutput {
    std::string c_header;       // extern "C" header content
    std::string cpp_impl;       // C++ implementation of extern "C" wrappers
    std::string rust_source;    // Rust FFI + safe wrapper source
};

// ============================================================================
// Code Generation
// ============================================================================

// Parameters:
//   class_name   — Name for the generated Rust struct and FFI prefix
//   class_header — Path to the original C++ header (included in generated .cpp).
//                  If nullptr, the user must ensure the header is on the include path.
template<Bindable T>
BindingOutput generate_bindings(const char* class_name, const char* class_header = nullptr) {
    BindingOutput output;
    std::ostringstream c_header;
    std::ostringstream cpp_impl;
    std::ostringstream rust_src;

    const std::string name(class_name);
    const std::string prefix = name + "_";

    // Collect member names for collision detection with method names
    std::vector<std::string> member_names;
    constexpr std::size_t member_count = get_data_member_count<T>();
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((member_names.push_back(std::string(std::meta::identifier_of(get_data_member<T, Is>())))), ...);
    }(std::make_index_sequence<member_count>{});

    // ---- C Header ----
    c_header << "// Auto-generated by mirror_bridge - do not edit\n";
    c_header << "#pragma once\n";
    c_header << "#include <stdint.h>\n\n";
    c_header << "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";
    c_header << "typedef void* " << name << "_handle;\n\n";
    c_header << name << "_handle " << prefix << "create(void);\n";
    c_header << "void " << prefix << "destroy(" << name << "_handle handle);\n\n";

    // ---- C++ Implementation ----
    cpp_impl << "// Auto-generated by mirror_bridge - do not edit\n";
    if (class_header) {
        cpp_impl << "#include \"" << class_header << "\"\n";
    }
    cpp_impl << "#include \"" << name << "_ffi.h\"\n";
    cpp_impl << "#include <cstring>\n\n";
    cpp_impl << "extern \"C\" {\n\n";
    cpp_impl << name << "_handle " << prefix << "create(void) {\n";
    cpp_impl << "    return static_cast<" << name << "_handle>(new " << name << "());\n";
    cpp_impl << "}\n\n";
    cpp_impl << "void " << prefix << "destroy(" << name << "_handle handle) {\n";
    cpp_impl << "    delete static_cast<" << name << "*>(handle);\n";
    cpp_impl << "}\n\n";

    // ---- Rust Source ----
    rust_src << "//! Auto-generated by mirror_bridge - do not edit\n";
    rust_src << "//! Safe Rust bindings for C++ class `" << name << "`\n";
    rust_src << "//!\n";
    rust_src << "//! # Safety\n";
    rust_src << "//! This type wraps a C++ object via opaque pointer. It is NOT\n";
    rust_src << "//! Send or Sync by default — implement these traits only if the\n";
    rust_src << "//! underlying C++ class is thread-safe.\n\n";
    rust_src << "use std::ffi::{CStr, CString};\n";
    rust_src << "use std::os::raw::c_char;\n\n";

    // Extern block
    rust_src << "extern \"C\" {\n";
    rust_src << "    fn " << prefix << "create() -> *mut std::ffi::c_void;\n";
    rust_src << "    fn " << prefix << "destroy(handle: *mut std::ffi::c_void);\n";

    // Generate getters/setters for data members
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr auto member = get_data_member<T, Is>();
            auto member_name = std::string(std::meta::identifier_of(member));
            using MemberType = std::remove_cvref_t<typename [:std::meta::type_of(member):]>;

            std::string c_type = c_ffi_type_name<MemberType>();
            std::string rust_type = rust_ffi_type<MemberType>();

            // Getter declaration + implementation
            c_header << c_type << " " << prefix << "get_" << member_name
                     << "(" << name << "_handle handle);\n";
            cpp_impl << c_type << " " << prefix << "get_" << member_name
                     << "(" << name << "_handle handle) {\n"
                     << "    auto* obj = static_cast<" << name << "*>(handle);\n";
            if constexpr (std::is_same_v<MemberType, std::string>) {
                cpp_impl << "    return obj->" << member_name << ".c_str();\n";
            } else {
                cpp_impl << "    return obj->" << member_name << ";\n";
            }
            cpp_impl << "}\n\n";
            rust_src << "    fn " << prefix << "get_" << member_name
                     << "(handle: *mut std::ffi::c_void) -> " << rust_type << ";\n";

            // Setter declaration + implementation
            c_header << "void " << prefix << "set_" << member_name
                     << "(" << name << "_handle handle, " << c_type << " value);\n";
            cpp_impl << "void " << prefix << "set_" << member_name
                     << "(" << name << "_handle handle, " << c_type << " value) {\n"
                     << "    auto* obj = static_cast<" << name << "*>(handle);\n";
            if constexpr (std::is_same_v<MemberType, std::string>) {
                cpp_impl << "    obj->" << member_name << " = std::string(value);\n";
            } else {
                cpp_impl << "    obj->" << member_name << " = value;\n";
            }
            cpp_impl << "}\n\n";
            rust_src << "    fn " << prefix << "set_" << member_name
                     << "(handle: *mut std::ffi::c_void, value: " << rust_type << ");\n";
        }(), ...);
    }(std::make_index_sequence<member_count>{});

    // Generate method wrappers (void and primitive return types only for now)
    constexpr std::size_t method_count = get_member_function_count<T>();
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr auto func = get_member_function<T, Is>();
            auto method_name = std::string(std::meta::identifier_of(func));
            constexpr auto return_meta = get_method_return_type<T, Is>();
            using ReturnType = std::remove_cvref_t<typename [:return_meta:]>;
            constexpr std::size_t param_count = get_method_param_count<T, Is>();

            // Only generate for zero-parameter methods with primitive/void returns
            // (multi-param methods require more complex FFI marshalling)
            if constexpr (param_count == 0) {
                constexpr const char* ret_rust = rust_type_name<ReturnType>();
                std::string ret_c = c_ffi_type_name<ReturnType>();

                if constexpr (std::is_void_v<ReturnType>) {
                    // void method
                    c_header << "void " << prefix << method_name << "(" << name << "_handle handle);\n";
                    cpp_impl << "void " << prefix << method_name << "(" << name << "_handle handle) {\n"
                             << "    static_cast<" << name << "*>(handle)->" << method_name << "();\n"
                             << "}\n\n";
                    rust_src << "    fn " << prefix << method_name << "(handle: *mut std::ffi::c_void);\n";
                } else if constexpr (ret_rust != nullptr || std::is_same_v<ReturnType, std::string>) {
                    // Primitive or string return
                    std::string rust_ret = rust_ffi_type<ReturnType>();

                    c_header << ret_c << " " << prefix << method_name << "(" << name << "_handle handle);\n";
                    cpp_impl << ret_c << " " << prefix << method_name << "(" << name << "_handle handle) {\n"
                             << "    auto* obj = static_cast<" << name << "*>(handle);\n";
                    if constexpr (std::is_same_v<ReturnType, std::string>) {
                        // Returning string requires static storage to keep the pointer valid.
                        // Each method gets its own thread_local buffer.
                        cpp_impl << "    static thread_local std::string _result;\n"
                                 << "    _result = obj->" << method_name << "();\n"
                                 << "    return _result.c_str();\n";
                    } else {
                        cpp_impl << "    return obj->" << method_name << "();\n";
                    }
                    cpp_impl << "}\n\n";
                    rust_src << "    fn " << prefix << method_name << "(handle: *mut std::ffi::c_void) -> " << rust_ret << ";\n";
                }
            }
        }(), ...);
    }(std::make_index_sequence<method_count>{});

    c_header << "\n#ifdef __cplusplus\n}\n#endif\n";
    cpp_impl << "} // extern \"C\"\n";
    rust_src << "}\n\n";

    // ---- Safe Rust Wrapper ----
    rust_src << "/// Safe wrapper for C++ class `" << name << "`.\n";
    rust_src << "///\n";
    rust_src << "/// This type is NOT `Send` or `Sync` by default. If the underlying\n";
    rust_src << "/// C++ class is thread-safe, you may implement these traits manually.\n";
    rust_src << "pub struct " << name << " {\n";
    rust_src << "    handle: *mut std::ffi::c_void,\n";
    rust_src << "}\n\n";

    rust_src << "impl " << name << " {\n";
    rust_src << "    pub fn new() -> Self {\n";
    rust_src << "        unsafe { " << name << " { handle: " << prefix << "create() } }\n";
    rust_src << "    }\n\n";

    // Generate safe property accessors (names escaped for Rust keywords)
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr auto member = get_data_member<T, Is>();
            auto member_name_raw = std::string(std::meta::identifier_of(member));
            auto member_name = escape_rust_keyword(member_name_raw);
            using MemberType = std::remove_cvref_t<typename [:std::meta::type_of(member):]>;

            constexpr const char* safe_type = rust_safe_type_name<MemberType>();

            // Getter (FFI functions use raw C++ name, Rust pub fn uses escaped name)
            if constexpr (std::is_same_v<MemberType, std::string>) {
                rust_src << "    pub fn " << member_name << "(&self) -> String {\n"
                         << "        unsafe {\n"
                         << "            let ptr = " << prefix << "get_" << member_name_raw << "(self.handle);\n"
                         << "            CStr::from_ptr(ptr).to_string_lossy().into_owned()\n"
                         << "        }\n"
                         << "    }\n\n";
            } else if constexpr (std::is_same_v<MemberType, bool>) {
                rust_src << "    pub fn " << member_name << "(&self) -> bool {\n"
                         << "        unsafe { " << prefix << "get_" << member_name_raw << "(self.handle) != 0 }\n"
                         << "    }\n\n";
            } else if constexpr (safe_type != nullptr) {
                rust_src << "    pub fn " << member_name << "(&self) -> " << safe_type << " {\n"
                         << "        unsafe { " << prefix << "get_" << member_name_raw << "(self.handle) }\n"
                         << "    }\n\n";
            }

            // Setter
            if constexpr (std::is_same_v<MemberType, std::string>) {
                rust_src << "    pub fn set_" << member_name << "(&mut self, value: &str) {\n"
                         << "        let c_str = CString::new(value)\n"
                         << "            .expect(\"string must not contain null bytes\");\n"
                         << "        unsafe { " << prefix << "set_" << member_name_raw << "(self.handle, c_str.as_ptr()) }\n"
                         << "    }\n\n";
            } else if constexpr (std::is_same_v<MemberType, bool>) {
                rust_src << "    pub fn set_" << member_name << "(&mut self, value: bool) {\n"
                         << "        unsafe { " << prefix << "set_" << member_name_raw << "(self.handle, value as i32) }\n"
                         << "    }\n\n";
            } else if constexpr (safe_type != nullptr) {
                rust_src << "    pub fn set_" << member_name << "(&mut self, value: " << safe_type << ") {\n"
                         << "        unsafe { " << prefix << "set_" << member_name_raw << "(self.handle, value) }\n"
                         << "    }\n\n";
            }
        }(), ...);
    }(std::make_index_sequence<member_count>{});

    // Generate safe method wrappers.
    // Methods whose names collide with data member names are skipped (the
    // getter already occupies that name in the Rust impl block).
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr auto func = get_member_function<T, Is>();
            auto method_name_raw = std::string(std::meta::identifier_of(func));

            // Skip methods that collide with data member names
            for (const auto& mn : member_names) {
                if (mn == method_name_raw) return;
            }

            auto method_name = escape_rust_keyword(method_name_raw);
            constexpr auto return_meta = get_method_return_type<T, Is>();
            using ReturnType = std::remove_cvref_t<typename [:return_meta:]>;
            constexpr std::size_t param_count = get_method_param_count<T, Is>();

            if constexpr (param_count == 0) {
                if constexpr (std::is_void_v<ReturnType>) {
                    rust_src << "    pub fn " << method_name << "(&self) {\n"
                             << "        unsafe { " << prefix << method_name_raw << "(self.handle) }\n"
                             << "    }\n\n";
                } else if constexpr (std::is_same_v<ReturnType, std::string>) {
                    rust_src << "    pub fn " << method_name << "(&self) -> String {\n"
                             << "        unsafe {\n"
                             << "            let ptr = " << prefix << method_name_raw << "(self.handle);\n"
                             << "            CStr::from_ptr(ptr).to_string_lossy().into_owned()\n"
                             << "        }\n"
                             << "    }\n\n";
                } else if constexpr (std::is_same_v<ReturnType, bool>) {
                    rust_src << "    pub fn " << method_name << "(&self) -> bool {\n"
                             << "        unsafe { " << prefix << method_name_raw << "(self.handle) != 0 }\n"
                             << "    }\n\n";
                } else if constexpr (rust_type_name<ReturnType>() != nullptr) {
                    constexpr const char* safe_ret = rust_safe_type_name<ReturnType>();
                    rust_src << "    pub fn " << method_name << "(&self) -> " << safe_ret << " {\n"
                             << "        unsafe { " << prefix << method_name_raw << "(self.handle) }\n"
                             << "    }\n\n";
                }
            }
        }(), ...);
    }(std::make_index_sequence<method_count>{});

    rust_src << "}\n\n";

    // Drop — releases the C++ object
    rust_src << "impl Drop for " << name << " {\n"
             << "    fn drop(&mut self) {\n"
             << "        unsafe { " << prefix << "destroy(self.handle) }\n"
             << "    }\n"
             << "}\n";

    output.c_header = c_header.str();
    output.cpp_impl = cpp_impl.str();
    output.rust_source = rust_src.str();
    return output;
}

// Convenience: write binding files to disk
template<Bindable T>
bool write_bindings(const char* class_name, const char* output_dir) {
    auto bindings = generate_bindings<T>(class_name);
    std::string dir(output_dir);
    std::string name(class_name);

    auto write_file = [](const std::string& path, const std::string& content) -> bool {
        FILE* f = fopen(path.c_str(), "w");
        if (!f) return false;
        fputs(content.c_str(), f);
        fclose(f);
        return true;
    };

    return write_file(dir + "/" + name + "_ffi.h", bindings.c_header) &&
           write_file(dir + "/" + name + "_ffi.cpp", bindings.cpp_impl) &&
           write_file(dir + "/" + name + ".rs", bindings.rust_source);
}

} // namespace rust
} // namespace mirror_bridge
