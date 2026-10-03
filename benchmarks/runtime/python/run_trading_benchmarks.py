#!/usr/bin/env python3
"""Trading workloads: the same C++ through three binding frameworks, against
pure Python and against numpy/scipy where those are genuine competitors.

What this is for. The existing runtime benchmark times one operation at a
time, which answers "which binder has the cheapest call". It does not answer
the question someone with a trading system actually has, which is "does the
binder matter for my workload at all". That depends entirely on how often
you cross the language boundary, so the suite measures three shapes:

  order book    one crossing per market event. Branchy, nothing to
                vectorise, so the binding layer is the whole story.
  pricing       Black-Scholes over a vector. Arithmetic dominates, so the
                binding layer should round to zero and numpy should be
                close or ahead.
  signal sweep  one EWMA recurrence driven at chunk sizes from 1 to 1M.
                Identical arithmetic at every point, so the whole curve is
                binding cost and the crossing frequency where the
                frameworks converge is the answer.

Fairness rules, because the easy version of this benchmark flatters the
host project:

  * every framework binds the SAME header, and every batched method
    delegates to the same kernel the per-element method uses;
  * the pure-Python baseline is written the way a competent author would
    write it (locals in the hot loop, __slots__, bound methods hoisted),
    not the way that makes the ratio look good;
  * numpy and scipy appear wherever the workload is genuinely vectorisable,
    including where they win. An EWMA is a linear recurrence, so the numpy
    baseline uses scipy.signal.lfilter rather than a Python loop over numpy
    scalars, which would be a straw man;
  * results that go against mirror_bridge are reported, not dropped.

Writes trading_results.json next to runtime_results.json.
"""
import gc
import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.environ.get("MB_BENCH_BUILD", os.path.join(HERE, "build")))

try:
    import numpy as np
except ImportError:
    np = None
try:
    from scipy.signal import lfilter
except ImportError:
    lfilter = None

FRAMEWORKS = {}
for mod_name, label in (("trade_mb", "mirror_bridge"), ("trade_pb", "pybind11"), ("trade_nb", "nanobind")):
    try:
        FRAMEWORKS[label] = __import__(mod_name)
    except ImportError:
        pass

REPEAT = 9


def bench(fn, iterations):
    """Fastest ns/iteration over REPEAT runs, after a discarded warmup.

    The minimum rather than the median, which is a deliberate departure from
    run_runtime_benchmarks.py. Several of these workloads return a 20k-element
    list per call, so a later measurement can land in the garbage collection
    the earlier one caused: taking the median of five produced a reading 4x
    too slow for one framework that measured correctly in isolation. The
    minimum is the standard estimator for "how fast is this code" because
    interference can only ever make a sample slower. Collecting first, and
    keeping the collector off during the timed section, removes the rest.
    """
    fn()
    samples = []
    for _ in range(REPEAT):
        gc.collect()
        gc.disable()
        try:
            t0 = time.perf_counter()
            fn()
            samples.append((time.perf_counter() - t0) / iterations * 1e9)
        finally:
            gc.enable()
    return min(samples)


# ----------------------------------------------------------- pure Python --

def py_norm_cdf(x):
    a1, a2, a3, a4, a5, p = 0.254829592, -0.284496736, 1.421413741, -1.453152027, 1.061405429, 0.3275911
    sign = -1.0 if x < 0 else 1.0
    z = abs(x) / math.sqrt(2.0)
    t = 1.0 / (1.0 + p * z)
    y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * math.exp(-z * z)
    return 0.5 * (1.0 + sign * y)


def py_black_scholes(s, k, r, vol, t):
    sqrt_t = math.sqrt(t)
    d1 = (math.log(s / k) + (r + 0.5 * vol * vol) * t) / (vol * sqrt_t)
    d2 = d1 - vol * sqrt_t
    return s * py_norm_cdf(d1) - k * math.exp(-r * t) * py_norm_cdf(d2)


