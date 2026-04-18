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

    // Real voxel downsampling: grid the points and keep at most one per cell.
    PointCloud voxel_down_sample(double voxel_size = 0.05) const {
        PointCloud out;
        if (voxel_size <= 0 || points.empty()) return out;
        std::vector<std::tuple<int, int, int>> seen;
        auto make_key = [&](const Eigen::Vector3d& p) {
            return std::make_tuple(
                int(std::floor(p.x() / voxel_size)),
                int(std::floor(p.y() / voxel_size)),
                int(std::floor(p.z() / voxel_size)));
        };
        for (size_t i = 0; i < points.size(); ++i) {
            auto k = make_key(points[i]);
            bool found = false;
            for (const auto& s : seen) { if (s == k) { found = true; break; } }
            if (!found) {
                seen.push_back(k);
                out.points.push_back(points[i]);
                if (i < normals.size()) out.normals.push_back(normals[i]);
                if (i < colors.size()) out.colors.push_back(colors[i]);
            }
        }
        return out;
    }

    // Compute per-point normals by looking at a point's offset from the cloud
    // centroid — simple, illustrative, good for visualization.
    PointCloud estimate_normals(double radius = 0.1, int max_nn = 30,
                                bool fast_normal_computation = true) const {
        PointCloud out = *this;
        out.normals.clear();
        Eigen::Vector3d center = get_center();
        for (const auto& p : points) {
            Eigen::Vector3d d = p - center;
            double n = d.norm();
            out.normals.push_back(n > 1e-9 ? d / n : Eigen::Vector3d(0, 0, 1));
        }
        (void)radius; (void)max_nn; (void)fast_normal_computation;
        return out;
    }

    void paint_uniform_color(const Eigen::Vector3d& c) {
        colors.assign(points.size(), c);
    }

    // Point-cloud merge via operator overloads.
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

// Free functions matching Open3D's factory style — sphere / torus meshes as
// PointClouds (we're demonstrating the point-cloud plumbing, not rendering).
inline PointCloud create_sphere(double radius = 1.0, int resolution = 30) {
    PointCloud pc;
    for (int i = 0; i <= resolution; ++i) {
        double phi = M_PI * i / resolution;
        for (int j = 0; j < resolution * 2; ++j) {
            double theta = 2 * M_PI * j / (resolution * 2);
            double x = radius * std::sin(phi) * std::cos(theta);
            double y = radius * std::sin(phi) * std::sin(theta);
            double z = radius * std::cos(phi);
            pc.points.push_back(Eigen::Vector3d(x, y, z));
        }
    }
    return pc;
}

inline PointCloud create_torus(double outer_radius = 1.0,
                                double inner_radius = 0.3,
                                int major_res = 40,
                                int minor_res = 20) {
    PointCloud pc;
    for (int i = 0; i < major_res; ++i) {
        double u = 2 * M_PI * i / major_res;
        for (int j = 0; j < minor_res; ++j) {
            double v = 2 * M_PI * j / minor_res;
            double cx = (outer_radius + inner_radius * std::cos(v)) * std::cos(u);
            double cy = (outer_radius + inner_radius * std::cos(v)) * std::sin(u);
            double cz = inner_radius * std::sin(v);
            pc.points.push_back(Eigen::Vector3d(cx, cy, cz));
        }
    }
    return pc;
}

}  // namespace geometry
}  // namespace open3d
