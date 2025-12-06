// ============================================================================
// Mirror Bridge - C++17 Code Generator
// ============================================================================
//
// This header generates standalone C++17 binding code that can be compiled
// with any standard C++17 compiler (gcc, clang, MSVC).
//
// Usage:
//   1. Create a codegen driver file:
//      #include "codegen/mirror_bridge_codegen.hpp"
//      #include "your_classes.hpp"
//
//      int main() {
//          mirror_bridge::codegen::generate_module("mymodule",
//              mirror_bridge::codegen::bind<YourClass>("YourClass"),
//              mirror_bridge::codegen::bind<OtherClass>("OtherClass")
//          );
//          return 0;
//      }
//
//   2. Compile with reflection compiler and run to generate code:
//      clang++ -std=c++2c -freflection ... codegen_driver.cpp -o codegen
//      ./codegen > mymodule_bindings.cpp
//
//   3. Compile the generated code with any C++17 compiler:
//      g++ -std=c++17 -shared -fPIC mymodule_bindings.cpp -o mymodule.so
//
// ============================================================================

#pragma once

#include <meta>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <type_traits>

namespace mirror_bridge {
namespace codegen {

// ============================================================================
// Class binding info - collected via reflection at runtime
// ============================================================================

struct MemberInfo {
    std::string name;
    std::string type;
    bool is_const;
};

struct MethodInfo {
    std::string name;
    std::string return_type;
    std::vector<std::pair<std::string, std::string>> params; // name, type
    bool is_const;
    bool is_static;
    bool is_void;
};

struct ConstructorInfo {
    std::vector<std::pair<std::string, std::string>> params; // name, type
};

struct ClassBinding {
    std::string cpp_name;
    std::string py_name;
    std::vector<MemberInfo> members;
    std::vector<MethodInfo> methods;
    std::vector<ConstructorInfo> constructors;
    bool has_default_ctor = false;
};

// ============================================================================
// Reflection helpers using same pattern as mirror_bridge_python.hpp
// ============================================================================

template<typename T>
consteval std::size_t get_data_member_count() {
    return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()).size();
}

template<typename T>
consteval auto get_data_member_at(std::size_t Index) {
    return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current())[Index];
}

template<typename T>
consteval std::size_t get_method_count() {
    auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
    std::size_t count = 0;
    for (auto member : all_members) {
        if (std::meta::is_function(member) &&
            !std::meta::is_constructor(member) &&
            !std::meta::is_special_member_function(member) &&
            !std::meta::is_operator_function(member)) {
            count++;
        }
    }
    return count;
}

template<typename T>
consteval auto get_method_at(std::size_t Index) {
    auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
    std::size_t func_index = 0;
    for (auto member : all_members) {
        if (std::meta::is_function(member) &&
            !std::meta::is_constructor(member) &&
            !std::meta::is_special_member_function(member) &&
            !std::meta::is_operator_function(member)) {
            if (func_index == Index) {
                return member;
            }
            func_index++;
        }
    }
    return all_members[0];
}

