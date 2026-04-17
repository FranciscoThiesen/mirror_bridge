// Regression test for keyword arguments and default parameter values —
// both derived from reflection (has_default_argument + identifier_of).
#pragma once
#include <string>

namespace api {

class PointCloud {
public:
    PointCloud() = default;

    // Open3D-style: required + defaulted args, all meaningfully named.
    PointCloud voxel_down_sample(double voxel_size) const {
        PointCloud out;
        out.last_voxel_size_ = voxel_size;
        return out;
    }

    std::string select_by_index(int index, bool invert = false) const {
        return (invert ? "invert_" : "") + std::to_string(index);
    }

    int estimate_normals(double radius = 0.1, int max_nn = 30,
                          bool fast_computation = true) const {
        return fast_computation ? max_nn : (max_nn + (int)(radius * 100));
    }

    double last_voxel_size_ = 0.0;
};

}  // namespace api
