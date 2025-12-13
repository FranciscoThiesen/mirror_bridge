// Test binding for free functions
#include "freefunc_test.hpp"
#include "mirror_bridge.hpp"

using namespace freefunc_test;

PyMODINIT_FUNC PyInit_freefunc_test(void) {
    static struct PyModuleDef module_def = {
        PyModuleDef_HEAD_INIT,
        "freefunc_test",
        "Test module for free function binding",
        -1,
        nullptr
    };

    PyObject* module = PyModule_Create(&module_def);
    if (!module) return nullptr;

    // Bind the Point class (needed for some free functions)
    mirror_bridge::bind_class<Point>(module, "Point");

    // Bind free functions
    mirror_bridge::bind_function<&add>(module, "add");
    mirror_bridge::bind_function<&multiply>(module, "multiply");
    mirror_bridge::bind_function<&greet>(module, "greet");
    mirror_bridge::bind_function<&get_pi>(module, "get_pi");
    mirror_bridge::bind_function<&increment_counter>(module, "increment_counter");
    mirror_bridge::bind_function<&get_counter>(module, "get_counter");
    mirror_bridge::bind_function<&reset_counter>(module, "reset_counter");
    mirror_bridge::bind_function<&sum_vector>(module, "sum_vector");
    mirror_bridge::bind_function<&distance>(module, "distance");
    mirror_bridge::bind_function<&range>(module, "range");
    mirror_bridge::bind_function<&point_distance>(module, "point_distance");
    mirror_bridge::bind_function<&make_point>(module, "make_point");

    return module;
}
