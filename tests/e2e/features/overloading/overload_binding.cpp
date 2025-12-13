// Test binding for method overloading dispatch
#include "overload_test.hpp"
#include "mirror_bridge.hpp"

using namespace overload_test;

PyMODINIT_FUNC PyInit_overload_test(void) {
    static struct PyModuleDef module_def = {
        PyModuleDef_HEAD_INIT,
        "overload_test",
        "Test module for method overloading dispatch",
        -1,
        nullptr
    };

    PyObject* module = PyModule_Create(&module_def);
    if (!module) return nullptr;

    // Bind Calculator with overloaded add() and set() methods
    mirror_bridge::bind_class<Calculator>(module, "Calculator");

    // Bind Point with static factory methods
    mirror_bridge::bind_class<Point>(module, "Point");

    return module;
}
