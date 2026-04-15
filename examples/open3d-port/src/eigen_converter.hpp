#pragma once

// Custom type converters for Eigen types in mirror_bridge.
// Converts Eigen::VectorNd/VectorNi to/from Python lists, and
// std::vector<Eigen::VectorNd> to/from numpy arrays for bulk transfer.

#include <Python.h>
#include <Eigen/Dense>
#include <vector>

namespace mirror_bridge {

// ============================================================================
// Eigen::Vector3d <-> Python list [x, y, z]
// ============================================================================

inline PyObject* to_python(const Eigen::Vector3d& v) {
    PyObject* list = PyList_New(3);
    if (!list) return nullptr;
    PyList_SET_ITEM(list, 0, PyFloat_FromDouble(v.x()));
    PyList_SET_ITEM(list, 1, PyFloat_FromDouble(v.y()));
    PyList_SET_ITEM(list, 2, PyFloat_FromDouble(v.z()));
    return list;
}

inline bool from_python(PyObject* obj, Eigen::Vector3d& out) {
    if (!PySequence_Check(obj) || PySequence_Size(obj) != 3) return false;
    for (int i = 0; i < 3; i++) {
        PyObject* item = PySequence_GetItem(obj, i);
        if (!item) return false;
        double val = PyFloat_AsDouble(item);
        Py_DECREF(item);
        if (val == -1.0 && PyErr_Occurred()) return false;
        out[i] = val;
    }
    return true;
}

// ============================================================================
// Eigen::Vector3i <-> Python list [x, y, z]
// ============================================================================

inline PyObject* to_python(const Eigen::Vector3i& v) {
    PyObject* list = PyList_New(3);
    if (!list) return nullptr;
    PyList_SET_ITEM(list, 0, PyLong_FromLong(v.x()));
    PyList_SET_ITEM(list, 1, PyLong_FromLong(v.y()));
    PyList_SET_ITEM(list, 2, PyLong_FromLong(v.z()));
    return list;
}

inline bool from_python(PyObject* obj, Eigen::Vector3i& out) {
    if (!PySequence_Check(obj) || PySequence_Size(obj) != 3) return false;
    for (int i = 0; i < 3; i++) {
        PyObject* item = PySequence_GetItem(obj, i);
        if (!item) return false;
        long val = PyLong_AsLong(item);
        Py_DECREF(item);
        if (val == -1 && PyErr_Occurred()) return false;
        out[i] = static_cast<int>(val);
    }
    return true;
}

// ============================================================================
// Eigen::Vector2i <-> Python list [x, y]
// ============================================================================

inline PyObject* to_python(const Eigen::Vector2i& v) {
    PyObject* list = PyList_New(2);
    if (!list) return nullptr;
    PyList_SET_ITEM(list, 0, PyLong_FromLong(v.x()));
    PyList_SET_ITEM(list, 1, PyLong_FromLong(v.y()));
    return list;
}

inline bool from_python(PyObject* obj, Eigen::Vector2i& out) {
    if (!PySequence_Check(obj) || PySequence_Size(obj) != 2) return false;
    for (int i = 0; i < 2; i++) {
        PyObject* item = PySequence_GetItem(obj, i);
        if (!item) return false;
        long val = PyLong_AsLong(item);
        Py_DECREF(item);
        if (val == -1 && PyErr_Occurred()) return false;
        out[i] = static_cast<int>(val);
    }
    return true;
}

// ============================================================================
// std::vector<Eigen::VectorNd/Ni> <-> Python list of lists
// ============================================================================
//
// These use the existing Container to_python/from_python which will
// call the Eigen vector converters above for each element.
// For large arrays, a numpy-based bulk converter would be faster,
// but this is sufficient for the demo.

} // namespace mirror_bridge
