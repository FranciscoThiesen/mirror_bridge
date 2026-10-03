#!/usr/bin/env python3
"""Compare two generated .pyi stubs and classify what changed.

The stub is the one artifact that cannot drift from the binding: it is
populated during `bind_class` from the same reflection data the binding is
generated from, and two runs of the same headers produce byte-identical
output (verified on clang-p2996 and on GCC 16, which also agree with each
other). That makes it usable as a committed description of the module's
Python surface — a lockfile — which is what lets a C++ team and a Python
team work from separate repositories without surprising each other.

This script answers the question a reviewer actually has: not "did any byte
change", which a plain diff answers and which fires on a reordered member,
but "will this break the people importing it".

  removed  a name Python code can no longer reach                BREAKING
  changed  a name whose signature or type is different           BREAKING
  added    a name that was not there before                      additive

Invoked by `mirror_bridge api`; usable directly for a one-off comparison.
"""
import argparse
import ast
import json
import sys


def _annotation(node):
    return ast.unparse(node) if node is not None else ""


def _signature(fn):
    """A function's parameters and return type, normalised to one line.

    Default VALUES are deliberately excluded: the stub prints them as `...`
    anyway, and a changed default is not a change to the surface a type
    checker sees.
    """
    a = fn.args
    parts = []
    for group, prefix in ((a.posonlyargs, ""), (a.args, "")):
        for arg in group:
            parts.append(f"{arg.arg}: {_annotation(arg.annotation)}" if arg.annotation else arg.arg)
    if a.vararg:
        parts.append("*" + a.vararg.arg)
    for arg in a.kwonlyargs:
        parts.append(f"{arg.arg}: {_annotation(arg.annotation)}" if arg.annotation else arg.arg)
    if a.kwarg:
        parts.append("**" + a.kwarg.arg)
    return f"({', '.join(parts)}) -> {_annotation(fn.returns) or 'None'}"


def surface(text, path):
    """Every importable name in a stub, as {qualified name: description}.

    Overloads share a name, so they are numbered in declaration order; the
    stub generator emits them deterministically, so the numbering is stable.
    """
    try:
        tree = ast.parse(text)
    except SyntaxError as e:
        raise SystemExit(f"error: {path} is not valid Python: {e}")

    names = {}

    def add(key, value):
        if key in names:            # an overload set
            n = 2
            while f"{key}#{n}" in names:
                n += 1
            key = f"{key}#{n}"
        names[key] = value

    def record_body(body, prefix):
        for node in body:
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                add(prefix + node.name, _signature(node))
            elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
                add(prefix + node.target.id, ": " + _annotation(node.annotation))
            elif isinstance(node, ast.ClassDef):
                add(prefix + node.name, "class")
                record_body(node.body, prefix + node.name + ".")

    record_body(tree.body, "")
    return names


def classify(before, after):
    removed, added, changed = {}, {}, {}
    for name, desc in before.items():
        if name not in after:
            removed[name] = desc
        elif after[name] != desc:
            changed[name] = (desc, after[name])
    for name, desc in after.items():
        if name not in before:
            added[name] = desc
    return removed, changed, added


def render(module, baseline_path, removed, changed, added, update_hint):
    out = []
    breaking = len(removed) + len(changed)
    if not breaking and not added:
        return f"API surface of '{module}' is unchanged (baseline {baseline_path})."

    out.append(f"API surface of '{module}' changed (baseline {baseline_path}):")
    if removed:
        out.append("")
        out.append("  removed — importers raise AttributeError:")
        for name in sorted(removed):
            out.append(f"      {name}")
    if changed:
        out.append("")
        out.append("  changed — importers may pass the wrong type:")
        for name in sorted(changed):
            was, now = changed[name]
            out.append(f"      {name}")
            out.append(f"          was: {was}")
            out.append(f"          now: {now}")
    if added:
        out.append("")
        out.append("  added — safe for existing importers:")
        for name in sorted(added):
            out.append(f"      {name}")

    out.append("")
    plural = "" if breaking == 1 else "s"
    out.append(f"  {breaking} breaking change{plural}, {len(added)} additive.")
    if update_hint:
        out.append(f"  Accept with: {update_hint}")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description="classify the difference between two .pyi stubs")
    ap.add_argument("--baseline", required=True, help="the committed stub")
    ap.add_argument("--current", required=True, help="the freshly generated stub")
    ap.add_argument("--module", default="", help="module name, for the report")
    ap.add_argument("--update-hint", default="", help="command that would accept the change")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    try:
        before = surface(open(args.baseline, encoding="utf-8").read(), args.baseline)
    except FileNotFoundError:
        print(f"error: no baseline at {args.baseline}", file=sys.stderr)
        return 2
    try:
        after = surface(open(args.current, encoding="utf-8").read(), args.current)
    except FileNotFoundError:
        print(f"error: no generated stub at {args.current}", file=sys.stderr)
        return 2

    removed, changed, added = classify(before, after)
    breaking = len(removed) + len(changed)

    if args.json:
        json.dump({
            "status": "ok",
            "module": args.module,
            "baseline": args.baseline,
            "breaking": breaking,
            "additive": len(added),
            "removed": sorted(removed),
            "changed": [{"name": n, "was": w, "now": c} for n, (w, c) in sorted(changed.items())],
            "added": sorted(added),
        }, sys.stdout)
        sys.stdout.write("\n")
    else:
        print(render(args.module or "module", args.baseline, removed, changed, added, args.update_hint))

    # 1 = breaking, 0 = unchanged or additive only. An additive change is
    # not a reason to fail a build: nobody's import breaks because a method
    # appeared.
    return 1 if breaking else 0


if __name__ == "__main__":
    sys.exit(main())
