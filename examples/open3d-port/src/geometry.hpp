#pragma once

// Open3D-style geometry classes using Eigen — mirrors the real Open3D API
// for demonstrating mirror_bridge binding generation.
//
// Class hierarchy (matches Open3D):
//   Geometry3D
//     ├── PointCloud
//     ├── LineSet
//     ├── AxisAlignedBoundingBox
//     └── TriangleMesh

#include <Eigen/Dense>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <memory>

namespace open3d {
namespace geometry {

// ============================================================================
// AxisAlignedBoundingBox
// ============================================================================

struct AxisAlignedBoundingBox {
    Eigen::Vector3d min_bound = Eigen::Vector3d(0, 0, 0);
    Eigen::Vector3d max_bound = Eigen::Vector3d(0, 0, 0);
    Eigen::Vector3d color = Eigen::Vector3d(1, 0, 0);

    AxisAlignedBoundingBox() = default;
    AxisAlignedBoundingBox(const Eigen::Vector3d& min_b, const Eigen::Vector3d& max_b)
        : min_bound(min_b), max_bound(max_b) {}

    Eigen::Vector3d get_center() const { return (min_bound + max_bound) * 0.5; }
    Eigen::Vector3d get_extent() const { return max_bound - min_bound; }
    double volume() const {
        auto e = get_extent();
        return e.x() * e.y() * e.z();
    }

    Eigen::Vector3d get_half_extent() const { return get_extent() * 0.5; }
    std::string to_string() const { return "AxisAlignedBoundingBox"; }
};

// ============================================================================
// PointCloud
// ============================================================================

struct PointCloud {
    std::vector<Eigen::Vector3d> points;
    std::vector<Eigen::Vector3d> normals;
    std::vector<Eigen::Vector3d> colors;

    PointCloud() = default;

    bool has_points() const { return !points.empty(); }
    bool has_normals() const { return normals.size() == points.size() && !normals.empty(); }
    bool has_colors() const { return colors.size() == points.size() && !colors.empty(); }
    bool is_empty() const { return points.empty(); }
    int size() const { return static_cast<int>(points.size()); }

    Eigen::Vector3d get_center() const {
        if (points.empty()) return Eigen::Vector3d::Zero();
        Eigen::Vector3d center = Eigen::Vector3d::Zero();
        for (const auto& p : points) center += p;
        return center / static_cast<double>(points.size());
    }

    Eigen::Vector3d get_min_bound() const {
        if (points.empty()) return Eigen::Vector3d::Zero();
        Eigen::Vector3d min_b = points[0];
        for (const auto& p : points) min_b = min_b.cwiseMin(p);
        return min_b;
    }

    Eigen::Vector3d get_max_bound() const {
        if (points.empty()) return Eigen::Vector3d::Zero();
        Eigen::Vector3d max_b = points[0];
        for (const auto& p : points) max_b = max_b.cwiseMax(p);
        return max_b;
    }

    AxisAlignedBoundingBox get_axis_aligned_bounding_box() const {
        return AxisAlignedBoundingBox(get_min_bound(), get_max_bound());
    }

    void translate(const Eigen::Vector3d& translation) {
        for (auto& p : points) p += translation;
    }

    void scale(double s, const Eigen::Vector3d& center) {
        for (auto& p : points) p = center + s * (p - center);
    }

    void paint_uniform_color(const Eigen::Vector3d& c) {
        colors.assign(points.size(), c);
    }

    void clear() {
        points.clear();
        normals.clear();
        colors.clear();
    }

    std::string to_string() const {
        return "PointCloud with " + std::to_string(points.size()) + " points";
    }
};

// ============================================================================
// LineSet
// ============================================================================

struct LineSet {
    std::vector<Eigen::Vector3d> points;
    std::vector<Eigen::Vector2i> lines;
    std::vector<Eigen::Vector3d> colors;

    LineSet() = default;

    bool has_points() const { return !points.empty(); }
    bool has_lines() const { return !lines.empty(); }
    bool has_colors() const { return colors.size() == lines.size() && !colors.empty(); }
    bool is_empty() const { return points.empty(); }

    Eigen::Vector3d get_center() const {
        if (points.empty()) return Eigen::Vector3d::Zero();
        Eigen::Vector3d center = Eigen::Vector3d::Zero();
        for (const auto& p : points) center += p;
        return center / static_cast<double>(points.size());
    }

    void paint_uniform_color(const Eigen::Vector3d& c) {
        colors.assign(lines.size(), c);
    }

    void clear() {
        points.clear();
        lines.clear();
        colors.clear();
    }

    std::string to_string() const {
        return "LineSet with " + std::to_string(lines.size()) + " lines";
    }
};

// ============================================================================
// TriangleMesh
// ============================================================================

struct TriangleMesh {
    std::vector<Eigen::Vector3d> vertices;
    std::vector<Eigen::Vector3d> vertex_normals;
    std::vector<Eigen::Vector3d> vertex_colors;
    std::vector<Eigen::Vector3i> triangles;
    std::vector<Eigen::Vector3d> triangle_normals;

    TriangleMesh() = default;

    bool has_vertices() const { return !vertices.empty(); }
    bool has_vertex_normals() const { return vertex_normals.size() == vertices.size(); }
    bool has_vertex_colors() const { return vertex_colors.size() == vertices.size(); }
    bool has_triangles() const { return !triangles.empty(); }
    bool has_triangle_normals() const { return triangle_normals.size() == triangles.size(); }
    bool is_empty() const { return vertices.empty(); }

    int num_vertices() const { return static_cast<int>(vertices.size()); }
    int num_triangles() const { return static_cast<int>(triangles.size()); }

    Eigen::Vector3d get_center() const {
        if (vertices.empty()) return Eigen::Vector3d::Zero();
        Eigen::Vector3d center = Eigen::Vector3d::Zero();
        for (const auto& v : vertices) center += v;
        return center / static_cast<double>(vertices.size());
    }

    void translate(const Eigen::Vector3d& translation) {
        for (auto& v : vertices) v += translation;
    }

    void scale(double s, const Eigen::Vector3d& center) {
        for (auto& v : vertices) v = center + s * (v - center);
    }

    void paint_uniform_color(const Eigen::Vector3d& c) {
        vertex_colors.assign(vertices.size(), c);
    }

    void clear() {
        vertices.clear();
        vertex_normals.clear();
        vertex_colors.clear();
        triangles.clear();
        triangle_normals.clear();
    }

    double get_surface_area() const {
        double area = 0;
        for (const auto& tri : triangles) {
            const auto& v0 = vertices[tri[0]];
            const auto& v1 = vertices[tri[1]];
            const auto& v2 = vertices[tri[2]];
            area += 0.5 * (v1 - v0).cross(v2 - v0).norm();
        }
        return area;
    }

    std::string to_string() const {
        return "TriangleMesh with " + std::to_string(vertices.size()) +
               " vertices and " + std::to_string(triangles.size()) + " triangles";
    }
};

} // namespace geometry
} // namespace open3d
