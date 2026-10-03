#pragma once

// Trading workloads, shared by every binding framework in this suite.
//
// The existing runtime benchmark measures one operation at a time. This one
// measures three shapes of real work, chosen so the conclusion falls out of
// the data rather than out of the prose:
//
//   OrderBook     one Python->C++ crossing per market event. Branchy pointer
//                 chasing, nothing to vectorise, so the binding layer is the
//                 whole story and numpy is not a competitor.
//   Pricer        Black-Scholes over a vector of contracts. Arithmetic
//                 dominates, so the binding layer should round to zero and
//                 numpy should be close.
//   SignalEngine  the same EWMA recurrence driven at chunk sizes from 1 to
//                 1M. A byte-identical computation at every point, so the
//                 whole curve is binding cost, and the chunk size where the
//                 frameworks converge is the answer to "does my binder
//                 matter".
//
// Every batched method delegates to the same raw-pointer kernel the
// per-element method uses, so there is exactly one copy of each algorithm
// and no framework is measured against a different implementation.
//
// Deliberate simplifications, stated because a market-structure reader will
// find them: the book matches against price levels rather than individual
// resting orders, so a cancel decrements a level and is clamped at zero;
// and rolling variance uses the sum-of-squares form, which is vectorisable
// and numerically poor, because the point is to compare substrates on
// identical arithmetic rather than to ship a variance estimator.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace trading {

// ---------------------------------------------------------------- pricing --

// Abramowitz & Stegun 26.2.17 rather than std::erf, so the Python and numpy
// baselines can implement the identical expression. A difference in the
// normal CDF would otherwise show up as a difference between languages.
inline double norm_cdf(double x) {
    constexpr double a1 = 0.254829592, a2 = -0.284496736, a3 = 1.421413741;
    constexpr double a4 = -1.453152027, a5 = 1.061405429, p = 0.3275911;
    const double sign = x < 0 ? -1.0 : 1.0;
    const double z = std::fabs(x) / std::sqrt(2.0);
    const double t = 1.0 / (1.0 + p * z);
    const double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * std::exp(-z * z);
    return 0.5 * (1.0 + sign * y);
}

inline double black_scholes_kernel(double s, double k, double r, double vol, double t, bool call) {
    const double sqrt_t = std::sqrt(t);
    const double d1 = (std::log(s / k) + (r + 0.5 * vol * vol) * t) / (vol * sqrt_t);
    const double d2 = d1 - vol * sqrt_t;
    const double disc = std::exp(-r * t);
    if (call) return s * norm_cdf(d1) - k * disc * norm_cdf(d2);
    return k * disc * norm_cdf(-d2) - s * norm_cdf(-d1);
}

struct Pricer {
    // One contract per call: the per-crossing cost dominates.
    double price(double s, double k, double r, double vol, double t, bool call) const {
        return black_scholes_kernel(s, k, r, vol, t, call);
    }

    // A flat [s, k, r, vol, t] per contract, returning one price each. The
    // vector return is itself a measurable cost, so scalar_batch isolates it.
    std::vector<double> price_batch(const std::vector<double>& flat) const {
        const std::size_t n = flat.size() / 5;
        std::vector<double> out(n);
        for (std::size_t i = 0; i < n; ++i) {
            out[i] = black_scholes_kernel(flat[5 * i], flat[5 * i + 1], flat[5 * i + 2],
                                          flat[5 * i + 3], flat[5 * i + 4], true);
        }
        return out;
    }

    // Same work, one double back: what the batch costs without the return.
    double portfolio_value(const std::vector<double>& flat) const {
        const std::size_t n = flat.size() / 5;
        double total = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            total += black_scholes_kernel(flat[5 * i], flat[5 * i + 1], flat[5 * i + 2],
                                          flat[5 * i + 3], flat[5 * i + 4], true);
        }
        return total;
    }
};

// ------------------------------------------------------------------ signal --

// Two EWMAs and the sign of their difference: a linear recurrence, which is
// what makes scipy.signal.lfilter a genuine competitor rather than a
// Python-loop straw man.
struct SignalEngine {
    double fast_alpha = 2.0 / (12 + 1);
    double slow_alpha = 2.0 / (26 + 1);
    double fast = 0.0;
    double slow = 0.0;
    bool primed = false;
    int crossovers = 0;
    int state = 0;

    void reset() { fast = slow = 0.0; primed = false; crossovers = 0; state = 0; }

