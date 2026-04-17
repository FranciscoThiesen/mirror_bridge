// End-to-end test for inherited method exposure via P2996 bases_of walking.
//
// Shape follows Open3D's geometry hierarchy: an abstract Geometry root, a
// middle Geometry3D layer, and concrete derived classes. mirror_bridge should
// expose every inherited method on the derived type, respecting C++ name
// hiding and virtual dispatch.
#pragma once

#include <Eigen/Dense>
#include <memory>
#include <string>
#include <vector>

namespace testing {

// Abstract root (like open3d::geometry::Geometry).
class Geometry {
public:
    virtual ~Geometry() = default;

    // Non-virtual inherited methods — exposed on every derived.
    std::string GetName() const { return name_; }
    void SetName(const std::string& n) { name_ = n; }

    // Pure virtual — derived must override.
    virtual int GeometryType() const = 0;

protected:
    std::string name_ = "unnamed";
};

// Abstract middle layer (like Geometry3D).
class Geometry3D : public Geometry {
public:
    // Concrete inherited method — every derived 3D geometry has dimension 3.
    int Dimension() const { return 3; }

    // Another abstract — derived must override.
    virtual Eigen::Vector3d GetCenter() const = 0;

    // A method with a default implementation that uses pure virtuals. Exposed
    // on derived classes and calls the override polymorphically.
    Eigen::Vector3d GetExtentFromCenter() const {
        Eigen::Vector3d c = GetCenter();
        return Eigen::Vector3d(std::abs(c.x()), std::abs(c.y()), std::abs(c.z()));
    }
};

// Concrete derived #1.
class PointCloud : public Geometry3D {
public:
    std::vector<Eigen::Vector3d> points;

    // Direct methods.
    int Size() const { return static_cast<int>(points.size()); }

    // Overrides of base virtuals.
    int GeometryType() const override { return 1; }  // hides Geometry's version
    Eigen::Vector3d GetCenter() const override {
        if (points.empty()) return Eigen::Vector3d::Zero();
        Eigen::Vector3d c = Eigen::Vector3d::Zero();
        for (const auto& p : points) c += p;
        return c / static_cast<double>(points.size());
    }
};

// Concrete derived #2 — different override of same virtual, same inherited
// non-virtuals. Both should be callable from Python with their own semantics.
class TriangleMesh : public Geometry3D {
public:
    std::vector<Eigen::Vector3d> vertices;
    std::vector<Eigen::Vector3i> triangles;

    int VertexCount() const { return static_cast<int>(vertices.size()); }
    int TriangleCount() const { return static_cast<int>(triangles.size()); }

    int GeometryType() const override { return 2; }
    Eigen::Vector3d GetCenter() const override {
        if (vertices.empty()) return Eigen::Vector3d::Zero();
        Eigen::Vector3d c = Eigen::Vector3d::Zero();
        for (const auto& v : vertices) c += v;
        return c / static_cast<double>(vertices.size());
    }
};

}  // namespace testing
