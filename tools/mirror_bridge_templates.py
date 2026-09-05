#!/usr/bin/env python3
"""Template instantiation planner for `mirror_bridge generate` (Python backend).

A header never says which instantiations of its templates it wants bound, so
the CLI asks the compiler:

    discover   compile a consteval translation unit (core/mirror_bridge_plan.hpp)
               that reads the header's API surface through reflection and
               prints candidate instantiations: every specialization an alias,
               a signature or a plain class mentions, plus every function
               template applied to the types the header talks about
    probe      compile one line per candidate with -fsyntax-only; the compiler
               is the oracle for "does this instantiation exist"
    repeat     approved instantiations expose new signatures (member templates,
               return types), so discover again until nothing new appears
    emit       ordinary binding lines naming every approved instantiation, which
               `mirror_bridge generate` appends to the module it was already
               generating

Invoked by tools/mirror_bridge; run with --help for the flags.
"""
import argparse
import json
import os
import pathlib
import re
import shlex
import subprocess
import sys
import time

# ---------------------------------------------------------------- text scan --
# Reflection cannot see a template's parameter list (a P2996 gap), so the one
# fact taken from text is the *shape* of each parameter list:
#   T = type parameter, V = non-type parameter, D = defaulted, P = pack,
#   X = template template parameter
TEMPLATE_RE = re.compile(
    r"template\s*<((?:[^<>]|<[^<>]*>)*)>\s*"
    r"(?:(?:struct|class|union)\s+(?:\[\[[^\]]*\]\]\s*)?(\w+)"          # class template
    r"|(?:\[\[[^\]]*\]\]\s*)?[\w:<>,\s\*&]+?\b(\w+)\s*\()", re.S)        # function template

NONTYPE_RE = re.compile(
    r"(const\s+)?(auto|bool|char|short|int|long|unsigned|signed|std::size_t|size_t|"
    r"std::u?int\d+_t|u?int\d+_t|std::ptrdiff_t)\b")

# Namespaces nobody wants bound. `detail`-style names hide implementation;
# std is not ours.
PRIVATE_NS = {"std", "detail", "details", "internal", "impl", "__detail"}


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def kind_of(param):
    param = param.strip()
    if "..." in param:
        return "P"
    if re.match(r"template\s*<", param):
        return "X"
    if re.match(r"(typename|class)\b", param) or not NONTYPE_RE.match(param):
        # `typename T`, `class T`, `std::floating_point T`, `Concept T`
        return "D" if "=" in param else "T"
    return "V"


def scan_hints(text):
    hints = {}
    for m in TEMPLATE_RE.finditer(text):
        params, cls, fn = m.group(1), m.group(2), m.group(3)
        name = cls or fn
        if not name or not params.strip():   # `template <>` explicit specialization
            continue
        depth, cur, parts = 0, "", []
        for ch in params:
            if ch == "<":
                depth += 1
            if ch == ">":
                depth -= 1
            if ch == "," and depth == 0:
                parts.append(cur)
                cur = ""
            else:
                cur += ch
        parts.append(cur)
        hints[name] = "".join(kind_of(p) for p in parts)
    return hints


def scan_namespaces(text):
    """Fully-qualified namespaces the header opens, private ones excluded.

    Mirrors the brace-tracking scan `mirror_bridge generate` uses for its
    `using namespace` lines, so the planner and the module agree on scope."""
    found, depth, stack, i = [], 0, [], 0
    while i < len(text):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            while stack and stack[-1][1] >= depth:
                stack.pop()
        elif ch == "n":
            m = re.match(r"namespace\s+([a-zA-Z_][a-zA-Z0-9_:]*)\s*\{", text[i:])
            if m:
                stack.append((m.group(1), depth))
                fqn = "::".join(n for n, _ in stack)
                if fqn not in found:
                    found.append(fqn)
                depth += 1
                i += m.end()
                continue
        i += 1
    return [ns for ns in found if not any(part in PRIVATE_NS or part.startswith("_") for part in ns.split("::"))]