template<typename T>
consteval std::size_t get_constructor_count() {
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

template<typename T>
consteval auto get_constructor_at(std::size_t Index) {
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

// ============================================================================
// Parameter info helpers (consteval with template indices)
// ============================================================================

template<typename T, std::size_t FuncIndex>
consteval std::size_t get_method_param_count() {
    constexpr auto method = get_method_at<T>(FuncIndex);
    return std::meta::parameters_of(method).size();
}

template<typename T, std::size_t FuncIndex, std::size_t ParamIndex>
consteval auto get_method_param_info() {
    constexpr auto method = get_method_at<T>(FuncIndex);
    return std::meta::parameters_of(method)[ParamIndex];
}

template<typename T, std::size_t CtorIndex>
consteval std::size_t get_ctor_param_count() {
    constexpr auto ctor = get_constructor_at<T>(CtorIndex);
    return std::meta::parameters_of(ctor).size();
}

template<typename T, std::size_t CtorIndex, std::size_t ParamIndex>
consteval auto get_ctor_param_info() {
    constexpr auto ctor = get_constructor_at<T>(CtorIndex);
    return std::meta::parameters_of(ctor)[ParamIndex];
}

// ============================================================================
// Introspect class using index_sequence pattern (like mirror_bridge_python.hpp)
// ============================================================================

template<typename T, std::size_t... Is>
void collect_members(ClassBinding& binding, std::index_sequence<Is...>) {
    ([&] {
        constexpr auto member = get_data_member_at<T>(Is);
        MemberInfo info;
        info.name = std::string(std::meta::identifier_of(member));
        info.type = std::string(std::meta::display_string_of(std::meta::type_of(member)));
        info.is_const = std::meta::is_const(std::meta::type_of(member));
        binding.members.push_back(info);
    }(), ...);
}

template<typename T, std::size_t FuncIndex, std::size_t... ParamIs>
void collect_method_params(MethodInfo& info, std::index_sequence<ParamIs...>) {
    ([&] {
        constexpr auto param = get_method_param_info<T, FuncIndex, ParamIs>();
        info.params.push_back({
            std::string(std::meta::identifier_of(param)),
            std::string(std::meta::display_string_of(std::meta::type_of(param)))
        });
    }(), ...);
}

template<typename T, std::size_t... Is>
void collect_methods(ClassBinding& binding, std::index_sequence<Is...>) {
    ([&] {
        constexpr auto method = get_method_at<T>(Is);
        MethodInfo info;
        info.name = std::string(std::meta::identifier_of(method));
        info.return_type = std::string(std::meta::display_string_of(std::meta::return_type_of(method)));
        info.is_const = std::meta::is_const(method);
        info.is_static = std::meta::is_static_member(method);
        info.is_void = std::is_void_v<typename [:std::meta::return_type_of(method):]>;

        constexpr std::size_t param_count = get_method_param_count<T, Is>();
        collect_method_params<T, Is>(info, std::make_index_sequence<param_count>{});

        binding.methods.push_back(info);
    }(), ...);
}

template<typename T, std::size_t CtorIndex, std::size_t... ParamIs>
void collect_ctor_params(ConstructorInfo& info, std::index_sequence<ParamIs...>) {
    ([&] {
        constexpr auto param = get_ctor_param_info<T, CtorIndex, ParamIs>();
        info.params.push_back({
            std::string(std::meta::identifier_of(param)),
            std::string(std::meta::display_string_of(std::meta::type_of(param)))
        });
    }(), ...);
}

template<typename T, std::size_t... Is>
void collect_constructors(ClassBinding& binding, std::index_sequence<Is...>) {
    ([&] {
        ConstructorInfo info;
        constexpr std::size_t param_count = get_ctor_param_count<T, Is>();
        collect_ctor_params<T, Is>(info, std::make_index_sequence<param_count>{});
        binding.constructors.push_back(info);
    }(), ...);
}

template<typename T>
ClassBinding introspect_class(const char* py_name) {
    ClassBinding binding;
    binding.cpp_name = std::string(std::meta::display_string_of(^^T));
    binding.py_name = py_name;

    // Collect data members
    constexpr std::size_t member_count = get_data_member_count<T>();
    collect_members<T>(binding, std::make_index_sequence<member_count>{});

    // Collect methods
    constexpr std::size_t method_count = get_method_count<T>();
    collect_methods<T>(binding, std::make_index_sequence<method_count>{});

    // Collect constructors
    constexpr std::size_t ctor_count = get_constructor_count<T>();
    collect_constructors<T>(binding, std::make_index_sequence<ctor_count>{});

    // Check for default constructibility
    binding.has_default_ctor = std::is_default_constructible_v<T>;

    return binding;
}

// ============================================================================
// Code generation helpers
// ============================================================================

inline void print_header() {
    std::cout << R"(// ============================================================================
// AUTO-GENERATED FILE - DO NOT EDIT
// Generated by Mirror Bridge C++17 Code Generator
// ============================================================================
//
// This file can be compiled with any C++17 compliant compiler:
//   g++ -std=c++17 -shared -fPIC -o module.so module_bindings.cpp $(python3-config --includes --ldflags)
//   clang++ -std=c++17 -shared -fPIC -o module.so module_bindings.cpp $(python3-config --includes --ldflags)
//
// ============================================================================

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdio>
#include <cmath>

)";
}

inline void print_conversion_helpers() {
    std::cout << R"(
// ============================================================================
// Type Conversion Helpers
// ============================================================================

namespace mirror_bridge_conv {

// Convert C++ types to Python objects
inline PyObject* to_python(int value) { return PyLong_FromLong(value); }
inline PyObject* to_python(long value) { return PyLong_FromLong(value); }
inline PyObject* to_python(long long value) { return PyLong_FromLongLong(value); }
inline PyObject* to_python(unsigned int value) { return PyLong_FromUnsignedLong(value); }
inline PyObject* to_python(unsigned long value) { return PyLong_FromUnsignedLong(value); }
inline PyObject* to_python(unsigned long long value) { return PyLong_FromUnsignedLongLong(value); }
inline PyObject* to_python(float value) { return PyFloat_FromDouble(value); }
inline PyObject* to_python(double value) { return PyFloat_FromDouble(value); }
inline PyObject* to_python(bool value) { return PyBool_FromLong(value); }
inline PyObject* to_python(const std::string& value) {
    return PyUnicode_FromStringAndSize(value.data(), value.size());
}
inline PyObject* to_python(const char* value) { return PyUnicode_FromString(value); }

template<typename T>
PyObject* to_python(const std::vector<T>& vec) {
    PyObject* list = PyList_New(vec.size());
    if (!list) return nullptr;
    for (size_t i = 0; i < vec.size(); ++i) {
        PyObject* item = to_python(vec[i]);
        if (!item) { Py_DECREF(list); return nullptr; }
        PyList_SET_ITEM(list, i, item);
    }
    return list;
}

// Convert Python objects to C++ types
inline bool from_python(PyObject* obj, int& out) {
    if (!PyLong_Check(obj)) return false;
    out = static_cast<int>(PyLong_AsLong(obj));
    return !PyErr_Occurred();
}
inline bool from_python(PyObject* obj, long& out) {
    if (!PyLong_Check(obj)) return false;
    out = PyLong_AsLong(obj);
    return !PyErr_Occurred();
}
inline bool from_python(PyObject* obj, double& out) {
    if (PyFloat_Check(obj)) { out = PyFloat_AsDouble(obj); return true; }
    if (PyLong_Check(obj)) { out = static_cast<double>(PyLong_AsLong(obj)); return true; }
    return false;
}
inline bool from_python(PyObject* obj, float& out) {
    double d; if (!from_python(obj, d)) return false;
    out = static_cast<float>(d); return true;
}
inline bool from_python(PyObject* obj, bool& out) {
    out = PyObject_IsTrue(obj);
    return true;
}
inline bool from_python(PyObject* obj, std::string& out) {
    if (!PyUnicode_Check(obj)) return false;
    Py_ssize_t size;
    const char* data = PyUnicode_AsUTF8AndSize(obj, &size);
    if (!data) return false;
    out = std::string(data, size);
    return true;
}

template<typename T>
bool from_python(PyObject* obj, std::vector<T>& out) {
    if (!PyList_Check(obj)) return false;
    Py_ssize_t size = PyList_Size(obj);
    out.clear();
    out.reserve(size);
    for (Py_ssize_t i = 0; i < size; ++i) {
        T item;
        if (!from_python(PyList_GetItem(obj, i), item)) return false;
        out.push_back(std::move(item));
    }
    return true;
}

} // namespace mirror_bridge_conv

)";
}

