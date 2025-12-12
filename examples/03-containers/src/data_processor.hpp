#pragma once
#include <vector>
#include <array>
#include <string>
#include <numeric>

// Demonstrates container support
struct DataProcessor {
    std::vector<double> values;
    std::vector<std::string> labels;
    std::array<int, 3> dimensions = {0, 0, 0};

    void add_value(double v) {
        values.push_back(v);
    }

    void add_label(std::string label) {
        labels.push_back(label);
    }

    double sum() const {
        return std::accumulate(values.begin(), values.end(), 0.0);
    }

    double average() const {
        if (values.empty()) return 0.0;
        return sum() / values.size();
    }

    int value_count() const {
        return static_cast<int>(values.size());
    }

    int label_count() const {
        return static_cast<int>(labels.size());
    }

    std::vector<double> get_values() const {
        return values;
    }

    std::vector<std::string> get_labels() const {
        return labels;
    }

    void set_dimensions(int x, int y, int z) {
        dimensions[0] = x;
        dimensions[1] = y;
        dimensions[2] = z;
    }

    int volume() const {
        return dimensions[0] * dimensions[1] * dimensions[2];
    }

    void clear() {
        values.clear();
        labels.clear();
        dimensions = {0, 0, 0};
    }
};