    int on_tick(double px) {
        if (!primed) {
            fast = slow = px;
            primed = true;
            return 0;
        }
        fast += fast_alpha * (px - fast);
        slow += slow_alpha * (px - slow);
        const int now = fast > slow ? 1 : (fast < slow ? -1 : 0);
        if (now != 0 && now != state) {
            if (state != 0) ++crossovers;
            state = now;
        }
        return state;
    }

    // Literally a loop over on_tick, so the only thing that changes across
    // the chunk-size sweep is how often Python is entered.
    int on_batch(const std::vector<double>& ticks) {
        int last = state;
        for (double px : ticks) last = on_tick(px);
        return last;
    }
};

// -------------------------------------------------------------- order book --

// A price-level book on a fixed integer tick ladder. Resting quantity per
// level, no per-order identity: a cancel decrements the level it names.
class OrderBook {
public:
    static constexpr int kLevels = 4096;

    OrderBook() : bid_qty_(kLevels, 0), ask_qty_(kLevels, 0) {}

    void reset() {
        std::fill(bid_qty_.begin(), bid_qty_.end(), 0);
        std::fill(ask_qty_.begin(), ask_qty_.end(), 0);
        best_bid_ = -1;
        best_ask_ = kLevels;
        traded_ = 0;
    }

    void add_limit_order(int side, int price, int qty) { add_kernel(side, price, qty); }
    void cancel_order(int side, int price, int qty) { cancel_kernel(side, price, qty); }
    int submit_market_order(int side, int qty) { return market_kernel(side, qty); }

    int best_bid() const { return best_bid_; }
    int best_ask() const { return best_ask_; }
    std::int64_t traded() const { return traded_; }

    // One crossing for a whole event stream: four ints per event
    // (op, side, price, qty), op 0 = add, 1 = cancel, 2 = market.
    // Lets the suite measure the same work at one crossing per event and at
    // one crossing per stream, which is the entire point of the comparison.
    std::int64_t replay(const std::vector<int>& events) {
        const std::size_t n = events.size() / 4;
        for (std::size_t i = 0; i < n; ++i) {
            const int op = events[4 * i], side = events[4 * i + 1];
            const int price = events[4 * i + 2], qty = events[4 * i + 3];
            if (op == 0) add_kernel(side, price, qty);
            else if (op == 1) cancel_kernel(side, price, qty);
            else market_kernel(side, qty);
        }
        return traded_;
    }

private:
    void add_kernel(int side, int price, int qty) {
        if (price < 0 || price >= kLevels) return;
        if (side == 0) {
            bid_qty_[price] += qty;
            if (price > best_bid_) best_bid_ = price;
        } else {
            ask_qty_[price] += qty;
            if (price < best_ask_) best_ask_ = price;
        }
    }

    void cancel_kernel(int side, int price, int qty) {
        if (price < 0 || price >= kLevels) return;
        std::vector<int>& book = side == 0 ? bid_qty_ : ask_qty_;
        book[price] = book[price] > qty ? book[price] - qty : 0;
        if (side == 0 && price == best_bid_ && book[price] == 0) {
            while (best_bid_ >= 0 && bid_qty_[best_bid_] == 0) --best_bid_;
        } else if (side == 1 && price == best_ask_ && book[price] == 0) {
            while (best_ask_ < kLevels && ask_qty_[best_ask_] == 0) ++best_ask_;
        }
    }

    int market_kernel(int side, int qty) {
        int filled = 0;
        if (side == 0) {                      // buy sweeps the ask side up
            while (qty > 0 && best_ask_ < kLevels) {
                int& resting = ask_qty_[best_ask_];
                const int take = resting < qty ? resting : qty;
                resting -= take;
                qty -= take;
                filled += take;
                if (resting == 0) {
                    while (best_ask_ < kLevels && ask_qty_[best_ask_] == 0) ++best_ask_;
                }
            }
        } else {
            while (qty > 0 && best_bid_ >= 0) {
                int& resting = bid_qty_[best_bid_];
                const int take = resting < qty ? resting : qty;
                resting -= take;
                qty -= take;
                filled += take;
                if (resting == 0) {
                    while (best_bid_ >= 0 && bid_qty_[best_bid_] == 0) --best_bid_;
                }
            }
        }
        traded_ += filled;
        return filled;
    }

    std::vector<int> bid_qty_;
    std::vector<int> ask_qty_;
    int best_bid_ = -1;
    int best_ask_ = kLevels;
    std::int64_t traded_ = 0;
};

}  // namespace trading
