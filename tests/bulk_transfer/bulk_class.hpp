#pragma once

#include <vector>
#include <cstdint>
#include <cmath>

// Test class for bulk array transfer optimization.
// Methods returning vectors of numeric types should use memcpy-based
// bulk transfer instead of element-by-element Python object creation.

struct DataGenerator {
    int size = 1000;

    std::vector<float> get_floats() const {
        std::vector<float> result(size);
        for (int i = 0; i < size; ++i) {
            result[i] = static_cast<float>(i) * 0.5f;
        }
        return result;
    }

    std::vector<double> get_doubles() const {
        std::vector<double> result(size);
        for (int i = 0; i < size; ++i) {
            result[i] = std::sin(static_cast<double>(i) * 0.01);
        }
        return result;
    }

    std::vector<int> get_ints() const {
        std::vector<int> result(size);
        for (int i = 0; i < size; ++i) {
            result[i] = i * i;
        }
        return result;
    }

    std::vector<int16_t> get_shorts() const {
        std::vector<int16_t> result(size);
        for (int i = 0; i < size; ++i) {
            result[i] = static_cast<int16_t>(i % 1000);
        }
        return result;
    }

    // String vectors should still use element-by-element conversion
    std::vector<std::string> get_strings() const {
        std::vector<std::string> result;
        for (int i = 0; i < 5; ++i) {
            result.push_back("item_" + std::to_string(i));
        }
        return result;
    }
};
