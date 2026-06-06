#!/usr/bin/env python3
"""Render runtime benchmark JSON as the living benchmarks markdown page.

Usage: format_results.py <runtime_results.json> [--commit SHA] [--env DESC]

Emits markdown on stdout. Ratios against pybind11 are the headline numbers:
absolute ns/op varies with the machine, but same-machine same-flags ratios
are stable, which is what makes a CI-regenerated page honest.
"""

import argparse
import datetime
import json
import sys

# Display order + labels for the operations measured by
# runtime/python/run_runtime_benchmarks.py
OPERATIONS = [
    ("null_call", "Null call (method dispatch)"),
    ("arithmetic_int", "Arithmetic: add_int(42)"),
    ("arithmetic_double", "Arithmetic: add_double(3.14)"),
    ("string_concat", "String concat"),
    ("string_get", "String return"),
    ("vector_sum", "Vector sum (list in)"),
    ("vector_get", "Vector return"),
    ("attribute_get", "Attribute read"),
    ("attribute_set", "Attribute write"),
    ("construction", "Object construction"),
]

FRAMEWORKS = ["mirror_bridge", "pybind11", "nanobind", "swig", "boost_python"]


def fmt_ns(v):
    return f"{v:,.0f} ns" if v >= 100 else f"{v:.1f} ns"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("results")
    ap.add_argument("--commit", default="unknown")
    ap.add_argument("--env", default="GitHub Actions ubuntu runner (containerized)")
    ap.add_argument(
        "--fragment", action="store_true",
        help="omit the H1 so the output can be spliced between the "
             "RUNTIME markers in docs/internals/benchmarks.md",
    )
    args = ap.parse_args()

    with open(args.results) as f:
        data = json.load(f)

    # Frameworks that actually ran (boost_python reports zeros when its
    # binding wasn't built; an all-zero column is noise, not data).
    present = [
        fw for fw in FRAMEWORKS
        if fw in data and any(v > 0 for v in data[fw].values())
    ]
    if "mirror_bridge" not in present or "pybind11" not in present:
        sys.exit("results missing mirror_bridge or pybind11 - refusing to render")

    today = datetime.date.today().isoformat()
    out = []
    if not args.fragment:
        out.append("# Benchmarks")
        out.append("")
    out.append(
        f"_Regenerated {today} at commit `{args.commit[:12]}` on {args.env}._"
    )
    out.append("")
    out.append(
        "All frameworks bind the **same C++ class** "
        "([`benchmark_class.hpp`](../../benchmarks/runtime/shared/benchmark_class.hpp)) "
        "with the same compiler and `-O3`; only the binding layer differs. "
        "Absolute times vary by machine - the **ratio columns** are the "
        "stable signal. Reproduce with `./run_benchmarks.sh` from the repo root."
    )
    out.append("")

    header = ["Operation"] + [fw.replace("_", " ") for fw in present]
    header += [f"{fw.replace('_', ' ')} / mirror bridge" for fw in present if fw != "mirror_bridge"]
    out.append("| " + " | ".join(header) + " |")
    out.append("|" + "---|" * len(header))

    for key, label in OPERATIONS:
        if not all(key in data[fw] for fw in present):
            continue
        mb = data["mirror_bridge"][key]
        row = [label] + [fmt_ns(data[fw][key]) for fw in present]
        for fw in present:
            if fw == "mirror_bridge":
                continue
            row.append(f"{data[fw][key] / mb:.2f}x" if mb > 0 else "-")
        out.append("| " + " | ".join(row) + " |")

    out.append("")
    out.append(
        "Ratios > 1.00x mean mirror_bridge is faster for that operation; "
        "< 1.00x means the other framework is. Methodology: median of 5 runs, "
        "warmup pass first, iteration counts per operation in "
        "[`run_runtime_benchmarks.py`](../../benchmarks/runtime/python/run_runtime_benchmarks.py)."
    )
    out.append("")
    print("\n".join(out))


if __name__ == "__main__":
    main()
