#pragma once

#include <chrono>
#include <string>
#include <thread>

// Exercises the release_gil() binding option: sleep_ms stands in for any
// long-running native call (I/O, compute). With the GIL released, N Python
// threads sleeping concurrently finish in ~one sleep duration; with it held
// they serialize to N durations — which is what the test measures.
struct Worker {
    int calls = 0;

    void sleep_ms(int ms) {
        ++calls;
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    double checksum(int n) {
        double acc = 0.0;
        for (int i = 0; i < n; ++i) {
            acc += static_cast<double>(i % 7) * 0.5;
        }
        return acc;
    }

    std::string greet(const std::string& name) const {
        return "hello, " + name;
    }
};