class PySignalEngine:
    """The same recurrence, written as a Python author would write it."""
    __slots__ = ("fast_alpha", "slow_alpha", "fast", "slow", "primed", "crossovers", "state")

    def __init__(self):
        self.fast_alpha = 2.0 / 13
        self.slow_alpha = 2.0 / 27
        self.fast = self.slow = 0.0
        self.primed = False
        self.crossovers = 0
        self.state = 0

    def on_batch(self, ticks):
        fast, slow, state, crossings = self.fast, self.slow, self.state, self.crossovers
        fa, sa, primed = self.fast_alpha, self.slow_alpha, self.primed
        for px in ticks:
            if not primed:
                fast = slow = px
                primed = True
                continue
            fast += fa * (px - fast)
            slow += sa * (px - slow)
            now = 1 if fast > slow else (-1 if fast < slow else 0)
            if now and now != state:
                if state:
                    crossings += 1
                state = now
        self.fast, self.slow, self.state, self.crossovers, self.primed = fast, slow, state, crossings, primed
        return state


class PyOrderBook:
    __slots__ = ("bid", "ask", "best_bid", "best_ask", "traded")
    LEVELS = 4096

    def __init__(self):
        self.reset()

    def reset(self):
        self.bid = [0] * self.LEVELS
        self.ask = [0] * self.LEVELS
        self.best_bid = -1
        self.best_ask = self.LEVELS
        self.traded = 0

    def replay(self, events):
        bid, ask = self.bid, self.ask
        best_bid, best_ask, traded = self.best_bid, self.best_ask, self.traded
        levels = self.LEVELS
        for i in range(0, len(events), 4):
            op, side, price, qty = events[i], events[i + 1], events[i + 2], events[i + 3]
            if op == 0:
                if side == 0:
                    bid[price] += qty
                    if price > best_bid:
                        best_bid = price
                else:
                    ask[price] += qty
                    if price < best_ask:
                        best_ask = price
            elif op == 1:
                book = bid if side == 0 else ask
                book[price] = book[price] - qty if book[price] > qty else 0
                if side == 0 and price == best_bid and book[price] == 0:
                    while best_bid >= 0 and bid[best_bid] == 0:
                        best_bid -= 1
                elif side == 1 and price == best_ask and book[price] == 0:
                    while best_ask < levels and ask[best_ask] == 0:
                        best_ask += 1
            else:
                filled = 0
                if side == 0:
                    while qty > 0 and best_ask < levels:
                        take = ask[best_ask] if ask[best_ask] < qty else qty
                        ask[best_ask] -= take
                        qty -= take
                        filled += take
                        while best_ask < levels and ask[best_ask] == 0:
                            best_ask += 1
                else:
                    while qty > 0 and best_bid >= 0:
                        take = bid[best_bid] if bid[best_bid] < qty else qty
                        bid[best_bid] -= take
                        qty -= take
                        filled += take
                        while best_bid >= 0 and bid[best_bid] == 0:
                            best_bid -= 1
                traded += filled
        self.best_bid, self.best_ask, self.traded = best_bid, best_ask, traded
        return traded


# ------------------------------------------------------------- input data --

def make_events(n, seed=12345):
    """60% add, 30% cancel, 10% market — stated because an unstated mix
    leaves the per-event number unanchored. Cancels and modifies dominate a
    real equities or futures feed, so an add-heavy stream would understate
    the cheap operations."""
    state = seed
    out = []
    for _ in range(n):
        state = (1103515245 * state + 12345) & 0x7FFFFFFF
        r = state % 100
        state = (1103515245 * state + 12345) & 0x7FFFFFFF
        side = state % 2
        state = (1103515245 * state + 12345) & 0x7FFFFFFF
        price = 1800 + (state % 400) + (0 if side == 0 else 400)
        state = (1103515245 * state + 12345) & 0x7FFFFFFF
        qty = 1 + state % 50
        op = 0 if r < 60 else (1 if r < 90 else 2)
        out.extend((op, side, price, qty))
    return out


def make_ticks(n, seed=99):
    state = seed
    px, out = 100.0, []
    for _ in range(n):
        state = (1103515245 * state + 12345) & 0x7FFFFFFF
        px += (state % 2001 - 1000) / 10000.0
        out.append(px)
    return out


