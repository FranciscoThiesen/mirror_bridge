#!/usr/bin/env python3
"""Gate the runtime ratios against a committed baseline.

The benchmarks page is regenerated monthly, but nothing read it: a regression
could land and sit in a table for a month. This turns the same numbers into a
check.

Two distinct things, kept distinct on purpose:

  * **Regression** is a failure. A ratio that was 2.8x against pybind11 and is
    now 2.2x means something got slower, and that fails the build. The floors
    are measured, committed values, not aspirations, so this can never fail
    for a claim that was never true.

  * **Being behind** is reported, not failed. Where a competitor is faster, the
    output says so and by how much. That is the honest form of "at least as
    fast as any alternative": a list of the operations where it is not yet
    true, rather than a claim that it is.

Absolute ns/op moves with the machine, so only same-run ratios are compared.
"""

import argparse
import json
import os
import sys

COMPETITORS = ["pybind11", "nanobind", "swig", "boost_python"]
DEFAULT_TOLERANCE = 0.12


def ratios(data):
    """{operation: {competitor: competitor_ns / mirror_bridge_ns}}"""
    mb = data.get("mirror_bridge", {})
    out = {}
    for op, mb_ns in mb.items():
        if not mb_ns or mb_ns <= 0:
            continue
        row = {}
        for fw in COMPETITORS:
            ns = data.get(fw, {}).get(op, 0)
            if ns and ns > 0:
                row[fw] = ns / float(mb_ns)
        if row:
            out[op] = row
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("results")
    ap.add_argument("--floors", default=os.path.join(os.path.dirname(__file__), "ratio_floors.json"))
    ap.add_argument("--tolerance", type=float, default=DEFAULT_TOLERANCE,
                    help="fraction a ratio may fall below its floor before it "
                         "counts as a regression (default %.2f, which is "
                         "runner noise on these operations)" % DEFAULT_TOLERANCE)
    ap.add_argument("--update", action="store_true",
                    help="write the current ratios as the new floors, and exit 0")
    args = ap.parse_args()

    with open(args.results) as f:
        data = json.load(f)
    fresh = ratios(data)
    if not fresh:
        sys.exit("no usable mirror_bridge numbers in %s" % args.results)

    try:
        with open(args.floors) as f:
            floors = json.load(f)["floors"]
    except IOError:
        # Bootstrap rather than fail. The first real run on the real machine is
        # where a floor should come from; a hand-written one would just be a
        # guess that either never fires or fires for no reason.
        print("no baseline at %s — establishing it from this run" % args.floors)
        args.update = True
        floors = None

    if args.update or floors is None:
        payload = {
            "_comment": "Measured ratios (competitor ns / mirror_bridge ns), "
                        "committed as the floor a later run must not fall below. "
                        "Refresh deliberately with check_ratios.py --update when "
                        "an improvement lands, never to make a red build green.",
            "floors": {op: {fw: round(v, 3) for fw, v in row.items()}
                       for op, row in sorted(fresh.items())},
        }
        with open(args.floors, "w") as f:
            json.dump(payload, f, indent=2, sort_keys=True)
            f.write("\n")
        print("wrote %d operations to %s" % (len(fresh), args.floors))
        return 0

    regressions, behind, checked = [], [], 0
    for op, row in sorted(floors.items()):
        for fw, floor in sorted(row.items()):
            got = fresh.get(op, {}).get(fw)
            if got is None:
                continue
            checked += 1
            if got < floor * (1.0 - args.tolerance):
                regressions.append((op, fw, floor, got))

    for op, row in sorted(fresh.items()):
        for fw, got in sorted(row.items()):
            if got < 1.0:
                behind.append((op, fw, got))

    print("compared %d operation/framework ratios against %s" % (checked, os.path.basename(args.floors)))

    if behind:
        print("\nSlower than a competitor (reported, not a failure):")
        for op, fw, got in behind:
            print("  %-22s %-12s %.2fx  (%s is %.0f%% faster)" % (
                op, fw, got, fw, (1.0 / got - 1.0) * 100))
    else:
        print("\nNot slower than any competitor on any measured operation.")

    if regressions:
        print("\nREGRESSED beyond the %.0f%% tolerance:" % (args.tolerance * 100))
        for op, fw, floor, got in regressions:
            print("  %-22s vs %-12s floor %.2fx, got %.2fx  (%.0f%% worse)" % (
                op, fw, floor, got, (1.0 - got / floor) * 100))
        print("\nIf the drop is real and intended, re-measure and refresh with:")
        print("  python3 benchmarks/check_ratios.py %s --update" % args.results)
        return 1

    print("\nNo ratio regressed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