def declares_at_global_scope(text):
    """Whether anything bindable lives outside every namespace.

    Scanning the global namespace through reflection walks the whole standard
    library (seconds per round), so it is only done when the headers actually
    declare something there."""
    out, i, n = [], 0, len(text)
    while i < n:
        m = re.match(r"namespace\s+[a-zA-Z_][a-zA-Z0-9_:]*\s*\{", text[i:]) if text[i] == "n" else None
        if m:   # skip the whole block: it is not global scope
            i += m.end()
            d = 1
            while i < n and d:
                d += {"{": 1, "}": -1}.get(text[i], 0)
                i += 1
            continue
        out.append(text[i])
        i += 1
    rest = re.sub(r"^\s*#[^\n]*", "", "".join(out), flags=re.M)
    rest = re.sub(r"\busing\s+namespace\b[^;]*;", "", rest)
    return re.search(r"\b(template|struct|class|union|enum|using|inline|constexpr|static|auto|void|int|bool|float|double)\b",
                     rest) is not None


# ------------------------------------------------------------- compilation --

class Compiler:
    def __init__(self, args):
        self.binary = args.cxx
        self.is_clang = "clang" in os.path.basename(args.cxx)
        self.flags = shlex.split(args.cxx_flags)
        self.flags += ["-w", "-fconstexpr-steps=100000000" if self.is_clang else "-fconstexpr-ops-limit=1000000000"]
        # The probe needs every diagnostic: an error the compiler never printed
        # would leave a broken candidate approved.
        self.flags += ["-ferror-limit=0" if self.is_clang else "-fmax-errors=0"]
        self.includes = ["-I" + args.src_dir, "-I.", "-I" + args.project_root] + ["-I" + d for d in args.include_dir]
        if args.eigen_dir:
            self.includes.append("-I" + args.eigen_dir)
        self.src_dir = args.src_dir
        self.log = []

    def run(self, extra, cwd=None):
        cmd = [self.binary] + self.flags + self.includes + extra
        t0 = time.time()
        r = subprocess.run(cmd, capture_output=True, text=True, cwd=cwd or self.src_dir)
        self.log.append(f"$ {' '.join(shlex.quote(c) for c in cmd)}\n  -> rc={r.returncode} in {time.time() - t0:.1f}s")
        return r


def include_lines(headers):
    return "".join(f'#include "{h}"\n' for h in headers)


# ----------------------------------------------------------------- discover --

def write_inputs(work, headers, hints, approved, namespaces, cap, requested):
    lines = ["#pragma once", "#include <cstddef>", "#include <string_view>", "#include <utility>", "#include <meta>",
             "namespace mirror_bridge::plan::input {",
             "inline constexpr std::string_view sources[] = {"]
    lines += [f'    "{h}",' for h in headers]
    lines += ["};", "inline constexpr std::pair<std::string_view, std::string_view> kind_hints[] = {"]
    lines += [f'    {{"{k}", "{v}"}},' for k, v in hints.items()] or ['    {"", ""},']
    lines += ["};", "inline constexpr std::string_view approved[] = {", '    "",']
    lines += [f'    "{a}",' for a in sorted(approved)]
    lines += ["};", f"inline constexpr std::size_t combination_cap = {cap};", "}"]
    if requested:
        lines += ["#define MIRROR_BRIDGE_PLAN_HAS_REQUESTED 1", "namespace mirror_bridge::plan::requested {"]
        lines += [f"using namespace {ns};" for ns in namespaces if ns != "::"]
        lines += [f"inline constexpr std::meta::info r{i} = {expr};" for i, expr in enumerate(requested)]
        lines += ["}"]
    lines += ["#define MIRROR_BRIDGE_PLAN_NAMESPACES " + ", ".join("^^" + ns for ns in namespaces), ""]
    (work / "plan_inputs.hpp").write_text("\n".join(lines))


DISCOVER_MAIN = """
consteval std::size_t mb_plan_len() { return mirror_bridge::plan::render().size(); }
consteval auto mb_plan_text() {
    std::array<char, mb_plan_len() + 1> a{};
    std::string s = mirror_bridge::plan::render();
    for (std::size_t i = 0; i < s.size(); ++i) a[i] = s[i];
    return a;
}
constexpr auto mb_plan = mb_plan_text();
int main() { std::fputs(mb_plan.data(), stdout); }
"""


