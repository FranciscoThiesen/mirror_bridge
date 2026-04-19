#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/eigen.h>
#include "geometry_min.hpp"
namespace py = pybind11;
using namespace demo;

PYBIND11_MODULE(pc_pybind, m) {
    py::class_<PointCloud>(m, "PointCloud")
        .def(py::init<>())
        .def(py::init<const std::vector<Eigen::Vector3d>&>())
        .def("get_center", &PointCloud::get_center)
        .def("size", &PointCloud::size)
        .def_readwrite("points", &PointCloud::points);
}
