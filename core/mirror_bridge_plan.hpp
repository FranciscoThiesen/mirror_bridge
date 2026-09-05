#pragma once

// ============================================================================
// Mirror Bridge - Template Instantiation Planner
// ============================================================================
//
// The CLI cannot know which instantiations of `template <typename T> struct
// Vector3` a header wants bound; nothing in the source says so. This planner
// answers the question at compile time, from the header's own API surface,
// and the CLI (tools/mirror_bridge_templates.py) lets the compiler decide
// which candidates actually compile:
//
//   discover  (this header, consteval)   -> candidate instantiations
//   probe     (compiler as the oracle)   -> which candidates compile
//   repeat until the approved set stops growing, then emit an ordinary
//   binding file naming every approved instantiation
//
// Rules:
//   * an alias is a declaration: `using Vec3f = Vector3<float>` binds
//     Vector3<float> under the Python name Vec3f
//   * signature closure: every specialization a free function, a plain
//     class or an approved specialization mentions is a candidate
//   * function/member templates are instantiated over the "universe" (the
//     Python scalar baseline plus every type the header talks about) for the
//     parameter shape the text scan reported, capped, and left to the probe
//   * class templates nobody mentions get the Python scalar baseline
//
// Portability contract (verified on clang-p2996 and GCC 16):
//   * never complete a candidate specialization (clang instantiates every
//     member body when a specialization is reflected)
//   * never substitute/can_substitute a function template (GCC instantiates
//     the body while checking the declaration)
//   * only entities the probe already approved (input::approved[]) are
//     completed or substituted, which is what makes the loop converge
//
// The discovery translation unit defines the inputs BEFORE including this
// header:
//
//   namespace mirror_bridge::plan::input {
//       inline constexpr std::string_view sources[] = { "/abs/src", "geom.hpp" };
//       inline constexpr std::pair<std::string_view, std::string_view> kind_hints[] = {
//           {"Vector3", "T"}, {"Matrix", "TV"}, {"clamp", "T"} };
//       inline constexpr std::string_view approved[] = { "", "geom::Vector3<float>" };
//       inline constexpr std::size_t combination_cap = 64;
//   }
//   namespace mirror_bridge::plan::requested {   // --instantiate, optional
//       inline constexpr std::meta::info r0 = ^^geom::Vector3<short>;
//       inline constexpr std::meta::info r1 = std::meta::reflect_function(geom::clamp<float>);
//   }
//   #define MIRROR_BRIDGE_PLAN_NAMESPACES ^^::, ^^geom
//
// ============================================================================

#include <meta>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/mirror_bridge_spelling.hpp"

#ifndef MIRROR_BRIDGE_PLAN_NAMESPACES
#error "define MIRROR_BRIDGE_PLAN_NAMESPACES (and mirror_bridge::plan::input) before including mirror_bridge_plan.hpp"
#endif

namespace mirror_bridge {
namespace plan {

using namespace std::meta;
using namespace mirror_bridge::spelling;

// ------------------------------------------------------------------ inputs --

// The file name is spelled the way the include was resolved ("./inc/x.hpp",
// "inc/x.hpp", or absolute), so absolute sources match by prefix and
// relative ones (include spellings) by suffix.
consteval bool in_source(info r) {
    std::string_view f = source_location_of(r).file_name();
    for (std::string_view s : input::sources) {
        if (s.empty()) continue;
        if (s.front() == '/') {
            if (f.starts_with(s)) return true;
        } else if (f == s || f.ends_with(std::string("/") + std::string(s))) {
            return true;
        }
    }
    return false;
}

consteval bool approved(std::string_view spelling) {
    for (auto a : input::approved) if (a == spelling) return true;
    return false;
}

consteval std::string_view kind_hint(info tmpl) {
    for (auto [name, kinds] : input::kind_hints) if (name == identifier_of(tmpl)) return kinds;
    return "";
}

// ------------------------------------------------------------------- model --

struct Cand {
    std::string kind;      // class | function | member
    std::string py_name;   // alias name, or synthesized
    std::string spelling;  // geom::Vector3<float> | geom::clamp<double> | geom::Vector3<float>::cast<double>
    std::string args;      // "float" | "float, 3"
    std::string origin;
    std::string owner;     // qualified template name, or the owning specialization for members
};

struct Plan {
    std::vector<Cand> cands;
    std::vector<std::string> notes;
    std::vector<std::pair<std::string, std::string>> needs;   // instantiation -> specialization its signature mentions
    std::vector<info> universe;     // decayed types the header talks about
    std::vector<info> values;       // non-type template arguments the header used
    std::vector<info> seen_specs;   // specializations of our templates the surface mentions
    std::vector<std::pair<info, std::string>> alias_names;
    std::vector<std::pair<std::string, std::string>> unbindable;   // instantiation -> why no backend can bind it

