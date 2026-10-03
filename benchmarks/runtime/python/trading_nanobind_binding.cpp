#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>
#include "../shared/trading_bench.hpp"

namespace nb = nanobind;
using namespace trading;

NB_MODULE(trade_nb, m) {
    nb::class_<Pricer>(m, "Pricer")
        .def(nb::init<>())
        .def("price", &Pricer::price)
        .def("price_batch", &Pricer::price_batch)
        .def("portfolio_value", &Pricer::portfolio_value);

    nb::class_<SignalEngine>(m, "SignalEngine")
        .def(nb::init<>())
        .def_rw("fast_alpha", &SignalEngine::fast_alpha)
        .def_rw("slow_alpha", &SignalEngine::slow_alpha)
        .def_rw("crossovers", &SignalEngine::crossovers)
        .def("reset", &SignalEngine::reset)
        .def("on_tick", &SignalEngine::on_tick)
        .def("on_batch", &SignalEngine::on_batch);

    nb::class_<OrderBook>(m, "OrderBook")
        .def(nb::init<>())
        .def("reset", &OrderBook::reset)
        .def("add_limit_order", &OrderBook::add_limit_order)
        .def("cancel_order", &OrderBook::cancel_order)
        .def("submit_market_order", &OrderBook::submit_market_order)
        .def("best_bid", &OrderBook::best_bid)
        .def("best_ask", &OrderBook::best_ask)
        .def("traded", &OrderBook::traded)
        .def("replay", &OrderBook::replay);
}
