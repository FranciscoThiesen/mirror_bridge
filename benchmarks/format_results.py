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

# Display order + labels. Every key must be one that
# benchmarks/runtime/python/run_runtime_benchmarks.py writes into its results
# dict: a key that does not match is a row that vanishes from the published
# page. tests/benchmark_table/ pins the two lists together, and its docstring
# records what that cost the last time they drifted.
OPERATIONS = [
    ("null_call", "Null call (method dispatch)"),
    ("arithmetic_int", "Arithmetic: add_int(42)"),
    ("arithmetic_double", "Arithmetic: multiply_double(3.14)"),
    ("string_concat", "String concat"),
    ("string_get", "String return (str out)"),
    ("string_set", "String argument (str in)"),
    ("vector_append", "Vector append (one double in)"),
    ("vector_get", "Vector return (100 doubles out)"),
    ("vector_set", "Vector argument (list in)"),
    ("attr_get", "Attribute read"),
    ("attr_set", "Attribute write"),
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
    ap.add_argument(
        "--strict", action="store_true",
        help="exit non-zero if any measured operation would not reach the "
             "page, instead of quietly rendering a shorter table",
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

    # Rendering fewer rows than were measured reads as "this is what we
    # measure", so account for every drop out loud rather than skipping it.
    dropped = []
    for key, label in OPERATIONS:
        absent = [fw for fw in present if key not in data[fw]]
        if absent:
            dropped.append((key, "not measured by " + ", ".join(absent)))
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

    floor = (data.get("_meta") or {}).get("harness_floor_ns") or {}
    if floor.get("empty"):
        empty = floor["empty"]
        c_call = floor.get("c_call") or 0.0
        out.append(
            "**Read the ratios as bounds, not as the binding layer.** Every number above "
            "is timed as `benchmark(name, lambda: ..., n)`, so each one carries a Python "
            "loop iteration and a full Python function call on top of the operation. On "
            "this run that floor is **%s** for an empty lambda and **%s** for the cheapest "
            "real C method call CPython can make. The constant is the same for all four "
            "frameworks, so it does not cancel in a ratio: it pulls every ratio toward "
            "1.00. A ratio above 1.00x is therefore a **lower** bound on how much faster "
            "mirror_bridge's binding layer is, and a ratio below 1.00x understates the gap "
            "the other way. A row within a few percent of 1.00 is not a tie that has been "
            "measured; it is a difference this instrument cannot resolve."
            % (fmt_ns(empty), fmt_ns(c_call))
        )
        out.append("")

    # Asymmetric on purpose: an operation only a competitor measured is not
    # something this project can publish a ratio for, so only mirror_bridge's
    # keys are checked for having somewhere to go.
    published = {key for key, _ in OPERATIONS}
    unpublished = [
        key for key in data["mirror_bridge"]
        if key not in published and data["mirror_bridge"][key] > 0
    ]
    for key, why in dropped:
        print(f"warning: '{key}' is not in the table: {why}", file=sys.stderr)
    for key in sorted(unpublished):
        print(f"warning: '{key}' was measured but no row in OPERATIONS "
              f"publishes it", file=sys.stderr)
    if dropped or unpublished:
        print(f"warning: the page shows {len(OPERATIONS) - len(dropped)} of "
              f"{len(data['mirror_bridge'])} measured operations",
              file=sys.stderr)

    print("\n".join(out))

    if args.strict and (dropped or unpublished):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