def make_contracts(n):
    flat = []
    for i in range(n):
        flat.extend((100.0 + (i % 50), 100.0, 0.03, 0.2 + (i % 10) / 100.0, 0.5 + (i % 4) / 4.0))
    return flat


# ------------------------------------------------------------- the suite --

def run():
    results = {}

    # --- 1. order book, one crossing per event -----------------------------
    n_events = 20000
    events = make_events(n_events)
    book_rows = {}

    pyb = PyOrderBook()
    book_rows["python"] = bench(lambda: (pyb.reset(), pyb.replay(events)), n_events)

    for label, mod in FRAMEWORKS.items():
        b = mod.OrderBook()
        # One crossing per event: what an event-driven strategy actually does.
        def per_event(b=b, ev=events):
            b.reset()
            add, cancel, market = b.add_limit_order, b.cancel_order, b.submit_market_order
            for i in range(0, len(ev), 4):
                op = ev[i]
                if op == 0:
                    add(ev[i + 1], ev[i + 2], ev[i + 3])
                elif op == 1:
                    cancel(ev[i + 1], ev[i + 2], ev[i + 3])
                else:
                    market(ev[i + 1], ev[i + 3])
        book_rows[label] = bench(per_event, n_events)
        book_rows[label + " (one call)"] = bench(
            lambda b=b, ev=events: (b.reset(), b.replay(ev)), n_events)

    results["order_book"] = {
        "unit": "ns/event",
        "note": f"{n_events} events, 60% add / 30% cancel / 10% market. "
                "numpy is absent because a book is branchy pointer chasing with nothing to vectorise.",
        "rows": book_rows,
    }

    # --- 2. Black-Scholes, compute-heavy -----------------------------------
    n_contracts = 20000
    flat = make_contracts(n_contracts)
    price_rows = {}

    def py_batch(flat=flat):
        out = []
        ap = out.append
        for i in range(0, len(flat), 5):
            ap(py_black_scholes(flat[i], flat[i + 1], flat[i + 2], flat[i + 3], flat[i + 4]))
        return out
    price_rows["python"] = bench(py_batch, n_contracts)

    if np is not None:
        arr = np.array(flat, dtype=np.float64).reshape(-1, 5)
        s, k, r, vol, t = (arr[:, i] for i in range(5))

        def np_batch():
            sqrt_t = np.sqrt(t)
            d1 = (np.log(s / k) + (r + 0.5 * vol * vol) * t) / (vol * sqrt_t)
            d2 = d1 - vol * sqrt_t
            # The same Abramowitz & Stegun polynomial, vectorised, so the
            # comparison is substrate and not arithmetic.
            def cdf(x):
                a1, a2, a3, a4, a5, p = 0.254829592, -0.284496736, 1.421413741, -1.453152027, 1.061405429, 0.3275911
                sign = np.where(x < 0, -1.0, 1.0)
                z = np.abs(x) / math.sqrt(2.0)
                tt = 1.0 / (1.0 + p * z)
                y = 1.0 - (((((a5 * tt + a4) * tt) + a3) * tt + a2) * tt + a1) * tt * np.exp(-z * z)
                return 0.5 * (1.0 + sign * y)
            return s * cdf(d1) - k * np.exp(-r * t) * cdf(d2)
        price_rows["numpy"] = bench(np_batch, n_contracts)

    for label, mod in FRAMEWORKS.items():
        p = mod.Pricer()
        price_rows[label] = bench(lambda p=p, f=flat: p.price_batch(f), n_contracts)
        price_rows[label + " (scalar out)"] = bench(lambda p=p, f=flat: p.portfolio_value(f), n_contracts)

    results["black_scholes"] = {
        "unit": "ns/contract",
        "note": f"{n_contracts} contracts per call. "
                "'scalar out' returns one double instead of a vector, isolating the cost of the return.",
        "rows": price_rows,
    }

    # --- 3. the crossing-frequency sweep -----------------------------------
    n_ticks = 262144
    ticks = make_ticks(n_ticks)
    sweep = {}
    for chunk in (1, 8, 64, 512, 4096, 32768, n_ticks):
        row = {}
        chunks = [ticks[i:i + chunk] for i in range(0, n_ticks, chunk)]

        pys = PySignalEngine()
        row["python"] = bench(lambda c=chunks, e=pys: [e.on_batch(x) for x in c], n_ticks)

        for label, mod in FRAMEWORKS.items():
            e = mod.SignalEngine()
            if chunk == 1:
                flatticks = ticks
                row[label] = bench(
                    lambda e=e, t=flatticks: [e.on_tick(x) for x in t], n_ticks)
            else:
                row[label] = bench(lambda e=e, c=chunks: [e.on_batch(x) for x in c], n_ticks)
        sweep[str(chunk)] = row

    # The flat tail of the sweep is not pure compute: at large chunk sizes the
    # per-call overhead is amortised away and what is left is converting the
    # input, which is per ELEMENT and differs by framework. Handing the same
    # data over as a contiguous buffer separates the two.
    buffer_rows = {}
    if True:
        import array as _array
        buf = _array.array("d", ticks)
        for label, mod in FRAMEWORKS.items():
            e = mod.SignalEngine()
            buffer_rows[label + " (list)"] = bench(lambda e=e, t=ticks: e.on_batch(t), n_ticks)
            try:
                e2 = mod.SignalEngine()
                buffer_rows[label + " (buffer)"] = bench(lambda e=e2, b=buf: e2.on_batch(b), n_ticks)
            except Exception:
                buffer_rows[label + " (buffer)"] = None
    results["bulk_ingest"] = {
        "unit": "ns/tick",
        "note": f"the whole {n_ticks}-tick array in ONE call, as a Python list and as an "
                "array.array('d'). Same work, same kernel; the only difference is how the "
                "framework gets the numbers in.",
        "rows": buffer_rows,
    }

    if np is not None and lfilter is not None:
        a = np.array(ticks, dtype=np.float64)

        def scipy_ewma():
            # An EWMA is a linear recurrence, so the honest numpy baseline is
            # a filter, not a Python loop. This is the competitor that can
            # beat the C++ at large chunk sizes.
            fa, sa = 2.0 / 13, 2.0 / 27
            fast = lfilter([fa], [1.0, -(1.0 - fa)], a)
            slow = lfilter([sa], [1.0, -(1.0 - sa)], a)
            sign = np.sign(fast - slow)
            return int(np.count_nonzero(np.diff(sign) != 0))
        sweep[str(n_ticks)]["numpy+scipy"] = bench(scipy_ewma, n_ticks)

    results["signal_sweep"] = {
        "unit": "ns/tick",
        "note": f"{n_ticks} ticks total at every chunk size; on_batch is a loop over on_tick, so only "
                "the crossing frequency changes. numpy+scipy uses signal.lfilter and is listed only at "
                "the full-array size, where a vectorised filter is actually usable.",
        "rows": sweep,
    }

    results["_meta"] = {
        "frameworks": sorted(FRAMEWORKS),
        "numpy": np.__version__ if np is not None else None,
        "scipy_lfilter": lfilter is not None,
        "repeat": REPEAT,
        "binding_lines": {"mirror_bridge": 9, "pybind11": 29, "nanobind": 27},
    }
    return results


def main():
    if not FRAMEWORKS:
        print("no trading benchmark modules importable; build them first", file=sys.stderr)
        return 1
    results = run()
    out = os.path.join(HERE, "trading_results.json")
    with open(out, "w") as f:
        json.dump(results, f, indent=2)

    for name, block in results.items():
        if name.startswith("_"):
            continue
        print(f"\n{name}  [{block['unit']}]")
        rows = block["rows"]
        if name == "signal_sweep":
            impls = sorted({k for r in rows.values() for k in r})
            print("  chunk    " + "".join(f"{i:>16}" for i in impls))
            for chunk in sorted(rows, key=int):
                cells = "".join(f"{rows[chunk].get(i, float('nan')):>16.1f}" if i in rows[chunk]
                                else f"{'-':>16}" for i in impls)
                print(f"  {int(chunk):>6}   {cells}")
        else:
            for impl in sorted(rows):
                print(f"    {impl:<34} {rows[impl]:>10.1f}")
    print(f"\nwrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
