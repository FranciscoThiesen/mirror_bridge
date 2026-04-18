#pragma once
#include <stdexcept>
#include <vector>
namespace api {
struct Container {
    std::vector<int> data;
    int get(int i) const {
        if (i < 0 || i >= (int)data.size())
            throw std::out_of_range("index " + std::to_string(i) + " out of range");
        return data[i];
    }
    void set_size(int n) {
        if (n < 0) throw std::invalid_argument("size must be non-negative");
        data.resize(n);
    }
    void check_positive(int v) {
        if (v <= 0) throw std::domain_error("must be positive");
    }
    void io_failure() {
        throw std::runtime_error("simulated IO failure");
    }
};
}