// ============================================================================
// Code generation for a single class
// ============================================================================

inline std::string sanitize_type(const std::string& type) {
    // Remove const& for cleaner variable declarations
    std::string result = type;
    if (result.find("const ") == 0) {
        result = result.substr(6);
    }
    size_t amp = result.find('&');
    if (amp != std::string::npos) {
        result = result.substr(0, amp);
    }
    // Trim whitespace
    while (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

inline void generate_class_code(const ClassBinding& binding) {
    const std::string& cls = binding.py_name;
    const std::string& cpp_cls = binding.cpp_name;

    // Include the original header (user must ensure it's available)
    std::cout << "// ============================================================================\n";
    std::cout << "// " << cls << " Binding\n";
    std::cout << "// ============================================================================\n\n";

    std::cout << "namespace " << cls << "_binding {\n\n";

    std::cout << "struct PyWrapper {\n";
    std::cout << "    PyObject_HEAD\n";
    std::cout << "    " << cpp_cls << "* cpp_object;\n";
    std::cout << "    bool owns;\n";
    std::cout << "};\n\n";

    // Dealloc
    std::cout << "static void py_dealloc(PyObject* self) {\n";
    std::cout << "    auto* wrapper = reinterpret_cast<PyWrapper*>(self);\n";
    std::cout << "    if (wrapper->owns && wrapper->cpp_object) {\n";
    std::cout << "        delete wrapper->cpp_object;\n";
    std::cout << "    }\n";
    std::cout << "    Py_TYPE(self)->tp_free(self);\n";
    std::cout << "}\n\n";

    // New
    std::cout << "static PyObject* py_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {\n";
    std::cout << "    auto* self = reinterpret_cast<PyWrapper*>(type->tp_alloc(type, 0));\n";
    std::cout << "    if (self) {\n";
    std::cout << "        self->cpp_object = nullptr;\n";
    std::cout << "        self->owns = false;\n";
    std::cout << "    }\n";
    std::cout << "    return reinterpret_cast<PyObject*>(self);\n";
    std::cout << "}\n\n";

    // Init with constructor support
    std::cout << "static int py_init(PyObject* self, PyObject* args, PyObject* kwds) {\n";
    std::cout << "    auto* wrapper = reinterpret_cast<PyWrapper*>(self);\n";
    std::cout << "    Py_ssize_t nargs = PyTuple_Size(args);\n\n";

    if (binding.has_default_ctor) {
        std::cout << "    if (nargs == 0) {\n";
        std::cout << "        wrapper->cpp_object = new " << cpp_cls << "();\n";
        std::cout << "        wrapper->owns = true;\n";
        std::cout << "        return 0;\n";
        std::cout << "    }\n\n";
    }

    // Generate constructor overloads
    for (const auto& ctor : binding.constructors) {
        std::cout << "    if (nargs == " << ctor.params.size() << ") {\n";

        // Declare variables
        for (size_t i = 0; i < ctor.params.size(); ++i) {
            std::string clean_type = sanitize_type(ctor.params[i].second);
            std::cout << "        " << clean_type << " arg" << i << ";\n";
        }

        // Parse arguments
        std::cout << "        bool ok = true;\n";
        for (size_t i = 0; i < ctor.params.size(); ++i) {
            std::cout << "        if (ok) ok = mirror_bridge_conv::from_python(PyTuple_GET_ITEM(args, " << i << "), arg" << i << ");\n";
        }

        std::cout << "        if (ok) {\n";
        std::cout << "            wrapper->cpp_object = new " << cpp_cls << "(";
        for (size_t i = 0; i < ctor.params.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << "arg" << i;
        }
        std::cout << ");\n";
        std::cout << "            wrapper->owns = true;\n";
        std::cout << "            return 0;\n";
        std::cout << "        }\n";
        std::cout << "    }\n\n";
    }

    std::cout << "    PyErr_SetString(PyExc_TypeError, \"No matching constructor\");\n";
    std::cout << "    return -1;\n";
    std::cout << "}\n\n";

    // Generate getters/setters for members
    for (const auto& member : binding.members) {
        // Getter
        std::cout << "static PyObject* get_" << member.name << "(PyObject* self, void*) {\n";
        std::cout << "    auto* wrapper = reinterpret_cast<PyWrapper*>(self);\n";
        std::cout << "    if (!wrapper->cpp_object) {\n";
        std::cout << "        PyErr_SetString(PyExc_RuntimeError, \"Invalid C++ object\");\n";
        std::cout << "        return nullptr;\n";
        std::cout << "    }\n";
        std::cout << "    return mirror_bridge_conv::to_python(wrapper->cpp_object->" << member.name << ");\n";
        std::cout << "}\n\n";

        // Setter (if not const)
        if (!member.is_const) {
            std::cout << "static int set_" << member.name << "(PyObject* self, PyObject* value, void*) {\n";
            std::cout << "    auto* wrapper = reinterpret_cast<PyWrapper*>(self);\n";
            std::cout << "    if (!wrapper->cpp_object) {\n";
            std::cout << "        PyErr_SetString(PyExc_RuntimeError, \"Invalid C++ object\");\n";
            std::cout << "        return -1;\n";
            std::cout << "    }\n";
            std::cout << "    if (!value) {\n";
            std::cout << "        PyErr_SetString(PyExc_TypeError, \"Cannot delete attribute\");\n";
            std::cout << "        return -1;\n";
            std::cout << "    }\n";
            std::string clean_type = sanitize_type(member.type);
            std::cout << "    " << clean_type << " cpp_value;\n";
            std::cout << "    if (!mirror_bridge_conv::from_python(value, cpp_value)) {\n";
            std::cout << "        PyErr_SetString(PyExc_TypeError, \"Type conversion failed\");\n";
            std::cout << "        return -1;\n";
            std::cout << "    }\n";
            std::cout << "    wrapper->cpp_object->" << member.name << " = cpp_value;\n";
            std::cout << "    return 0;\n";
            std::cout << "}\n\n";
        }
    }

    // Generate methods
    for (const auto& method : binding.methods) {
        if (method.is_static) {
            std::cout << "static PyObject* method_" << method.name << "(PyObject*, PyObject* args) {\n";
        } else {
            std::cout << "static PyObject* method_" << method.name << "(PyObject* self, PyObject* args) {\n";
            std::cout << "    auto* wrapper = reinterpret_cast<PyWrapper*>(self);\n";
            std::cout << "    if (!wrapper->cpp_object) {\n";
            std::cout << "        PyErr_SetString(PyExc_RuntimeError, \"Invalid C++ object\");\n";
            std::cout << "        return nullptr;\n";
            std::cout << "    }\n";
        }

        // Parse parameters
        if (!method.params.empty()) {
            std::cout << "    if (PyTuple_Size(args) != " << method.params.size() << ") {\n";
            std::cout << "        PyErr_SetString(PyExc_TypeError, \"Wrong number of arguments\");\n";
            std::cout << "        return nullptr;\n";
            std::cout << "    }\n";

            for (size_t i = 0; i < method.params.size(); ++i) {
                std::string clean_type = sanitize_type(method.params[i].second);
                std::cout << "    " << clean_type << " arg" << i << ";\n";
            }

            std::cout << "    bool ok = true;\n";
            for (size_t i = 0; i < method.params.size(); ++i) {
                std::cout << "    if (ok) ok = mirror_bridge_conv::from_python(PyTuple_GET_ITEM(args, " << i << "), arg" << i << ");\n";
            }
            std::cout << "    if (!ok) {\n";
            std::cout << "        PyErr_SetString(PyExc_TypeError, \"Argument conversion failed\");\n";
            std::cout << "        return nullptr;\n";
            std::cout << "    }\n";
        }

        // Call method
        std::cout << "    try {\n";
        if (method.is_void) {
            if (method.is_static) {
                std::cout << "        " << cpp_cls << "::" << method.name << "(";
            } else {
                std::cout << "        wrapper->cpp_object->" << method.name << "(";
            }
            for (size_t i = 0; i < method.params.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << "arg" << i;
            }
            std::cout << ");\n";
            std::cout << "        Py_RETURN_NONE;\n";
        } else {
            if (method.is_static) {
                std::cout << "        auto result = " << cpp_cls << "::" << method.name << "(";
            } else {
                std::cout << "        auto result = wrapper->cpp_object->" << method.name << "(";
            }
            for (size_t i = 0; i < method.params.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << "arg" << i;
            }
            std::cout << ");\n";
            std::cout << "        return mirror_bridge_conv::to_python(result);\n";
        }
        std::cout << "    } catch (const std::exception& e) {\n";
        std::cout << "        PyErr_SetString(PyExc_RuntimeError, e.what());\n";
        std::cout << "        return nullptr;\n";
        std::cout << "    }\n";
        std::cout << "}\n\n";
    }

    // GetSetDef array
    std::cout << "static PyGetSetDef getset[] = {\n";
    for (const auto& member : binding.members) {
        std::cout << "    {const_cast<char*>(\"" << member.name << "\"), get_" << member.name << ", ";
        if (member.is_const) {
            std::cout << "nullptr";
        } else {
            std::cout << "set_" << member.name;
        }
        std::cout << ", nullptr, nullptr},\n";
    }
    std::cout << "    {nullptr, nullptr, nullptr, nullptr, nullptr}\n";
    std::cout << "};\n\n";

    // MethodDef array
    std::cout << "static PyMethodDef methods[] = {\n";
    for (const auto& method : binding.methods) {
        std::cout << "    {\"" << method.name << "\", method_" << method.name << ", METH_VARARGS, nullptr},\n";
    }
    std::cout << "    {nullptr, nullptr, 0, nullptr}\n";
    std::cout << "};\n\n";

    // Repr function
    std::cout << "static PyObject* py_repr(PyObject* self) {\n";
    std::cout << "    auto* wrapper = reinterpret_cast<PyWrapper*>(self);\n";
    std::cout << "    char buffer[256];\n";
    std::cout << "    snprintf(buffer, sizeof(buffer), \"<" << cls << " object at %p>\", static_cast<void*>(wrapper->cpp_object));\n";
    std::cout << "    return PyUnicode_FromString(buffer);\n";
    std::cout << "}\n\n";

    // Type object
    std::cout << "static PyTypeObject type_object = {\n";
    std::cout << "    PyVarObject_HEAD_INIT(nullptr, 0)\n";
    std::cout << "    \"" << cls << "\",                         // tp_name\n";
    std::cout << "    sizeof(PyWrapper),                    // tp_basicsize\n";
    std::cout << "    0,                                    // tp_itemsize\n";
    std::cout << "    py_dealloc,                           // tp_dealloc\n";
    std::cout << "    0,                                    // tp_vectorcall_offset\n";
    std::cout << "    nullptr,                              // tp_getattr\n";
    std::cout << "    nullptr,                              // tp_setattr\n";
    std::cout << "    nullptr,                              // tp_as_async\n";
    std::cout << "    py_repr,                              // tp_repr\n";
    std::cout << "    nullptr,                              // tp_as_number\n";
    std::cout << "    nullptr,                              // tp_as_sequence\n";
    std::cout << "    nullptr,                              // tp_as_mapping\n";
    std::cout << "    nullptr,                              // tp_hash\n";
    std::cout << "    nullptr,                              // tp_call\n";
    std::cout << "    nullptr,                              // tp_str\n";
    std::cout << "    nullptr,                              // tp_getattro\n";
    std::cout << "    nullptr,                              // tp_setattro\n";
    std::cout << "    nullptr,                              // tp_as_buffer\n";
    std::cout << "    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, // tp_flags\n";
    std::cout << "    \"Auto-generated binding for " << cpp_cls << "\", // tp_doc\n";
    std::cout << "    nullptr,                              // tp_traverse\n";
    std::cout << "    nullptr,                              // tp_clear\n";
    std::cout << "    nullptr,                              // tp_richcompare\n";
    std::cout << "    0,                                    // tp_weaklistoffset\n";
    std::cout << "    nullptr,                              // tp_iter\n";
    std::cout << "    nullptr,                              // tp_iternext\n";
    std::cout << "    methods,                              // tp_methods\n";
    std::cout << "    nullptr,                              // tp_members\n";
    std::cout << "    getset,                               // tp_getset\n";
    std::cout << "    nullptr,                              // tp_base\n";
    std::cout << "    nullptr,                              // tp_dict\n";
    std::cout << "    nullptr,                              // tp_descr_get\n";
    std::cout << "    nullptr,                              // tp_descr_set\n";
    std::cout << "    0,                                    // tp_dictoffset\n";
    std::cout << "    py_init,                              // tp_init\n";
    std::cout << "    nullptr,                              // tp_alloc\n";
    std::cout << "    py_new,                               // tp_new\n";
    std::cout << "};\n\n";

    // Register function
    std::cout << "bool register_type(PyObject* module) {\n";
    std::cout << "    if (PyType_Ready(&type_object) < 0) return false;\n";
    std::cout << "    Py_INCREF(&type_object);\n";
    std::cout << "    if (PyModule_AddObject(module, \"" << cls << "\", reinterpret_cast<PyObject*>(&type_object)) < 0) {\n";
    std::cout << "        Py_DECREF(&type_object);\n";
    std::cout << "        return false;\n";
    std::cout << "    }\n";
    std::cout << "    return true;\n";
    std::cout << "}\n\n";

    std::cout << "} // namespace " << cls << "_binding\n\n";
}

// ============================================================================
// Module generation
// ============================================================================

template<typename T>
ClassBinding bind(const char* py_name) {
    return introspect_class<T>(py_name);
}

template<typename... Bindings>
void generate_module(const char* module_name, Bindings... bindings) {
    print_header();
    print_conversion_helpers();

    // Generate code for each class
    (generate_class_code(bindings), ...);

    // Generate module init function
    std::cout << "// ============================================================================\n";
    std::cout << "// Module Initialization\n";
    std::cout << "// ============================================================================\n\n";

    std::cout << "static PyModuleDef module_def = {\n";
    std::cout << "    PyModuleDef_HEAD_INIT,\n";
    std::cout << "    \"" << module_name << "\",\n";
    std::cout << "    \"Auto-generated module via Mirror Bridge C++17 codegen\",\n";
    std::cout << "    -1,\n";
    std::cout << "    nullptr\n";
    std::cout << "};\n\n";

    std::cout << "PyMODINIT_FUNC PyInit_" << module_name << "() {\n";
    std::cout << "    PyObject* m = PyModule_Create(&module_def);\n";
    std::cout << "    if (!m) return nullptr;\n\n";

    // Register each class - use fold expression with comma operator
    auto register_class = [](const ClassBinding& b) {
        std::cout << "    if (!" << b.py_name << "_binding::register_type(m)) return nullptr;\n";
    };
    (register_class(bindings), ...);

    std::cout << "\n    return m;\n";
    std::cout << "}\n";
}

} // namespace codegen
} // namespace mirror_bridge
