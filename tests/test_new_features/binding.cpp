// Test binding for new Mirror Bridge features
// Tests: stubgen, options (exclude/rename/readonly), buffer views

#include "mirror_bridge.hpp"
#include "python/mirror_bridge_stubgen.hpp"
#include "python/mirror_bridge_options.hpp"
#include "python/mirror_bridge_buffer.hpp"
#include "test_class.hpp"

#include <cmath>  // for sqrt in Vec2

// ============================================================================
// Demonstrate stubgen integration
// ============================================================================

// Register stubs manually for demonstration
// In a real integration, this would be automatic from bind_class
void register_stubs_for_test() {
    using namespace mirror_bridge::stubgen;

    // TestClass stubs
    register_class_stub<TestClass>("TestClass");
    register_constructor_stub<int, std::string>("TestClass", {"id", "name"});
    register_constructor_stub<int, std::string, double>("TestClass", {"id", "name", "value"});
    register_property_stub<TestClass, int>("TestClass", "id");
    register_property_stub<TestClass, std::string>("TestClass", "name");
    register_property_stub<TestClass, double>("TestClass", "value");
    register_property_stub<TestClass, int>("TestClass", "member_count");  // renamed
    register_property_stub<TestClass, int>("TestClass", "computed_value", true);  // readonly
    register_method_stub<void, int>("TestClass", "set_id", {"new_id"});
    register_method_stub<int>("TestClass", "get_id");
    register_method_stub<void, std::string>("TestClass", "set_name", {"new_name"});
    register_method_stub<std::string>("TestClass", "get_name");
    register_method_stub<double, double, double>("TestClass", "calculate", {"x", "y"});
    register_method_stub<std::vector<uint8_t>>("TestClass", "get_bytes");
    register_method_stub<std::vector<float>>("TestClass", "get_floats");

    // Vec2 stubs
    register_class_stub<Vec2>("Vec2");
    register_constructor_stub<double, double>("Vec2", {"x", "y"});
    register_property_stub<Vec2, double>("Vec2", "x");
    register_property_stub<Vec2, double>("Vec2", "y");
    register_method_stub<double, Vec2>("Vec2", "dot", {"other"});
    register_method_stub<double>("Vec2", "length");
    register_method_stub<Vec2, Vec2>("Vec2", "add", {"other"});
}

// ============================================================================
// Module Definition
// ============================================================================

MIRROR_BRIDGE_MODULE(test_features,
    // Register TestClass with options
    auto test_class = mirror_bridge::bind_class<TestClass>(m, "TestClass");

    // Note: In a full implementation, these options would be applied during binding.
    // For now, we demonstrate the API - actual filtering requires modifying bind_class.
    // The options are stored and can be queried.
    mirror_bridge::OptionsRegistry::instance().get_options("TestClass")
        .renamed_members["memberCount"] = "member_count";
    mirror_bridge::OptionsRegistry::instance().get_options("TestClass")
        .readonly_members.insert("computedValue");

    // Register Vec2
    mirror_bridge::bind_class<Vec2>(m, "Vec2");

    // Register stubs
    register_stubs_for_test();

    // Add stub generation function to module
    static auto generate_stubs_func = +[](PyObject* self, PyObject* args) -> PyObject* {
        const char* filename = nullptr;
        if (!PyArg_ParseTuple(args, "s", &filename)) {
            return nullptr;
        }

        bool success = mirror_bridge::stubgen::write_stubs("test_features", filename);
        if (success) {
            Py_RETURN_TRUE;
        } else {
            Py_RETURN_FALSE;
        }
    };

    static PyMethodDef generate_stubs_def = {
        "generate_stubs",
        generate_stubs_func,
        METH_VARARGS,
        "Generate .pyi stub file"
    };

    PyObject* func = PyCFunction_New(&generate_stubs_def, nullptr);
    PyModule_AddObject(m, "generate_stubs", func);

    // Add get_stub_content function
    static auto get_stubs_func = +[](PyObject* self, PyObject* args) -> PyObject* {
        std::string content = mirror_bridge::stubgen::generate_stubs("test_features");
        return PyUnicode_FromString(content.c_str());
    };

    static PyMethodDef get_stubs_def = {
        "get_stub_content",
        get_stubs_func,
        METH_NOARGS,
        "Get stub content as string"
    };

    PyObject* get_func = PyCFunction_New(&get_stubs_def, nullptr);
    PyModule_AddObject(m, "get_stub_content", get_func);
)