    consteval bool has_cand(std::string_view spelling) const {
        for (auto& c : cands) if (c.spelling == spelling) return true;
        return false;
    }
    consteval void add_unique(std::vector<info>& v, info t) { for (info u : v) if (u == t) return; v.push_back(t); }
    consteval std::string name_for(info spec, info tmpl, const std::vector<info>& args) const {
        for (auto& [s, n] : alias_names) if (s == spec) return n;
        return synth_name(tmpl, args);
    }
};

consteval bool ours(info entity) { return in_source(entity); }

// Strip references/pointers/cv and walk into foreign templates' arguments
// (std::vector<Vec3f> contributes Vec3f, not std::vector). Every type that
// survives is a universe member and, if it is a specialization of one of our
// templates, a seed.
consteval void visit_type(Plan& p, info t, const std::string& origin, bool extend_universe = true) {
    t = remove_cvref(t);
    while (is_pointer_type(t)) t = remove_cvref(remove_pointer(t));
    t = dealias(t);
    if (t == ^^void) return;
    // Strings are scalars to Python; walking basic_string<char, ...> would
    // put `char` into the universe and give every function template a
    // char instantiation nobody asked for.
    if (t == dealias(^^std::string) || t == dealias(^^std::string_view)) {
        if (extend_universe && t == dealias(^^std::string)) p.add_unique(p.universe, t);
        return;
    }
    if (has_template_arguments(t)) {
        info tm = template_of(t);
        if (is_class_template(tm) && ours(tm)) {
            if (extend_universe) p.add_unique(p.universe, t);
            std::string sp = spell(t);
            if (!p.has_cand(sp)) {
                auto args = args_of(t);
                p.cands.push_back({"class", p.name_for(t, tm, args), sp, spell_args(args), origin, qualified(tm)});
                p.seen_specs.push_back(t);
                for (info a : args) if (is_value(a)) p.add_unique(p.values, a);
            }
        }
        for (info a : template_arguments_of(t)) if (is_type(a)) visit_type(p, a, origin, extend_universe);
        return;
    }
    if (extend_universe && (is_fundamental_type(t) || is_enum_type(t) || (is_class_type(t) && ours(t))))
        p.add_unique(p.universe, t);
}

// A return type must convert (its specializations are seeded) but says
// nothing about what the header accepts, so it never extends the universe:
// `size_t size() const` should not instantiate every function template for
// unsigned long.
consteval void visit_function(Plan& p, info f, const std::string& origin, bool extend_universe = true) {
    visit_type(p, return_type_of(f), origin, /*extend_universe=*/false);
    for (info prm : parameters_of(f)) visit_type(p, type_of(prm), origin, extend_universe);
}

// Specializations of our templates that a signature mentions (through
// references, pointers and foreign templates such as std::vector<T>).
consteval void collect_specs(info t, std::vector<std::string>& out) {
    t = remove_cvref(t);
    while (is_pointer_type(t)) t = remove_cvref(remove_pointer(t));
    t = dealias(t);
    if (!has_template_arguments(t)) return;
    if (is_class_template(template_of(t)) && ours(template_of(t))) out.push_back(spell(t));
    for (info a : template_arguments_of(t)) if (is_type(a)) collect_specs(a, out);
}

consteval std::vector<std::string> needs_of(info f) {
    std::vector<std::string> specs;
    collect_specs(return_type_of(f), specs);
    for (info prm : parameters_of(f)) collect_specs(type_of(prm), specs);
    return specs;
}

consteval bool plain_method(info f) {
    return is_function(f) && !is_function_template(f) && has_identifier(f) && !is_constructor(f) && !is_destructor(f);
}

// Candidate argument lists for a template of the given kinds ("T" = one type
// parameter, "TT" = two, "TV" = type + value).
consteval std::vector<std::vector<info>> arg_lists(const std::vector<info>& universe, const std::vector<info>& values, std::string_view kinds) {
    std::vector<std::vector<info>> lists = {{}};
    for (char k : kinds) {
        std::vector<std::vector<info>> next;
        const std::vector<info>& pool = (k == 'V') ? values : universe;
        for (auto& l : lists) for (info a : pool) { auto n = l; n.push_back(a); next.push_back(n); }
        lists = next;
    }
    return lists;
}

// What Python can hand a C++ template without any hint from the header.
consteval std::vector<info> scalar_baseline() {
    std::vector<info> v;
    for (info t : {^^bool, ^^long, ^^double, ^^std::string}) v.push_back(dealias(t));
    return v;
}

consteval std::string spec_spelling(info tmpl, const std::vector<info>& args, const std::string& owner_spelling) {
    std::string qname = owner_spelling.empty() ? qualified(tmpl) : owner_spelling + "::" + std::string(identifier_of(tmpl));
    return qname + "<" + spell_args(args) + ">";
}

// Record an approved function instantiation's signature: what it returns and
// takes is seeded (so results convert) without widening the universe, which
// would feed on itself (cast<Vector3<bool>> -> Vector3<Vector3<bool>> -> ...).
consteval void note_signature(Plan& p, info inst, const std::string& sp) {
    visit_function(p, inst, "signature of " + sp, /*extend_universe=*/false);
    for (auto& needed : needs_of(inst)) p.needs.push_back({sp, needed});
    // The one thing about bindability that is knowable without a backend:
    // a raw pointer parameter has no Python value to receive (const char*
    // being the exception every backend converts). Flagging it here keeps
    // deref<T> for sixteen Ts out of the module instead of sixteen
    // "skipped" lines at import.
    for (info prm : parameters_of(inst)) {
        info t = remove_cvref(type_of(prm));
        if (is_pointer_type(t) && t != ^^const char*) {
            p.unbindable.push_back({sp, "parameter '" + spell(type_of(prm)) + "' is a raw pointer (only const char* converts)"});
            return;
        }
    }
}

// Function templates (free or member) are instantiated over the universe and
// left to the probe. Approved instantiations from an earlier round may be
// substituted here: their signatures extend the closure.
consteval void plan_function_template(Plan& p, info ft, const std::string& owner_spelling) {
    std::string_view kinds = kind_hint(ft);
    std::string qname = owner_spelling.empty() ? qualified(ft) : owner_spelling + "::" + std::string(identifier_of(ft));
    if (kinds.empty()) { p.notes.push_back("skip " + qname + ": no template-parameter hint"); return; }
    for (char k : kinds) {
        if (k != 'T' && k != 'V') {
            p.notes.push_back("skip " + qname + ": template parameter list has a "
                              + std::string(k == 'P' ? "pack" : k == 'D' ? "default" : "template template parameter")
                              + " (pass --instantiate to bind specific instantiations)");
            return;
        }
    }
    auto lists = arg_lists(p.universe, p.values, kinds);
    if (lists.size() > input::combination_cap) {
        p.notes.push_back("restrict " + qname + ": " + itoa(lists.size()) + " combinations exceed the cap of "
                          + itoa(input::combination_cap) + ", keeping Python scalars only");
        lists = arg_lists(scalar_baseline(), p.values, kinds);
    }
    for (auto& args : lists) {
        std::string sp = spec_spelling(ft, args, owner_spelling);
        if (p.has_cand(sp)) continue;
        p.cands.push_back({owner_spelling.empty() ? "function" : "member", std::string(identifier_of(ft)), sp, spell_args(args),
                           "universe(" + std::string(kinds) + ")", qname});
        if (approved(sp)) note_signature(p, substitute(ft, args), sp);
    }
}

struct Inventory { std::vector<info> fn_templates, class_templates, plain_classes, aliases, functions; };

// One namespace, no recursion: the driver lists every namespace it wants
// scanned (nested ones included) and leaves out detail/impl/std.
consteval void inventory_namespace(Inventory& inv, info ns) {
    for (info m : members_of(ns, access_context::unchecked())) {
        if (is_namespace(m) || !in_source(m)) continue;
        if (is_type_alias(m)) inv.aliases.push_back(m);
        else if (is_class_template(m)) inv.class_templates.push_back(m);
        else if (is_function_template(m)) inv.fn_templates.push_back(m);
        else if (is_function(m)) inv.functions.push_back(m);
        else if (is_type(m) && is_class_type(m) && !has_template_arguments(m) && is_complete_type(m)) inv.plain_classes.push_back(m);
    }
}

consteval Inventory inventory() {
    Inventory inv;
    for (info ns : {MIRROR_BRIDGE_PLAN_NAMESPACES}) inventory_namespace(inv, ns);
    return inv;
}

// --instantiate: the user named specializations explicitly. They enter the
// plan as seeds (classes) or candidates (functions) with origin "requested".
consteval void add_requested(Plan& p) {
#ifdef MIRROR_BRIDGE_PLAN_HAS_REQUESTED
    for (info v : members_of(^^requested, access_context::unchecked())) {
        if (!is_variable(v)) continue;
        info r = extract<info>(v);
        if (is_type(r)) {
            visit_type(p, r, "requested");
        } else if (is_function(r) && has_template_arguments(r)) {
            info tmpl = template_of(r);
            auto args = args_of(r);
            std::string sp = qualified(tmpl) + "<" + spell_args(args) + ">";
            if (p.has_cand(sp)) continue;
            p.cands.push_back({"function", std::string(identifier_of(tmpl)), sp, spell_args(args), "requested", qualified(tmpl)});
            if (approved(sp)) note_signature(p, r, sp);
        }
    }
#else
    (void)p;
#endif
}

consteval Plan make_plan() {
    Plan p;
    Inventory inv = inventory();
    for (info t : scalar_baseline()) p.add_unique(p.universe, t);

    // 1. Aliases are declarations: the alias name is the Python name.
    for (info a : inv.aliases) {
        info t = dealias(a);
        if (has_template_arguments(t) && is_class_template(template_of(t)) && ours(template_of(t)))
            p.alias_names.push_back({t, std::string(identifier_of(a))});
    }
    for (info a : inv.aliases) visit_type(p, a, "alias " + std::string(identifier_of(a)));
    add_requested(p);

    // 2. Signature closure: free functions, then plain classes (safe to complete).
    for (info f : inv.functions) visit_function(p, f, "signature of " + std::string(identifier_of(f)));
    for (info c : inv.plain_classes) {
        p.add_unique(p.universe, c);
        std::string origin = "member of " + std::string(identifier_of(c));
        for (info f : nonstatic_data_members_of(c, access_context::unchecked())) visit_type(p, type_of(f), origin);
        for (info f : members_of(c, access_context::unchecked())) if (plain_method(f)) visit_function(p, f, origin);
    }

    // 3. Class templates that nothing above names (no alias, no signature, no
    //    plain-class member) get the Python scalar baseline. Deciding this
    //    before step 4 keeps the decision the same in every round; a
    //    --instantiate request is not a mention, it adds to the baseline.
    for (info ct : inv.class_templates) {
        bool mentioned = false;
        for (auto& c : p.cands) if (c.kind == "class" && c.owner == qualified(ct) && c.origin != "requested") mentioned = true;
        if (mentioned) continue;
        std::vector<info> probe;
        for (int n = 0; n < 4 && !can_substitute(ct, probe); ++n) probe.push_back(^^long);
        if (!can_substitute(ct, probe) || probe.size() != 1) {
            p.notes.push_back("skip " + qualified(ct) + ": nothing in the header names an instantiation and it is not a "
                              "single-type-parameter template (pass --instantiate to bind specific instantiations)");
            continue;
        }
        for (info t : scalar_baseline()) {
            info spec = substitute(ct, {t});
            p.cands.push_back({"class", synth_name(ct, {t}), spell(spec), spell_arg(t), "baseline", qualified(ct)});
            p.seen_specs.push_back(spec);
        }
    }

    // 4. Specializations approved by the probe may be completed: their members
    //    extend the closure and their member templates get planned.
    for (info s : std::vector<info>(p.seen_specs)) {
        if (!approved(spell(s))) continue;
        std::string origin = "member of " + spell(s);
        for (info f : nonstatic_data_members_of(s, access_context::unchecked())) visit_type(p, type_of(f), origin);
        for (info f : members_of(s, access_context::unchecked())) if (plain_method(f)) visit_function(p, f, origin);
    }

    // 5. Function templates: universe x hinted parameter kinds, verified by the probe.
    for (info ft : inv.fn_templates) plan_function_template(p, ft, "");
    for (info c : inv.plain_classes)
        for (info f : members_of(c, access_context::unchecked()))
            if (is_function_template(f)) plan_function_template(p, f, spell(c));
    for (info s : std::vector<info>(p.seen_specs)) {
        if (!approved(spell(s))) continue;
        for (info f : members_of(s, access_context::unchecked()))
            if (is_function_template(f)) plan_function_template(p, f, spell(s));
    }
    return p;
}

// Tab-separated, one entry per line: kind, python name, C++ spelling,
// template arguments, origin, owner. The driver parses this.
consteval std::string render() {
    Plan p = make_plan();
    Inventory inv = inventory();
    std::string out;
    // The non-template surface: the driver binds free functions from it and
    // learns which names are templates (a plain function that shares a name
    // with a function template has no unique address to bind).
    for (info c : inv.plain_classes) {
        out += "plain\t" + std::string(identifier_of(c)) + "\t" + qualified(c) + "\t\t\t\n";
        std::vector<std::string> specs;
        for (info f : nonstatic_data_members_of(c, access_context::unchecked())) collect_specs(type_of(f), specs);
        for (info f : members_of(c, access_context::unchecked())) if (plain_method(f)) for (auto& n : needs_of(f)) specs.push_back(n);
        for (auto& n : specs) out += "needs\t" + qualified(c) + "\t" + n + "\t\t\t\n";
    }
    for (info f : inv.functions) {
        if (!has_identifier(f)) continue;   // operators
        out += "free\t" + std::string(identifier_of(f)) + "\t" + qualified(f) + "\t\t\t\n";
        for (auto& n : needs_of(f)) out += "needs\t" + qualified(f) + "\t" + n + "\t\t\t\n";
    }
    for (info ft : inv.fn_templates)
        out += "fntemplate\t" + std::string(identifier_of(ft)) + "\t" + qualified(ft) + "\t\t\t\n";
    for (auto& c : p.cands)
        out += c.kind + "\t" + c.py_name + "\t" + c.spelling + "\t" + c.args + "\t" + c.origin + "\t" + c.owner + "\n";
    for (auto& n : p.notes) out += "note\t\t\t\t" + n + "\t\n";
    for (auto& [fn, spec] : p.needs) out += "needs\t" + fn + "\t" + spec + "\t\t\t\n";
    for (auto& [fn, why] : p.unbindable) out += "unbindable\t\t" + fn + "\t\t" + why + "\t\n";
    out += "universe\t\t\t\t";
    for (info u : p.universe) out += spell(u) + " ";
    out += "\t\n";
    return out;
}

} // namespace plan
} // namespace mirror_bridge