def discover(work, cc, headers):
    src = work / "discover.cpp"
    src.write_text("#include <array>\n#include <cstdio>\n#include <string>\n" + include_lines(headers)
                   + '#include "plan_inputs.hpp"\n#include "core/mirror_bridge_plan.hpp"\n' + DISCOVER_MAIN)
    exe = work / "discover"
    plan = {"cands": [], "notes": [], "universe": "", "needs": [], "plain": [], "free": [], "fntemplates": [],
            "unbindable": []}
    r = cc.run(["-I" + str(work), str(src), "-o", str(exe)])
    if r.returncode:
        return plan, r.stderr or "discover compile failed without diagnostics"
    out = subprocess.run([str(exe)], capture_output=True, text=True).stdout
    for line in out.splitlines():
        kind, py, spelling, args, origin, owner = line.split("\t")
        entry = dict(kind=kind, py=py, spelling=spelling, args=args, origin=origin, owner=owner)
        if kind == "note":
            plan["notes"].append(origin)
        elif kind == "universe":
            plan["universe"] = origin.strip()
        elif kind == "needs":
            plan["needs"].append((py, spelling))
        elif kind == "unbindable":
            plan["unbindable"].append((spelling, origin))
        elif kind == "plain":
            plan["plain"].append(entry)
        elif kind == "free":
            plan["free"].append(entry)
        elif kind == "fntemplate":
            plan["fntemplates"].append(entry)
        else:
            plan["cands"].append(entry)
    return plan, ""


# -------------------------------------------------------------------- probe --

def probe_line(i, c):
    if c["kind"] == "class":
        return f"template struct {c['spelling']};"
    # Taking the address odr-uses the specialization: the body is instantiated
    # and any error is attributed (via the note chain) to this line.
    return f"[[maybe_unused]] inline constexpr auto mb_probe_{i} = &{c['spelling']};"


# Only the instantiation chain attributes an error to a probe line. Other
# notes ("candidate constructor ... not viable") may point at an unrelated
# explicit instantiation and must not count against it.
CHAIN_RE = re.compile(r"probe\.cpp:(\d+)(?::\d+)?:\s+(?:note: )?(?:in instantiation of .*requested here"
                      r"|.*required from here|.*required by substitution|.*recursively required)")
ERR_RE = re.compile(r"^(.*?):(\d+):(?:\d+:)?\s*(?:fatal )?error: (.*)$")


def probe(work, cc, headers, cands):
    if not cands:
        return [], [], ""
    src = work / "probe.cpp"
    lines = [f'#include "{h}"' for h in headers]
    prologue = len(lines)
    lines += [probe_line(i, c) for i, c in enumerate(cands)]
    src.write_text("\n".join(lines) + "\n")
    r = cc.run(["-fsyntax-only", str(src)])
    # Attribute every diagnostic block to the probe line it mentions. A block
    # is an error plus the notes ("in instantiation of ... requested here" on
    # clang, "required from here" on GCC) that follow it.
    failed, reasons, current_err = set(), {}, None
    for raw in r.stderr.splitlines():
        m = ERR_RE.match(raw)
        if m:
            current_err = m.group(3)
            if m.group(1).endswith("probe.cpp"):
                failed.add(int(m.group(2)))
                reasons.setdefault(int(m.group(2)), current_err)
            continue
        m = CHAIN_RE.search(raw)
        if m and current_err:
            failed.add(int(m.group(1)))
            reasons.setdefault(int(m.group(1)), current_err)
    approved, rejected = [], []
    for i, c in enumerate(cands, start=prologue + 1):
        (rejected if i in failed else approved).append((c, reasons.get(i, "")))
    if r.returncode and not failed:
        # Errors that no probe line owns (a broken header, a missing include):
        # nothing can be trusted, so treat the round as failed.
        return [], [], r.stderr or "probe compile failed without diagnostics"
    return approved, [(c, friendly_reason(why)) for c, why in rejected], ""


