// pybind11 equivalent of the mirror_bridge open3d_geometry binding.
// Uses the same geometry classes for an apples-to-apples performance comparison.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/eigen.h>
#include "../src/geometry.hpp"

namespace py = pybind11;
using namespace open3d::geometry;

PYBIND11_MODULE(open3d_geometry_pybind, m) {
    py::class_<AxisAlignedBoundingBox>(m, "AxisAlignedBoundingBox")
        .def(py::init<>())
        .def(py::init<const Eigen::Vector3d&, const Eigen::Vector3d&>())
        .def_readwrite("min_bound", &AxisAlignedBoundingBox::min_bound)
        .def_readwrite("max_bound", &AxisAlignedBoundingBox::max_bound)
        .def_readwrite("color", &AxisAlignedBoundingBox::color)
        .def("get_center", &AxisAlignedBoundingBox::get_center)
        .def("get_extent", &AxisAlignedBoundingBox::get_extent)
        .def("get_half_extent", &AxisAlignedBoundingBox::get_half_extent)
        .def("volume", &AxisAlignedBoundingBox::volume)
        .def("to_string", &AxisAlignedBoundingBox::to_string);

    py::class_<PointCloud>(m, "PointCloud")
        .def(py::init<>())
        .def_readwrite("points", &PointCloud::points)
        .def_readwrite("normals", &PointCloud::normals)
        .def_readwrite("colors", &PointCloud::colors)
        .def("has_points", &PointCloud::has_points)
        .def("has_normals", &PointCloud::has_normals)
        .def("has_colors", &PointCloud::has_colors)
        .def("is_empty", &PointCloud::is_empty)
        .def("size", &PointCloud::size)
        .def("get_center", &PointCloud::get_center)
        .def("get_min_bound", &PointCloud::get_min_bound)
        .def("get_max_bound", &PointCloud::get_max_bound)
        .def("get_axis_aligned_bounding_box", &PointCloud::get_axis_aligned_bounding_box)
        .def("translate", &PointCloud::translate)
        .def("scale", &PointCloud::scale)
        .def("paint_uniform_color", &PointCloud::paint_uniform_color)
        .def("clear", &PointCloud::clear)
        .def("to_string", &PointCloud::to_string);

    py::class_<LineSet>(m, "LineSet")
        .def(py::init<>())
        .def_readwrite("points", &LineSet::points)
        .def_readwrite("lines", &LineSet::lines)
        .def_readwrite("colors", &LineSet::colors)
        .def("has_points", &LineSet::has_points)
        .def("has_lines", &LineSet::has_lines)
        .def("has_colors", &LineSet::has_colors)
        .def("is_empty", &LineSet::is_empty)
        .def("get_center", &LineSet::get_center)
        .def("paint_uniform_color", &LineSet::paint_uniform_color)
        .def("clear", &LineSet::clear)
        .def("to_string", &LineSet::to_string);

    py::class_<TriangleMesh>(m, "TriangleMesh")
        .def(py::init<>())
        .def_readwrite("vertices", &TriangleMesh::vertices)
        .def_readwrite("vertex_normals", &TriangleMesh::vertex_normals)
        .def_readwrite("vertex_colors", &TriangleMesh::vertex_colors)
        .def_readwrite("triangles", &TriangleMesh::triangles)
        .def_readwrite("triangle_normals", &TriangleMesh::triangle_normals)
        .def("has_vertices", &TriangleMesh::has_vertices)
        .def("has_vertex_normals", &TriangleMesh::has_vertex_normals)
        .def("has_vertex_colors", &TriangleMesh::has_vertex_colors)
        .def("has_triangles", &TriangleMesh::has_triangles)
        .def("has_triangle_normals", &TriangleMesh::has_triangle_normals)
        .def("is_empty", &TriangleMesh::is_empty)
        .def("num_vertices", &TriangleMesh::num_vertices)
        .def("num_triangles", &TriangleMesh::num_triangles)
        .def("get_center", &TriangleMesh::get_center)
        .def("translate", &TriangleMesh::translate)
        .def("scale", &TriangleMesh::scale)
        .def("paint_uniform_color", &TriangleMesh::paint_uniform_color)
        .def("clear", &TriangleMesh::clear)
        .def("get_surface_area", &TriangleMesh::get_surface_area)
        .def("to_string", &TriangleMesh::to_string);
}
