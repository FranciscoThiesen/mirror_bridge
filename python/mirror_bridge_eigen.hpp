#pragma once

// ============================================================================
// Mirror Bridge — Built-in Eigen Type Converters
// ============================================================================
//
// Automatically included when Eigen types are detected in bound classes.
// Provides to_python/from_python overloads for common Eigen vector and
// matrix types, enabling zero-effort binding of classes that use Eigen.
//
// Supported types:
//   Eigen::Vector2d, Vector2f, Vector2i
//   Eigen::Vector3d, Vector3f, Vector3i
//   Eigen::Vector4d, Vector4f, Vector4i
//   Eigen::Matrix3d, Matrix3f
//   Eigen::Matrix4d, Matrix4f
//
// Vectors convert to/from Python lists: [x, y, z]
// Matrices convert to/from nested lists: [[r0c0, r0c1, ...], ...]
//
// ============================================================================

#ifdef __has_include
#if __has_include(<Eigen/Dense>)
#define MIRROR_BRIDGE_HAS_EIGEN 1
#endif
#if __has_include(<Eigen/Core>)
#ifndef MIRROR_BRIDGE_HAS_EIGEN
#define MIRROR_BRIDGE_HAS_EIGEN 1
#endif
#endif
#endif

#ifdef MIRROR_BRIDGE_HAS_EIGEN

#include <Eigen/Core>
#include <Python.h>