# The raw diagnostic for a probe line that names no valid specialization is
# about the probe variable, not the template; say what it means instead.
UNFRIENDLY = [
    (re.compile(r"incompatible initializer of type '<overloaded function type>'"),
     "no such specialization (constraints not satisfied, or deduction failed)"),
    (re.compile(r"no matches converting function '(\w+)' to type"),
     r"no such specialization of '\1' (constraints not satisfied, or deduction failed)"),
    (re.compile(r"unable to deduce 'const auto' from '&\S+'"),
     "no such specialization (constraints not satisfied, or deduction failed)"),
]


def friendly_reason(why):
    for pattern, replacement in UNFRIENDLY:
        if pattern.search(why):
            return pattern.sub(replacement, why) if "\\1" in replacement else replacement
    return why


# ------------------------------------------------------------- --instantiate --

def classify_requested(work, cc, headers, namespaces, specs):
    """Turn --instantiate spellings into reflection expressions, compiler-checked.

    A class specialization reflects as ^^X; a function specialization must go
    through reflect_function, which odr-uses it, so its body is compiled here
    once and a broken request never reaches the discovery unit."""
    exprs, errors = [], []
    using = "".join(f"using namespace {ns};\n" for ns in namespaces if ns != "::")
    for spec in specs:
        chosen = None
        for expr, check in ((f"^^{spec}", f"static_assert(std::meta::is_type({{}}));"),
                            (f"std::meta::reflect_function({spec})", "")):
            src = work / "request.cpp"
            src.write_text(include_lines(headers) + "#include <meta>\n" + using
                           + f"inline constexpr std::meta::info r = {expr};\n" + check.format("r") + "\n")
            if cc.run(["-fsyntax-only", str(src)]).returncode == 0:
                chosen = expr
                break
        if chosen:
            exprs.append(chosen)
        else:
            errors.append(f"--instantiate '{spec}': not a class or function specialization the headers define "
                          f"(spell it as the header would, e.g. geom::Vector3<short> or geom::clamp<float>)")
    return exprs, errors


# --------------------------------------------------------------------- loop --

def plan_module(args, cc, work, headers, hints, namespaces, requested, log):
    approved_spellings, rejected_ever, unbindable_ever, rounds, seen_spellings = set(), {}, {}, [], set()
    plan, approved, rejected, unbindable, dropped = {"free": [], "needs": [], "cands": []}, [], [], [], []
    before = set()
    for rnd in range(1, max(1, args.max_rounds) + 1):
        write_inputs(work, headers, hints, approved_spellings, namespaces, args.template_cap, requested)
        plan, err = discover(work, cc, headers)
        if err:
            return None, "discovery failed:\n" + err
        approved, rejected, err = probe(work, cc, headers, plan["cands"])
        if err:
            return None, "probe failed:\n" + err
        # Verdicts are final: a spelling rejected in any round stays rejected
        # (the probe is deterministic; only the derived rejections below would
        # otherwise oscillate as the approved list changes between rounds).
        # An instantiation that compiles but whose signature no backend can
        # convert (the planner only sees these once it is approved and
        # substituted) is dropped the same way, under its own heading.
        for c, why in rejected:
            rejected_ever.setdefault(c["spelling"], why)
        for spelling, why in plan["unbindable"]:
            unbindable_ever.setdefault(spelling, why)
        for c, _ in approved:
            if c["spelling"] in rejected_ever:
                rejected.append((c, rejected_ever[c["spelling"]]))
        unbindable = [(c, unbindable_ever[c["spelling"]]) for c, _ in approved if c["spelling"] in unbindable_ever]
        approved = [(c, w) for c, w in approved
                    if c["spelling"] not in rejected_ever and c["spelling"] not in unbindable_ever]
        # A function is only as bindable as the specializations in its
        # signature: converting a result whose type does not compile would
        # complete that type inside the module and break the build.
        rejected_spellings = {c["spelling"] for c, _ in rejected}
        changed = True
        while changed:
            changed = False
            for fn, spec in plan["needs"]:
                if spec in rejected_spellings and fn not in rejected_spellings:
                    rejected_spellings.add(fn)
                    changed = True
                    for c, _ in approved:
                        if c["spelling"] == fn:
                            rejected.append((c, f"signature mentions {spec}, which does not compile"))
        approved = [(c, why) for c, why in approved if c["spelling"] not in rejected_spellings]
        for c, why in rejected:
            rejected_ever.setdefault(c["spelling"], why)
        dropped = []
        for e in plan["free"]:
            specs = [spec for fn, spec in plan["needs"] if fn == e["spelling"] and spec in rejected_spellings]
            if specs:
                dropped.append((e, specs))
        rounds.append((rnd, len(plan["cands"]), len(approved), len(rejected)))
        # Which candidates each round added explains the closure to a reader
        # of the report (and to whoever debugs a plan that keeps growing).
        new = [c["spelling"] for c in plan["cands"] if c["spelling"] not in seen_spellings]
        seen_spellings.update(new)
        log.append(f"round {rnd}: {len(plan['cands'])} candidates, {len(approved)} compile, {len(rejected)} rejected"
                   + (f"; new: {', '.join(new[:8])}{', ...' if len(new) > 8 else ''}" if rnd > 1 and new else ""))
        before = set(approved_spellings)
        approved_spellings = {c["spelling"] for c, _ in approved}
        if approved_spellings == before:
            break
    else:
        # A function approved only in the last round never had its signature
        # noted, so its `needs` were never checked against the probe; binding
        # it could complete an unprobed specialization inside the module.
        # Classes are safe either way: the probe instantiated them whole.
        unchecked = [(c, w) for c, w in approved if c["kind"] != "class" and c["spelling"] not in before]
        approved = [(c, w) for c, w in approved if c["kind"] == "class" or c["spelling"] in before]
        rejected += [(c, "compiles, but was approved in the final round and its signature was never checked")
                     for c, _ in unchecked]
        log.append(f"stopped after {args.max_rounds} rounds without converging; binding what compiled so far"
                   + (f" ({len(unchecked)} unchecked left out)" if unchecked else ""))
    dropped_names = {e["spelling"] for e, _ in dropped}
    return {"plan": plan, "approved": approved, "rejected": rejected, "unbindable": unbindable, "dropped": dropped,
            "rounds": rounds, "free": [e for e in plan["free"] if e["spelling"] not in dropped_names]}, ""


