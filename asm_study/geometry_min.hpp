#pragma once
#include <Eigen/Dense>
#include <vector>

namespace demo {

class PointCloud {
public:
    std::vector<Eigen::Vector3d> points;

    PointCloud() = default;
    explicit PointCloud(const std::vector<Eigen::Vector3d>& pts) : points(pts) {}

    Eigen::Vector3d get_center() const {
        if (points.empty()) return Eigen::Vector3d::Zero();
        Eigen::Vector3d c = Eigen::Vector3d::Zero();
        for (const auto& p : points) c += p;
        return c / static_cast<double>(points.size());
    }

    int size() const { return static_cast<int>(points.size()); }
};

}  // namespace demo
