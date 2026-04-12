#pragma once

#include <cmath>
#include <vector>

struct Statistics {
    double mean = 0.0;
    double stddev = 0.0;
    int count = 0;

    void add_samples(const std::vector<double>& samples) {
        for (double s : samples) {
            count++;
            double delta = s - mean;
            mean += delta / count;
            double delta2 = s - mean;
            stddev += delta * delta2;
        }
    }

    double get_variance() const {
        return count > 1 ? stddev / (count - 1) : 0.0;
    }

    double get_stddev() const {
        return std::sqrt(get_variance());
    }
};
