// Python binding for Pixel with custom Color3 converter
// Demonstrates the custom type converter extension point

#include "color3.hpp"

// Include Python API first
#include <Python.h>

// Forward declare the custom converter BEFORE including mirror_bridge
namespace mirror_bridge {

// METHOD 1: Function overloads - simpler approach
// Define to_python/from_python for Color3 type

inline PyObject* to_python(const Color3& color) {
    // Convert Color3 to Python list [r, g, b]
    PyObject* list = PyList_New(3);
    if (!list) return nullptr;

    PyList_SET_ITEM(list, 0, PyLong_FromLong(color.r));
    PyList_SET_ITEM(list, 1, PyLong_FromLong(color.g));
    PyList_SET_ITEM(list, 2, PyLong_FromLong(color.b));

    return list;
}

inline bool from_python(PyObject* obj, Color3& color) {
    // Convert Python list/tuple [r, g, b] to Color3
    if (PyList_Check(obj)) {
        if (PyList_Size(obj) != 3) return false;
        color.r = static_cast<uint8_t>(PyLong_AsLong(PyList_GetItem(obj, 0)));
        color.g = static_cast<uint8_t>(PyLong_AsLong(PyList_GetItem(obj, 1)));
        color.b = static_cast<uint8_t>(PyLong_AsLong(PyList_GetItem(obj, 2)));
        return !PyErr_Occurred();
    }
    if (PyTuple_Check(obj)) {
        if (PyTuple_Size(obj) != 3) return false;
        color.r = static_cast<uint8_t>(PyLong_AsLong(PyTuple_GetItem(obj, 0)));
        color.g = static_cast<uint8_t>(PyLong_AsLong(PyTuple_GetItem(obj, 1)));
        color.b = static_cast<uint8_t>(PyLong_AsLong(PyTuple_GetItem(obj, 2)));
        return !PyErr_Occurred();
    }
    // Also accept dict with r, g, b keys
    if (PyDict_Check(obj)) {
        PyObject* r = PyDict_GetItemString(obj, "r");
        PyObject* g = PyDict_GetItemString(obj, "g");
        PyObject* b = PyDict_GetItemString(obj, "b");
        if (!r || !g || !b) return false;
        color.r = static_cast<uint8_t>(PyLong_AsLong(r));
        color.g = static_cast<uint8_t>(PyLong_AsLong(g));
        color.b = static_cast<uint8_t>(PyLong_AsLong(b));
        return !PyErr_Occurred();
    }
    return false;
}

} // namespace mirror_bridge

// Now include mirror_bridge - it will find our custom converters
#include "mirror_bridge.hpp"

MIRROR_BRIDGE_MODULE(pixel,
    mirror_bridge::bind_class<Pixel>(m, "Pixel");
)
