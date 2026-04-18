// Comprehensive Open3D-style geometry library exercising every mirror_bridge
// feature: inheritance, virtual overrides, operators, kwargs+defaults, nested
// classes, polymorphic dispatch via trampoline. Zero binding glue required.
#pragma once

#include <Eigen/Dense>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace open3d {
namespace geometry {

// Abstract root matching Open3D::Geometry.
class Geometry {
public:
    enum GeometryType { Unspecified = 0, PointCloud = 1, LineSet = 2, TriangleMesh = 3 };

    Geometry() = default;
    virtual ~Geometry() = default;

    virtual GeometryType get_geometry_type() const = 0;
    virtual bool is_empty() const = 0;

    // Shared non-virtual method — every derived gets it via reflection.
    int dimension() const { return 3; }
};

// Concrete middle layer — Open3D::Geometry3D shape with default virtual
// implementations. Auto-trampoline works here because there's nothing pure.
class Geometry3D : public Geometry {
public:
    virtual Eigen::Vector3d get_center() const { return Eigen::Vector3d::Zero(); }
    virtual Eigen::Vector3d get_min_bound() const { return Eigen::Vector3d::Zero(); }
    virtual Eigen::Vector3d get_max_bound() const { return Eigen::Vector3d::Zero(); }

    GeometryType get_geometry_type() const override { return Unspecified; }
    bool is_empty() const override { return true; }

    // Polymorphic method using virtual calls — a Python override of
    // get_center() reaches THIS method's computation via the trampoline.
    Eigen::Vector3d get_extent() const {
        return get_max_bound() - get_min_bound();
    }
};

class AxisAlignedBoundingBox : public Geometry3D {
public:
    Eigen::Vector3d min_bound = Eigen::Vector3d::Zero();
    Eigen::Vector3d max_bound = Eigen::Vector3d::Zero();
    Eigen::Vector3d color = Eigen::Vector3d(1, 0, 0);

    AxisAlignedBoundingBox() = default;
    AxisAlignedBoundingBox(const Eigen::Vector3d& min_b, const Eigen::Vector3d& max_b)
        : min_bound(min_b), max_bound(max_b) {}

    // Virtual overrides — plumbed to Python if subclassed.
    Eigen::Vector3d get_center() const override { return (min_bound + max_bound) * 0.5; }
    Eigen::Vector3d get_min_bound() const override { return min_bound; }
    Eigen::Vector3d get_max_bound() const override { return max_bound; }

    double volume() const {
        auto e = get_max_bound() - get_min_bound();
        return e.x() * e.y() * e.z();
    }

    // Default args — voxel_size has a reasonable default; invert is a switch.
    int count_voxels(double voxel_size = 0.05, bool invert = false) const {
        auto e = get_max_bound() - get_min_bound();
        int n = int(std::ceil(e.x() / voxel_size)) *
                int(std::ceil(e.y() / voxel_size)) *
                int(std::ceil(e.z() / voxel_size));
        return invert ? -n : n;
    }

    // Operators: Python gets __add__ (union), __eq__, __len__-ish via volume.
    AxisAlignedBoundingBox operator+(const AxisAlignedBoundingBox& o) const {
        return AxisAlignedBoundingBox(
            min_bound.cwiseMin(o.min_bound),
            max_bound.cwiseMax(o.max_bound));
    }
    bool operator==(const AxisAlignedBoundingBox& o) const {
        return min_bound == o.min_bound && max_bound == o.max_bound;
    }

    bool is_empty() const override {
        return (max_bound - min_bound).minCoeff() <= 0;
    }
};

class PointCloud : public Geometry3D {
public:
    std::vector<Eigen::Vector3d> points;
    std::vector<Eigen::Vector3d> normals;
    std::vector<Eigen::Vector3d> colors;

    PointCloud() = default;
    explicit PointCloud(const std::vector<Eigen::Vector3d>& pts) : points(pts) {}

    // Overrides of the abstract interface.
    GeometryType get_geometry_type() const override { return Geometry::PointCloud; }
    bool is_empty() const override { return points.empty(); }

    Eigen::Vector3d get_center() const override {
        if (points.empty()) return Eigen::Vector3d::Zero();
        Eigen::Vector3d c = Eigen::Vector3d::Zero();
        for (const auto& p : points) c += p;
        return c / static_cast<double>(points.size());
    }

    Eigen::Vector3d get_min_bound() const override {
        if (points.empty()) return Eigen::Vector3d::Zero();
        Eigen::Vector3d m = points[0];
        for (const auto& p : points) m = m.cwiseMin(p);
        return m;
    }

    Eigen::Vector3d get_max_bound() const override {
        if (points.empty()) return Eigen::Vector3d::Zero();
        Eigen::Vector3d m = points[0];
        for (const auto& p : points) m = m.cwiseMax(p);
        return m;
    }

    int size() const { return static_cast<int>(points.size()); }

    AxisAlignedBoundingBox get_axis_aligned_bounding_box() const {
        return AxisAlignedBoundingBox(get_min_bound(), get_max_bound());
    }

    // Default arg — Open3D-idiomatic kwarg.
    PointCloud voxel_down_sample(double voxel_size = 0.05) const {
        PointCloud out;
        for (size_t i = 0; i < points.size(); ++i) {
            if (static_cast<double>(i) * voxel_size < 1.0) out.points.push_back(points[i]);
        }
        return out;
    }

    // Multiple kwargs with defaults — mimics estimate_normals.
    PointCloud estimate_normals(double radius = 0.1, int max_nn = 30,
                                bool fast_normal_computation = true) const {
        PointCloud out = *this;
        out.normals.clear();
        for (size_t i = 0; i < points.size(); ++i) {
            out.normals.push_back(Eigen::Vector3d(0, 0, (double)max_nn * radius));
        }
        (void)fast_normal_computation;
        return out;
    }

    void paint_uniform_color(const Eigen::Vector3d& c) {
        colors.assign(points.size(), c);
    }

    // Operators — `pcd1 + pcd2` merges.
    PointCloud operator+(const PointCloud& o) const {
        PointCloud r = *this;
        r.points.insert(r.points.end(), o.points.begin(), o.points.end());
        r.normals.insert(r.normals.end(), o.normals.begin(), o.normals.end());
        r.colors.insert(r.colors.end(), o.colors.begin(), o.colors.end());
        return r;
    }
    PointCloud& operator+=(const PointCloud& o) {
        points.insert(points.end(), o.points.begin(), o.points.end());
        normals.insert(normals.end(), o.normals.begin(), o.normals.end());
        colors.insert(colors.end(), o.colors.begin(), o.colors.end());
        return *this;
    }
    bool operator==(const PointCloud& o) const { return points == o.points; }
};

}  // namespace geometry
}  // namespace open3d
