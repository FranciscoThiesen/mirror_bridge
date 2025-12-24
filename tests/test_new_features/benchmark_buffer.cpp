// Benchmark: Zero-Copy Buffer Views vs Element-by-Element Copy
//
// This demonstrates the performance difference between:
// 1. Element-by-element copy (naive approach)
// 2. Bulk memcpy (current optimization)
// 3. Zero-copy buffer protocol (new feature)

#include "mirror_bridge.hpp"
#include "python/mirror_bridge_buffer.hpp"
#include <vector>
#include <chrono>
#include <cstdint>

// Test class with different buffer return strategies
struct BufferBenchmark {
    std::vector<uint8_t> data;

    BufferBenchmark(size_t size = 1000000) : data(size) {
        // Initialize with some data
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<uint8_t>(i & 0xFF);
        }
    }

    size_t size() const { return data.size(); }

    // Method 1: Return by copy (uses current optimized bulk transfer)
    std::vector<uint8_t> get_data_copy() const {
        return data;  // Returns a copy, then bulk memcpy to Python
    }

    // Method 2: Return reference for zero-copy (buffer protocol)
    const std::vector<uint8_t>& get_data_ref() const {
        return data;  // Reference - can be used with buffer protocol
    }
};

// Custom to_python that returns a zero-copy buffer view
// This keeps the C++ object alive while Python accesses its memory
template<>
PyObject* mirror_bridge::to_python(const std::vector<uint8_t>& vec) {
    // For this benchmark, we'll use the optimized bulk copy
    // In production, you'd choose based on use case
    return mirror_bridge::buffer::to_memoryview(vec);
}

MIRROR_BRIDGE_MODULE(buffer_benchmark,
    mirror_bridge::bind_class<BufferBenchmark>(m, "BufferBenchmark");

    // Add timing utilities
    static auto time_operation = +[](PyObject* self, PyObject* args) -> PyObject* {
        PyObject* callable;
        int iterations = 100;

        if (!PyArg_ParseTuple(args, "O|i", &callable, &iterations)) {
            return nullptr;
        }

        // Warmup
        for (int i = 0; i < 3; ++i) {
            PyObject* result = PyObject_CallObject(callable, nullptr);
            Py_XDECREF(result);
        }

        // Timed runs
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            PyObject* result = PyObject_CallObject(callable, nullptr);
            Py_XDECREF(result);
        }
        auto end = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        return PyFloat_FromDouble(ms / iterations);
    };

    static PyMethodDef time_def = {"time_operation", time_operation, METH_VARARGS, nullptr};
    PyModule_AddObject(m, "time_operation", PyCFunction_New(&time_def, nullptr));
)
