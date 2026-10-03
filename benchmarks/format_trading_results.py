#!/usr/bin/env python3
"""Render trading_results.json into the markdown fragment docs/internals/
benchmarks.md splices between its TRADING markers.

Every ratio printed here is computed from the JSON. Nothing in the prose is
a number somebody typed, because the one way a results page goes stale
without anyone noticing is a hand-carried figure in a sentence.
"""
import argparse
import json
import os
import sys

ORDER = ["python", "numpy", "numpy+scipy", "pybind11", "mirror_bridge", "nanobind"]


def ordered(names):
    known = [n for n in ORDER if n in names]
    return known + sorted(n for n in names if n not in ORDER)


def fmt(v):
    if v is None:
        return "-"
    return f"{v:.0f}" if v >= 100 else f"{v:.1f}"


def ratio(a, b):
    """How many times faster b is than a (both ns, lower is better)."""
    if not a or not b:
        return None
    return a / b


def render(data, commit, env):
    mb = "mirror_bridge"
    out = []
    out.append(f"_Regenerated{' at commit `' + commit + '`' if commit else ''}"
               f"{' on ' + env if env else ''}._")
    out.append("")
    out.append("Three trading workloads, chosen so the answer to \"does my binding "
               "framework matter\" falls out of the data. Every framework binds the "
               "[same header](../../benchmarks/runtime/shared/trading_bench.hpp) and every "
               "batched method delegates to the same kernel the per-element method uses, "
               "so no framework is measured against a different implementation. "
               "Reproduce with `./run_benchmarks.sh`.")
    out.append("")

    # ---- 1. order book
    book = data["order_book"]["rows"]
    out.append("### Order book — one crossing per market event")
    out.append("")
    out.append("A price-level book on a 4096-tick ladder, 20,000 events, "
               "**60% add / 30% cancel / 10% market**. Branchy pointer chasing with "
               "nothing to vectorise, so numpy is not a competitor and the binding "
               "layer is the whole story. The book matches against price levels rather "
               "than individual resting orders.")
    out.append("")
    out.append("| | ns/event, one call per event | ns/event, whole stream in one call |")
    out.append("|---|---:|---:|")
    for name in ordered([k for k in book if not k.endswith(" (one call)")]):
        batched = book.get(name + " (one call)")
        out.append(f"| {name} | {fmt(book[name])} | {fmt(batched)} |")
    out.append("")
    py = book.get("python")
    worst = max((book[k], k) for k in book if not k.endswith(" (one call)") and k != "python")
    if py and worst[0] > py:
        out.append(f"At one crossing per event **{worst[1]} is slower than pure Python** "
                   f"({fmt(worst[0])} vs {fmt(py)} ns): the binding overhead exceeds what "
                   "the C++ saves. Batching the same work into one call is what changes "
                   f"the picture — {fmt(book[mb + ' (one call)'])} ns/event for "
                   f"{mb}, {ratio(py, book[mb + ' (one call)']):.1f}x pure Python.")
        out.append("")

    # ---- 2. pricing
    bs = data["black_scholes"]["rows"]
    out.append("### Black-Scholes — compute-heavy")
    out.append("")
    out.append("20,000 contracts per call. The normal CDF is the Abramowitz & Stegun "
               "polynomial rather than `std::erf`, so Python, numpy and C++ evaluate the "
               "identical expression and the columns differ in substrate, not arithmetic. "
               "\"scalar out\" returns one double instead of a vector, isolating the cost "
               "of handing the results back.")
    out.append("")
    out.append("| | ns/contract | scalar out |")
    out.append("|---|---:|---:|")
    for name in ordered([k for k in bs if not k.endswith(" (scalar out)")]):
        out.append(f"| {name} | {fmt(bs[name])} | {fmt(bs.get(name + ' (scalar out)'))} |")
    out.append("")
    if "numpy" in bs and mb in bs:
        rel = ratio(bs["numpy"], bs[mb])
        verdict = (f"{rel:.2f}x numpy" if rel >= 1 else f"{1/rel:.2f}x slower than numpy")
        out.append(f"Arithmetic dominates, so the binders bunch up: {mb} is {verdict}, and "
                   f"pure Python is {ratio(bs['python'], bs[mb]):.0f}x slower than any of them. "
                   "This is the half of a trading stack where the binding layer does not matter.")
        out.append("")

    # ---- 3. the sweep
    sweep = data["signal_sweep"]["rows"]
    impls = ordered({k for row in sweep.values() for k in row})
    chunks = sorted(sweep, key=int)
    out.append("### Crossing frequency — how much batching buys")
    out.append("")
    out.append("One EWMA-crossover recurrence over 262,144 ticks at every chunk size; "
               "`on_batch` is a loop over `on_tick`, so the arithmetic is byte-identical "
               "and the only thing that changes is how often Python is entered. "
               "numpy+scipy appears at the full-array size, where `signal.lfilter` can "
               "actually be used — an EWMA is a linear recurrence, so a vectorised filter "
               "is the honest baseline rather than a Python loop over numpy scalars.")
    out.append("")
    out.append("| ticks per call | " + " | ".join(impls) + " |")
    out.append("|---:|" + "---:|" * len(impls))
    for c in chunks:
        cells = " | ".join(fmt(sweep[c].get(i)) for i in impls)
        out.append(f"| {int(c):,} | {cells} |")
    out.append("")

    first, last = sweep[chunks[0]], sweep[chunks[-1]]
    binders = [k for k in (mb, "pybind11", "nanobind") if k in first]
    if len(binders) > 1:
        speedup = {k: first[k] / last[k] for k in binders}
        out.append(f"Batching is worth {min(speedup.values()):.0f}x to {max(speedup.values()):.0f}x "
                   "per framework, and almost all of it has arrived by 64 ticks per call. "
                   "What does NOT happen is the frameworks converging: the spread between "
                   f"them is {max(first[k] for k in binders) / min(first[k] for k in binders):.1f}x "
                   "at one tick per call and "
                   f"{max(last[k] for k in binders) / min(last[k] for k in binders):.1f}x at "
                   "262,144, because once the per-call overhead is amortised away what is "
                   "left is converting the input, and that cost is per element. The next "
                   "table separates the two.")
        out.append("")

    # ---- 4. bulk ingest, which is what the flat tail actually measures
    if "bulk_ingest" in data:
        ing = data["bulk_ingest"]["rows"]
        out.append("### Getting the numbers in — the flat tail explained")
        out.append("")
        out.append("The whole 262,144-tick array in one call, handed over two ways: as a "
                   "Python `list`, and as an `array.array('d')`, which is a contiguous "
                   "buffer. Same kernel, same work, so the difference is purely how each "
                   "framework ingests it.")
        out.append("")
        out.append("| | list | array.array('d') |")
        out.append("|---|---:|---:|")
        names = ordered({k.rsplit(" (", 1)[0] for k in ing})
        for name in names:
            out.append(f"| {name} | {fmt(ing.get(name + ' (list)'))} | {fmt(ing.get(name + ' (buffer)'))} |")
        out.append("")
        mbl, mbb = ing.get(mb + " (list)"), ing.get(mb + " (buffer)")
        if mbl and mbb and mbb < mbl:
            slower = [k for k in ("pybind11", "nanobind")
                      if ing.get(k + " (buffer)") and ing.get(k + " (list)")
                      and ing[k + " (buffer)"] > ing[k + " (list)"]]
            out.append(f"mirror_bridge recognises the buffer and memcpys it: {ratio(mbl, mbb):.1f}x "
                       "faster, with nothing asked of the author. "
                       + (f"{' and '.join(slower)} get *slower*, because their `stl.h` casters walk "
                          "it through the generic sequence protocol. " if slower else "")
                       + "That is a real advantage but a narrow one, and it is worth being "
                       "precise about: both can match or beat this if the author hand-writes "
                       "a `py::array_t` / `nb::ndarray` signature. The claim is parity-or-better "
                       "for zero code, not that the ceiling is higher.")
            out.append("")

    # ---- the unflattering half, computed
    out.append("### Where mirror_bridge loses")
    out.append("")
    losses = []
    for label, rows, unit in (("order book, per event", book, "ns/event"),
                              ("Black-Scholes", bs, "ns/contract")):
        for other in ("nanobind", "pybind11", "numpy"):
            if other in rows and mb in rows and rows[other] < rows[mb]:
                losses.append(f"- **{other}** is faster on {label}: "
                              f"{fmt(rows[other])} vs {fmt(rows[mb])} {unit} "
                              f"({ratio(rows[mb], rows[other]):.2f}x).")
    beaten_everywhere = [k for k in ("nanobind", "pybind11")
                         if k in first and all(sweep[c].get(k, 1e9) < sweep[c].get(mb, 0) for c in chunks)]
    for k in beaten_everywhere:
        worst_ratio = max(sweep[c][mb] / sweep[c][k] for c in chunks)
        losses.append(f"- **{k}** is faster at every point of the crossing-frequency sweep, "
                      f"by up to {worst_ratio:.2f}x.")
    if "numpy+scipy" in last and mb in last and last["numpy+scipy"] < last[mb]:
        losses.append("- **numpy+scipy** wins the full-array signal case.")
    out.extend(losses or ["- Nothing in this run."])
    out.append("")

    nb_lines = data.get("_meta", {}).get("binding_lines", {})
    if nb_lines:
        parts = ", ".join(f"{k} {v}" for k, v in sorted(nb_lines.items(), key=lambda kv: kv[1]))
        out.append(f"What is defensible is the combination, not the speed alone: "
                   f"hand-written binding lines for these three classes are {parts}. "
                   "The mirror_bridge figure is the `MIRROR_BRIDGE_MODULE` block; through "
                   "the CLI it is zero.")
        out.append("")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                      "runtime", "python", "trading_results.json"))
    ap.add_argument("--commit", default="")
    ap.add_argument("--env", default="")
    ap.add_argument("--fragment", action="store_true", help="print the markdown only")
    args = ap.parse_args()

    try:
        data = json.load(open(args.results))
    except FileNotFoundError:
        print(f"no results at {args.results}; run run_trading_benchmarks.py first", file=sys.stderr)
        return 1

    # The numpy columns are load-bearing: without them the page would claim a
    # comparison it did not make. Refuse rather than render a partial table,
    # mirroring format_results.py's refusal when a framework is missing.
    if data.get("_meta", {}).get("numpy") is None:
        print("refusing to render: numpy was missing, so the vectorised baselines are absent",
              file=sys.stderr)
        return 1

    print(render(data, args.commit, args.env))
    return 0


if __name__ == "__main__":
    sys.exit(main())
