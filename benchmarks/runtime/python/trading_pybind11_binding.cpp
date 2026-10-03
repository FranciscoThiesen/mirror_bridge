#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "../shared/trading_bench.hpp"

namespace py = pybind11;
using namespace trading;

// The same surface mirror_bridge derives, written by hand. Counted in the
// results table: this is what the generator is replacing.
PYBIND11_MODULE(trade_pb, m) {
    py::class_<Pricer>(m, "Pricer")
        .def(py::init<>())
        .def("price", &Pricer::price)
        .def("price_batch", &Pricer::price_batch)
        .def("portfolio_value", &Pricer::portfolio_value);

    py::class_<SignalEngine>(m, "SignalEngine")
        .def(py::init<>())
        .def_readwrite("fast_alpha", &SignalEngine::fast_alpha)
        .def_readwrite("slow_alpha", &SignalEngine::slow_alpha)
        .def_readwrite("crossovers", &SignalEngine::crossovers)
        .def("reset", &SignalEngine::reset)
        .def("on_tick", &SignalEngine::on_tick)
        .def("on_batch", &SignalEngine::on_batch);

    py::class_<OrderBook>(m, "OrderBook")
        .def(py::init<>())
        .def("reset", &OrderBook::reset)
        .def("add_limit_order", &OrderBook::add_limit_order)
        .def("cancel_order", &OrderBook::cancel_order)
        .def("submit_market_order", &OrderBook::submit_market_order)
        .def("best_bid", &OrderBook::best_bid)
        .def("best_ask", &OrderBook::best_ask)
        .def("traded", &OrderBook::traded)
        .def("replay", &OrderBook::replay);
}
