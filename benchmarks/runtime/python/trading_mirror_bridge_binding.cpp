#include "mirror_bridge.hpp"
#include "../shared/trading_bench.hpp"

using namespace trading;

// Nine lines, and nothing in them is per-method: the reflection pass reads
// the three classes and binds every method, field and constructor it finds.
MIRROR_BRIDGE_MODULE(trade_mb,
    mirror_bridge::bind_class<Pricer>(m, "Pricer");
    mirror_bridge::bind_class<SignalEngine>(m, "SignalEngine");
    mirror_bridge::bind_class<OrderBook>(m, "OrderBook");
)
