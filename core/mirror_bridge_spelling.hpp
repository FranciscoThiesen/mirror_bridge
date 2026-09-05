#pragma once

// ============================================================================
// Mirror Bridge - Reflection Spelling Helpers
// ============================================================================
//
// Shared by the template planner (core/mirror_bridge_plan.hpp, compile-time
// discovery run by the CLI) and the Python template runtime
// (python/mirror_bridge_templates.hpp): a stable, compiler-independent way to
// spell a type or a template argument list as C++ source, and to synthesize
// Python names for instantiations nobody aliased.
//
// display_string_of is deliberately avoided for anything that ends up in
// generated code or in a Python name: clang prints unqualified names and GCC
// prints "long int", so the two compilers would disagree on the identity of
// the same specialization.
//
// ============================================================================

#include <meta>
#include <string>
#include <string_view>
#include <vector>

namespace mirror_bridge {
namespace spelling {

using namespace std::meta;

// The fundamental types (plus the two string types the runtime treats as
// scalars), with their C++ spelling and the short name used in Python
// identifiers: Vector3<unsigned char> becomes Vector3_uint8.
struct Fundamental {
    info type;
    std::string_view spelling;
    std::string_view pretty;
};

consteval std::vector<Fundamental> fundamentals() {
    return {
        {^^bool, "bool", "bool"},
        {^^char, "char", "char"},
        {^^signed char, "signed char", "int8"},
        {^^unsigned char, "unsigned char", "uint8"},
        {^^short, "short", "short"},
        {^^unsigned short, "unsigned short", "ushort"},
        {^^int, "int", "int"},
        {^^unsigned, "unsigned", "uint"},
        {^^long, "long", "long"},
        {^^unsigned long, "unsigned long", "ulong"},
        {^^long long, "long long", "llong"},
        {^^unsigned long long, "unsigned long long", "ullong"},
        {^^float, "float", "float"},
        {^^double, "double", "double"},
        {^^long double, "long double", "ldouble"},
        {^^char8_t, "char8_t", "char8"},
        {^^char16_t, "char16_t", "char16"},
        {^^char32_t, "char32_t", "char32"},
        {^^wchar_t, "wchar_t", "wchar"},
        {^^void, "void", "void"},
        {^^std::string, "std::string", "string"},
        {^^std::string_view, "std::string_view", "string_view"},
    };
}

consteval std::string itoa(std::size_t n) {
    std::string s;
    do {
        s.insert(s.begin(), char('0' + n % 10));
        n /= 10;
    } while (n);
    return s;
}

consteval std::string spell(info t);

// Qualified name of a named entity: walks parent_of through namespaces and
// enclosing classes (a member of a specialization is spelled through the
// specialization, e.g. geom::Vector3<float>::cast).
consteval std::string qualified(info entity) {
    std::string name(identifier_of(entity));
    info p = parent_of(entity);
    while (true) {
        if (is_namespace(p)) {
            if (!has_identifier(p)) break;   // global (or anonymous) namespace
            name = std::string(identifier_of(p)) + "::" + name;
            p = parent_of(p);
        } else if (is_type(p)) {
            name = spell(p) + "::" + name;
            break;
        } else {
            break;
        }
    }
    return name;
}

// A template argument: a type, or a value such as "3" for Matrix<float, 3>.
consteval std::string spell_arg(info a) {
    return is_type(a) ? spell(a) : std::string(display_string_of(a));
}

consteval std::string spell_args(const std::vector<info>& args) {
    std::string s;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i) s += ", ";
        s += spell_arg(args[i]);
    }
    return s;
}

// template_arguments_of returns a span; the planner wants to append to it.
consteval std::vector<info> args_of(info spec) {
    std::vector<info> v;
    for (info a : template_arguments_of(spec)) v.push_back(a);
    return v;
}

// C++ source spelling of a type, valid in any scope: fully qualified, aliases
// resolved, template arguments spelled recursively.
consteval std::string spell(info t) {
    if (is_lvalue_reference_type(t)) return spell(remove_reference(t)) + "&";
    if (is_rvalue_reference_type(t)) return spell(remove_reference(t)) + "&&";
    if (is_pointer_type(t))          return spell(remove_pointer(t)) + "*";
    if (is_const(t))                 return spell(remove_const(t)) + " const";
    if (is_volatile(t))              return spell(remove_volatile(t)) + " volatile";
    t = dealias(t);
    for (auto f : fundamentals()) {
        if (t == dealias(f.type)) return std::string(f.spelling);
    }
    if (has_template_arguments(t)) return qualified(template_of(t)) + "<" + spell_args(args_of(t)) + ">";
    if (has_identifier(t))         return qualified(t);
    return std::string(display_string_of(t));
}

// Python-facing identifier fragment for a type:
//   float -> float, unsigned char -> uint8, geom::Robot -> Robot,
//   Vector3<Vector3<unsigned>> -> Vector3_Vector3_uint
consteval std::string pretty(info t) {
    t = dealias(remove_cvref(t));
    for (auto f : fundamentals()) {
        if (t == dealias(f.type)) return std::string(f.pretty);
    }
    std::string s = has_template_arguments(t) ? std::string(identifier_of(template_of(t)))
                  : has_identifier(t)         ? std::string(identifier_of(t))
                                              : std::string("anon");
    if (has_template_arguments(t)) {
        for (info a : template_arguments_of(t)) {
            s += "_";
            s += is_type(a) ? pretty(a) : std::string(display_string_of(a));
        }
    }
    return s;
}

// Python name for an instantiation nobody aliased:
//   Vector3<float> -> Vector3_float, Matrix<float, 3> -> Matrix_float_3,
//   Stack<std::string> -> Stack_string, Stack<geom::Robot> -> Stack_Robot
consteval std::string synth_name(info tmpl, const std::vector<info>& args) {
    std::string s(identifier_of(tmpl));
    for (info a : args) {
        s += "_";
        s += is_type(a) ? pretty(a) : std::string(display_string_of(a));
    }
    return s;
}

} // namespace spelling
} // namespace mirror_bridge
