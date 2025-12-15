// Test binding for static constexpr members
#include "constexpr_test.hpp"
#include "mirror_bridge.hpp"

using namespace constexpr_test;

PyMODINIT_FUNC PyInit_constexpr(void) {
    static struct PyModuleDef module_def = {
        PyModuleDef_HEAD_INIT,
        "constexpr",
        "Test module for static constexpr member binding",
        -1,
        nullptr
    };

    PyObject* module = PyModule_Create(&module_def);
    if (!module) return nullptr;

    // Bind PhysicsConstants with static constexpr members
    mirror_bridge::bind_class<PhysicsConstants>(module, "PhysicsConstants");

    // Bind Config with static constexpr members
    mirror_bridge::bind_class<Config>(module, "Config");

    return module;
}