# --------------------------------------------------------------------- emit --

def free_functions_to_bind(result, notes):
    """Plain free functions with a unique address: not overloaded, not sharing
    a name with a function template (both need an overload set, phase C)."""
    counts = {}
    for e in result["free"]:
        counts[e["spelling"]] = counts.get(e["spelling"], 0) + 1
    template_names = {t["spelling"] for t in result["plan"]["fntemplates"]}
    out, seen = [], set()
    for e in result["free"]:
        if e["spelling"] in seen:
            continue
        seen.add(e["spelling"])
        if counts[e["spelling"]] > 1:
            notes.append(f"skip {e['spelling']}: overloaded ({counts[e['spelling']]} overloads); "
                         "overload sets are not bound yet")
        elif e["spelling"] in template_names:
            notes.append(f"skip plain {e['spelling']}: shares its name with a function template")
        else:
            out.append(e)
    return out


def emit_lines(args, result, bound_classes, notes):
    """Binding lines for the module: free functions, then instantiations."""
    lines = []
    gil = ".release_gil()" if args.release_gil else ""
    frees = free_functions_to_bind(result, notes)
    if frees:
        lines.append("    // Free functions (mirror_bridge_templates.py)")
        for e in frees:
            lines.append(f'    mirror_bridge::bind_function_when_bindable<&{e["spelling"]}>(m, "{e["py"]}");')
    approved = [c for c, _ in result["approved"]]
    members = {}
    for c in approved:
        # owner is the member template's qualified name: geom::Vector3<float>::cast
        if c["kind"] == "member":
            members.setdefault(c["owner"].rsplit("::", 1)[0], []).append(c)
    classes = [c for c in approved if c["kind"] == "class"]
    functions = [c for c in approved if c["kind"] == "function"]
    if classes or functions or members:
        lines.append(f"    // Template instantiations (plan: {args.report})")
    for c in classes:
        chain = "".join(f'\n        .member_template<"{mc["py"]}", {mc["args"]}>()' for mc in members.pop(c["spelling"], []))
        lines.append(f'    mirror_bridge::templates::bind_instance<{c["spelling"]}>(m, "{c["py"]}"){chain}{gil};')
    for c in functions:
        lines.append(f'    mirror_bridge::templates::bind_instance<&{c["spelling"]}>(m);')
    # Member templates of plain classes hang off the class the CLI bound; a
    # class the tokenizer skipped (MIRROR_BRIDGE_SKIP, private) has no type
    # object to attach them to.
    for owner, mcs in members.items():
        if owner.split("::")[-1] not in bound_classes and owner not in bound_classes:
            notes.append(f"skip member templates of {owner}: the class itself is not bound")
            continue
        for mc in mcs:
            lines.append(f'    mirror_bridge::templates::bind_member_template<{owner}, "{mc["py"]}", {mc["args"]}>();')
    return lines, frees