namespace mirror_bridge {

// ============================================================================
// Helper: fixed-size Eigen vector <-> Python list
// ============================================================================

template<typename Scalar, int N>
inline PyObject* eigen_vector_to_python(const Eigen::Matrix<Scalar, N, 1>& v) {
    PyObject* list = PyList_New(N);
    if (!list) return nullptr;
    for (int i = 0; i < N; i++) {
        if constexpr (std::is_floating_point_v<Scalar>) {
            PyList_SET_ITEM(list, i, PyFloat_FromDouble(static_cast<double>(v[i])));
        } else {
            PyList_SET_ITEM(list, i, PyLong_FromLong(static_cast<long>(v[i])));
        }
    }
    return list;
}

template<typename Scalar, int N>
inline bool eigen_vector_from_python(PyObject* obj, Eigen::Matrix<Scalar, N, 1>& out) {
    if (!PySequence_Check(obj) || PySequence_Size(obj) != N) return false;
    for (int i = 0; i < N; i++) {
        PyObject* item = PySequence_GetItem(obj, i);
        if (!item) return false;
        if constexpr (std::is_floating_point_v<Scalar>) {
            double val = PyFloat_AsDouble(item);
            Py_DECREF(item);
            if (val == -1.0 && PyErr_Occurred()) return false;
            out[i] = static_cast<Scalar>(val);
        } else {
            long val = PyLong_AsLong(item);
            Py_DECREF(item);
            if (val == -1 && PyErr_Occurred()) return false;
            out[i] = static_cast<Scalar>(val);
        }
    }
    return true;
}

// ============================================================================
// Helper: fixed-size Eigen matrix <-> Python nested list
// ============================================================================

template<typename Scalar, int Rows, int Cols>
inline PyObject* eigen_matrix_to_python(const Eigen::Matrix<Scalar, Rows, Cols>& m) {
    PyObject* outer = PyList_New(Rows);
    if (!outer) return nullptr;
    for (int r = 0; r < Rows; r++) {
        PyObject* row = PyList_New(Cols);
        if (!row) { Py_DECREF(outer); return nullptr; }
        for (int c = 0; c < Cols; c++) {
            if constexpr (std::is_floating_point_v<Scalar>) {
                PyList_SET_ITEM(row, c, PyFloat_FromDouble(static_cast<double>(m(r, c))));
            } else {
                PyList_SET_ITEM(row, c, PyLong_FromLong(static_cast<long>(m(r, c))));
            }
        }
        PyList_SET_ITEM(outer, r, row);
    }
    return outer;
}

template<typename Scalar, int Rows, int Cols>
inline bool eigen_matrix_from_python(PyObject* obj, Eigen::Matrix<Scalar, Rows, Cols>& out) {
    if (!PySequence_Check(obj) || PySequence_Size(obj) != Rows) return false;
    for (int r = 0; r < Rows; r++) {
        PyObject* row = PySequence_GetItem(obj, r);
        if (!row || !PySequence_Check(row) || PySequence_Size(row) != Cols) {
            Py_XDECREF(row);
            return false;
        }
        for (int c = 0; c < Cols; c++) {
            PyObject* item = PySequence_GetItem(row, c);
            if (!item) { Py_DECREF(row); return false; }
            if constexpr (std::is_floating_point_v<Scalar>) {
                out(r, c) = static_cast<Scalar>(PyFloat_AsDouble(item));
            } else {
                out(r, c) = static_cast<Scalar>(PyLong_AsLong(item));
            }
            Py_DECREF(item);
        }
        Py_DECREF(row);
    }
    return true;
}

// ============================================================================
// Vector2
// ============================================================================

inline PyObject* to_python(const Eigen::Vector2d& v) { return eigen_vector_to_python(v); }
inline PyObject* to_python(const Eigen::Vector2f& v) { return eigen_vector_to_python(v); }
inline PyObject* to_python(const Eigen::Vector2i& v) { return eigen_vector_to_python(v); }
inline bool from_python(PyObject* o, Eigen::Vector2d& v) { return eigen_vector_from_python(o, v); }
inline bool from_python(PyObject* o, Eigen::Vector2f& v) { return eigen_vector_from_python(o, v); }
inline bool from_python(PyObject* o, Eigen::Vector2i& v) { return eigen_vector_from_python(o, v); }

// ============================================================================
// Vector3
// ============================================================================

inline PyObject* to_python(const Eigen::Vector3d& v) { return eigen_vector_to_python(v); }
inline PyObject* to_python(const Eigen::Vector3f& v) { return eigen_vector_to_python(v); }
inline PyObject* to_python(const Eigen::Vector3i& v) { return eigen_vector_to_python(v); }
inline bool from_python(PyObject* o, Eigen::Vector3d& v) { return eigen_vector_from_python(o, v); }
inline bool from_python(PyObject* o, Eigen::Vector3f& v) { return eigen_vector_from_python(o, v); }
inline bool from_python(PyObject* o, Eigen::Vector3i& v) { return eigen_vector_from_python(o, v); }

// ============================================================================
// Vector4
// ============================================================================

inline PyObject* to_python(const Eigen::Vector4d& v) { return eigen_vector_to_python(v); }
inline PyObject* to_python(const Eigen::Vector4f& v) { return eigen_vector_to_python(v); }
inline PyObject* to_python(const Eigen::Vector4i& v) { return eigen_vector_to_python(v); }
inline bool from_python(PyObject* o, Eigen::Vector4d& v) { return eigen_vector_from_python(o, v); }
inline bool from_python(PyObject* o, Eigen::Vector4f& v) { return eigen_vector_from_python(o, v); }
inline bool from_python(PyObject* o, Eigen::Vector4i& v) { return eigen_vector_from_python(o, v); }

// ============================================================================
// Matrix3
// ============================================================================

inline PyObject* to_python(const Eigen::Matrix3d& m) { return eigen_matrix_to_python(m); }
inline PyObject* to_python(const Eigen::Matrix3f& m) { return eigen_matrix_to_python(m); }
inline bool from_python(PyObject* o, Eigen::Matrix3d& m) { return eigen_matrix_from_python(o, m); }
inline bool from_python(PyObject* o, Eigen::Matrix3f& m) { return eigen_matrix_from_python(o, m); }

// ============================================================================
// Matrix4
// ============================================================================

inline PyObject* to_python(const Eigen::Matrix4d& m) { return eigen_matrix_to_python(m); }
inline PyObject* to_python(const Eigen::Matrix4f& m) { return eigen_matrix_to_python(m); }
inline bool from_python(PyObject* o, Eigen::Matrix4d& m) { return eigen_matrix_from_python(o, m); }
inline bool from_python(PyObject* o, Eigen::Matrix4f& m) { return eigen_matrix_from_python(o, m); }

} // namespace mirror_bridge

#endif // MIRROR_BRIDGE_HAS_EIGEN
