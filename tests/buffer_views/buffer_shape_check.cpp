// Two buffer views of the same container type, with a Py_buffer held on each
// at the same time.
//
// This is not a binding. It is a standalone program, because the bug it covers
// is invisible from Python: memoryview copies shape and strides out of the
// Py_buffer the moment it is created, and numpy acquires and releases one
// buffer per call, so neither ever holds two at once. A C extension reading two
// arrays together does, and the buffer protocol explicitly allows it.
//
// What used to happen: shape and strides were function-local statics shared by
// every view of a given container type, so the second getbuffer rewrote what
// the first view reported. A view of 3 doubles went on to describe itself as
// 1000 elements over its 24-byte allocation, while its own len stayed 24. A
// consumer that trusts shape -- which the protocol tells it to -- reads 8000
// bytes out of 24.

#include "python/mirror_bridge_buffer.hpp"

#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("  %s %s\n", ok ? "\xE2\x9C\x93" : "\xE2\x9C\x97", what);
    if (!ok) ++failures;
}

}  // namespace

int main() {
    Py_Initialize();

    std::vector<double> small(3, 1.0);
    std::vector<double> big(1000, 2.0);

    PyObject* v_small = mirror_bridge::buffer::create_view_borrowed(small, nullptr);
    PyObject* v_big = mirror_bridge::buffer::create_view_borrowed(big, nullptr);
    if (!v_small || !v_big) {
        PyErr_Print();
        return 1;
    }

    std::puts("two Py_buffers over one container type, held at once:");

    Py_buffer b_small{};
    Py_buffer b_big{};
    if (PyObject_GetBuffer(v_small, &b_small, PyBUF_FULL_RO) < 0) {
        PyErr_Print();
        return 1;
    }
    check(b_small.shape[0] == 3, "the first view describes 3 elements");

    if (PyObject_GetBuffer(v_big, &b_big, PyBUF_FULL_RO) < 0) {
        PyErr_Print();
        return 1;
    }
    check(b_small.shape[0] == 3, "and still does after a second view is acquired");
    check(b_big.shape[0] == 1000, "the second view describes 1000 elements");
    check(b_small.shape != b_big.shape, "the two views do not share a shape pointer");
    check(b_small.strides != b_big.strides, "nor a strides pointer");
    check(b_small.shape[0] * b_small.itemsize == b_small.len,
          "shape and len agree on the first view");
    check(b_big.shape[0] * b_big.itemsize == b_big.len,
          "shape and len agree on the second");

    PyBuffer_Release(&b_small);
    PyBuffer_Release(&b_big);

    // The protocol requires the fields the consumer did not ask for to be
    // null, not helpfully filled in anyway.
    std::puts("a request that asks for less gets less:");

    Py_buffer simple{};
    if (PyObject_GetBuffer(v_small, &simple, PyBUF_SIMPLE) < 0) {
        PyErr_Print();
        return 1;
    }
    check(simple.format == nullptr, "PyBUF_SIMPLE leaves format null");
    check(simple.shape == nullptr, "PyBUF_SIMPLE leaves shape null");
    check(simple.strides == nullptr, "PyBUF_SIMPLE leaves strides null");
    check(simple.len == 24, "and still reports the length");
    PyBuffer_Release(&simple);

    Py_buffer nd{};
    if (PyObject_GetBuffer(v_small, &nd, PyBUF_ND | PyBUF_FORMAT) < 0) {
        PyErr_Print();
        return 1;
    }
    check(nd.shape != nullptr, "PyBUF_ND gets shape");
    check(nd.format != nullptr, "PyBUF_FORMAT gets format");
    check(nd.strides == nullptr, "but not strides, which it did not ask for");
    PyBuffer_Release(&nd);

    // memoryview is the path that works today, and has to keep working.
    PyObject* mv = PyMemoryView_FromObject(v_big);
    if (!mv) {
        PyErr_Print();
        return 1;
    }
    check(PyObject_Length(mv) == 1000, "a memoryview of the second view has 1000 items");
    Py_DECREF(mv);

    Py_DECREF(v_small);
    Py_DECREF(v_big);

    std::printf("\n%s\n", failures == 0 ? "PASS: buffer views describe themselves"
                                        : "FAIL");
    return failures == 0 ? 0 : 1;
}