def write_report(path, args, cc, result, frees, notes, log, extra_headers):
    approved = [c for c, _ in result["approved"]]
    out = [f"mirror_bridge template plan for module '{args.module}'",
           f"compiler: {cc.binary} {' '.join(cc.flags)}",
           "rounds: " + ", ".join(f"#{r}: {n} candidates / {a} compile / {j} rejected" for r, n, a, j in result["rounds"]),
           ""]
    out.append(f"Bound instantiations ({len(approved)}):")
    for c in approved:
        out.append(f"  {c['kind']:9s} {c['py']:20s} {c['spelling']:56s} <- {c['origin']}")
    out.append("")
    out.append(f"Bound free functions ({len(frees)}): " + ", ".join(e["py"] for e in frees))
    if result["dropped"]:
        out.append("")
        out.append("Free functions skipped (their signatures mention specializations that do not compile):")
        for e, specs in result["dropped"]:
            out.append(f"  {e['spelling']}: {', '.join(specs)}")
    if result["rejected"]:
        out.append("")
        out.append(f"Rejected by the compiler ({len(result['rejected'])}):")
        for c, why in result["rejected"]:
            out.append(f"  {c['spelling']:56s} {why}")
    if result["unbindable"]:
        out.append("")
        out.append(f"Compile but cannot be bound ({len(result['unbindable'])}):")
        for c, why in result["unbindable"]:
            out.append(f"  {c['spelling']:56s} {why}")
    if notes:
        out.append("")
        out.append("Notes:")
        out += [f"  {n}" for n in notes]
    if extra_headers:
        out.append("")
        out.append("Headers included for their functions/templates: " + ", ".join(extra_headers))
    out.append("")
    out.append("Universe (types function templates were instantiated over): " + result["plan"]["universe"])
    out.append("")
    out.append("Log:")
    out += [f"  {l}" for l in log]
    pathlib.Path(path).write_text("\n".join(out) + "\n")


# --------------------------------------------------------------------- main --

def parse_args():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cxx", required=True, help="reflection-capable compiler binary")
    ap.add_argument("--cxx-flags", default="", help="reflection flags for that compiler")
    ap.add_argument("--project-root", required=True, help="mirror_bridge checkout (for core/mirror_bridge_plan.hpp)")
    ap.add_argument("--src-dir", required=True)
    ap.add_argument("-I", dest="include_dir", action="append", default=[])
    ap.add_argument("--eigen-dir", default="")
    ap.add_argument("--module", required=True)
    ap.add_argument("--work", required=True, help="scratch directory for the discover/probe units")
    ap.add_argument("--headers-file", required=True,
                    help="one header per line as '<include spelling>\\t<1 if the module already includes it>'")
    ap.add_argument("--classes-file", required=True, help="classes the module binds, one per line")
    ap.add_argument("--instantiate", action="append", default=[], help="bind this specialization (repeatable)")
    ap.add_argument("--template-cap", type=int, default=64,
                    help="max argument combinations per function template before falling back to scalars")
    ap.add_argument("--max-rounds", type=int, default=5)
    ap.add_argument("--release-gil", action="store_true")
    ap.add_argument("--emit", required=True, help="write the binding lines here")
    ap.add_argument("--includes-out", required=True, help="write extra headers the binding lines need here")
    ap.add_argument("--report", required=True, help="human-readable plan")
    ap.add_argument("--json-out", default="")
    ap.add_argument("--verbose", action="store_true")
    return ap.parse_args()


def main():
    args = parse_args()
    work = pathlib.Path(args.work)
    work.mkdir(parents=True, exist_ok=True)
    cc = Compiler(args)
    log = cc.log

    headers, included = [], set()
    for line in pathlib.Path(args.headers_file).read_text().splitlines():
        if not line.strip():
            continue
        rel, flag = (line.split("\t") + ["1"])[:2]
        headers.append(rel)
        if flag.strip() == "1":
            included.add(rel)
    bound_classes = {l.strip() for l in pathlib.Path(args.classes_file).read_text().splitlines() if l.strip()}

    text = ""
    for h in headers:
        for base in [args.src_dir] + args.include_dir:
            p = pathlib.Path(base) / h
            if p.is_file():
                text += strip_comments(p.read_text(encoding="utf-8", errors="ignore")) + "\n"
                break
    hints = scan_hints(text)
    namespaces = scan_namespaces(text)
    if declares_at_global_scope(text) or not namespaces:
        namespaces.append("::")
    log.append(f"scopes: {namespaces}")
    log.append(f"template parameter hints: {hints}")

    notes = []
    requested, errors = classify_requested(work, cc, headers, namespaces, args.instantiate)
    for e in errors:
        print(f"  warning: {e}", file=sys.stderr)
        notes.append(e)

    result, err = plan_module(args, cc, work, headers, hints, namespaces, requested, log)
    if result is None and len(headers) > len(included):
        # A header without classes may not be self-contained; retry with the
        # ones the module already includes.
        log.append("retrying with the module's own headers only")
        headers = [h for h in headers if h in included]
        result, err = plan_module(args, cc, work, headers, hints, namespaces, requested, log)
    if result is None:
        print(f"  templates: {err.strip().splitlines()[0] if err.strip() else 'planning failed'}", file=sys.stderr)
        (work / "planner.log").write_text("\n".join(log) + "\n\n" + err)
        print(f"  templates: skipped (details: {work / 'planner.log'})", file=sys.stderr)
        return 2

    notes = result["plan"]["notes"] + notes
    lines, frees = emit_lines(args, result, bound_classes, notes)
    pathlib.Path(args.emit).write_text("\n".join(lines) + ("\n" if lines else ""))
    extra_headers = [h for h in headers if h not in included]
    pathlib.Path(args.includes_out).write_text("".join(h + "\n" for h in extra_headers) if lines else "")
    write_report(args.report, args, cc, result, frees, notes, log, extra_headers if lines else [])

    approved = [c for c, _ in result["approved"]]
    families = sorted({c["owner"] for c in approved})
    if args.json_out:
        json.dump({
            "functions": [e["py"] for e in frees],
            "instantiations": [{"kind": c["kind"], "template": c["owner"], "args": c["args"],
                                "cpp": c["spelling"], "python": c["py"], "origin": c["origin"]} for c in approved],
            "rejected": [{"cpp": c["spelling"], "reason": why} for c, why in result["rejected"]],
            "unbindable": [{"cpp": c["spelling"], "reason": why} for c, why in result["unbindable"]],
            "skipped_functions": [{"cpp": e["spelling"], "needs": specs} for e, specs in result["dropped"]],
            "notes": notes,
            "rounds": len(result["rounds"]),
            "report": args.report,
        }, open(args.json_out, "w"))   # one line: the CLI splices it into its own JSON object

    n_class = sum(1 for c in approved if c["kind"] == "class")
    n_fn = sum(1 for c in approved if c["kind"] != "class")
    summary = []
    if n_class or n_fn:
        summary.append(f"{n_class} class and {n_fn} function instantiations across {len(families)} templates")
    if frees:
        summary.append(f"{len(frees)} free functions")
    if result["rejected"]:
        summary.append(f"{len(result['rejected'])} candidates rejected by the compiler")
    if result["unbindable"]:
        summary.append(f"{len(result['unbindable'])} compile but cannot be bound")
    print("  templates: " + (", ".join(summary) if summary else "nothing to bind") + f" (plan: {args.report})",
          file=sys.stderr)
    if args.verbose:
        for l in log:
            print("    " + l, file=sys.stderr)
    for n in notes:
        print(f"    note: {n}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
