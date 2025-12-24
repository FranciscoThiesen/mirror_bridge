#pragma once

// ============================================================================
// Mirror Bridge - Zero-Copy Buffer Protocol Support
// ============================================================================
//
// Provides Python buffer protocol implementation for efficient data sharing
// between C++ and Python without copying.
//
// Features:
// - BufferView<T> wrapper for exposing C++ containers to Python
// - Automatic buffer protocol for vector<uint8_t>, vector<float>, etc.
// - memoryview and numpy array compatibility
// - Reference counting to prevent premature deallocation
//
// Usage:
//   // Expose a vector as a zero-copy buffer
//   std::vector<float> data = get_data();
//   return mirror_bridge::buffer::create_view(data);  // Returns memoryview
//
//   // In Python:
//   view = obj.get_data()  # memoryview, shares memory with C++
//   arr = np.asarray(view)  # numpy array without copying
//
// ============================================================================

#include <Python.h>
#include <vector>
#include <array>
#include <span>
#include <type_traits>
#include <cstring>
#include <cstdint>

namespace mirror_bridge {
namespace buffer {

// ============================================================================
// Buffer Format Strings (PEP 3118)
// ============================================================================

template<typename T>
constexpr const char* format_string() {
    using CleanT = std::remove_cvref_t<T>;

    if constexpr (std::is_same_v<CleanT, char> || std::is_same_v<CleanT, int8_t>) {
        return "b";  // signed char
    } else if constexpr (std::is_same_v<CleanT, unsigned char> ||
                         std::is_same_v<CleanT, uint8_t> ||
                         std::is_same_v<CleanT, std::byte>) {
        return "B";  // unsigned char
    } else if constexpr (std::is_same_v<CleanT, short>) {
        return "h";  // short
    } else if constexpr (std::is_same_v<CleanT, unsigned short>) {
        return "H";  // unsigned short
    } else if constexpr (std::is_same_v<CleanT, int>) {
        return "i";  // int
    } else if constexpr (std::is_same_v<CleanT, unsigned int>) {
        return "I";  // unsigned int
    } else if constexpr (std::is_same_v<CleanT, long>) {
        return "l";  // long
    } else if constexpr (std::is_same_v<CleanT, unsigned long>) {
        return "L";  // unsigned long
    } else if constexpr (std::is_same_v<CleanT, long long>) {
        return "q";  // long long
    } else if constexpr (std::is_same_v<CleanT, unsigned long long>) {
        return "Q";  // unsigned long long
    } else if constexpr (std::is_same_v<CleanT, float>) {
        return "f";  // float
    } else if constexpr (std::is_same_v<CleanT, double>) {
        return "d";  // double
    } else if constexpr (std::is_same_v<CleanT, bool>) {
        return "?";  // bool
    } else {
        return nullptr;  // Unsupported type
    }
}

// ============================================================================
// Concept for Bufferable Types
// ============================================================================

template<typename T>
concept ContiguousContainer = requires(T& t) {
    { t.data() } -> std::convertible_to<const void*>;
    { t.size() } -> std::convertible_to<std::size_t>;
    typename T::value_type;
} && std::is_trivially_copyable_v<typename T::value_type>;

template<typename T>
concept BufferableElement = (format_string<T>() != nullptr);

template<typename T>
concept Bufferable = ContiguousContainer<T> && BufferableElement<typename T::value_type>;

// ============================================================================
// BufferView - Python Object Wrapper with Buffer Protocol
// ============================================================================

template<typename Container>
struct BufferViewObject {
    PyObject_HEAD
    Container* container;      // Pointer to the C++ container
    PyObject* owner;           // Reference to the owner object (prevents deallocation)
    bool owns_data;            // True if we own the container and should delete it
};

// Forward declarations
template<typename Container>
int buffer_getbuffer(PyObject* self, Py_buffer* view, int flags);

template<typename Container>
void buffer_releasebuffer(PyObject* self, Py_buffer* view);

template<typename Container>
void bufferview_dealloc(PyObject* self);

// Buffer protocol methods
template<typename Container>
PyBufferProcs buffer_procs = {
    .bf_getbuffer = buffer_getbuffer<Container>,
    .bf_releasebuffer = buffer_releasebuffer<Container>,
};

// Get or create the BufferView type for a specific container type
template<typename Container>
PyTypeObject* get_bufferview_type() {
    using ValueT = typename Container::value_type;

    static PyTypeObject type_object = {
        PyVarObject_HEAD_INIT(nullptr, 0)
        .tp_name = "mirror_bridge.BufferView",
        .tp_basicsize = sizeof(BufferViewObject<Container>),
        .tp_itemsize = 0,
        .tp_dealloc = bufferview_dealloc<Container>,
        .tp_as_buffer = &buffer_procs<Container>,
        .tp_flags = Py_TPFLAGS_DEFAULT,
        .tp_doc = "Zero-copy buffer view of C++ container",
    };

    static bool initialized = false;
    if (!initialized) {
        if (PyType_Ready(&type_object) < 0) {
            return nullptr;
        }
        initialized = true;
    }

    return &type_object;
}

// ============================================================================
// Buffer Protocol Implementation
// ============================================================================

template<typename Container>
int buffer_getbuffer(PyObject* self, Py_buffer* view, int flags) {
    auto* obj = reinterpret_cast<BufferViewObject<Container>*>(self);
    using ValueT = typename Container::value_type;

    if (!obj->container) {
        PyErr_SetString(PyExc_BufferError, "Buffer has been released");
        return -1;
    }

    // Check format string support
    constexpr const char* fmt = format_string<ValueT>();
    if (!fmt) {
        PyErr_SetString(PyExc_BufferError, "Unsupported element type for buffer protocol");
        return -1;
    }

    // Fill the buffer info
    view->obj = self;
    Py_INCREF(self);

    view->buf = const_cast<void*>(static_cast<const void*>(obj->container->data()));
    view->len = obj->container->size() * sizeof(ValueT);
    view->itemsize = sizeof(ValueT);
    view->readonly = 0;  // Allow writes
    view->ndim = 1;
    view->format = const_cast<char*>(fmt);

    // Allocate shape and strides
    static Py_ssize_t shape_val;
    static Py_ssize_t strides_val;
    shape_val = static_cast<Py_ssize_t>(obj->container->size());
    strides_val = sizeof(ValueT);

    view->shape = &shape_val;
    view->strides = &strides_val;
    view->suboffsets = nullptr;
    view->internal = nullptr;

    return 0;
}

template<typename Container>
void buffer_releasebuffer(PyObject* self, Py_buffer* view) {
    // Nothing to do - the view's lifetime is managed by Python
    (void)self;
    (void)view;
}

template<typename Container>
void bufferview_dealloc(PyObject* self) {
    auto* obj = reinterpret_cast<BufferViewObject<Container>*>(self);

    if (obj->owns_data && obj->container) {
        delete obj->container;
    }

    // Release reference to owner
    Py_XDECREF(obj->owner);

    Py_TYPE(self)->tp_free(self);
}

// ============================================================================
// Factory Functions
// ============================================================================

// Create a BufferView from a container, transferring ownership
// The container will be deleted when the BufferView is garbage collected
template<Bufferable Container>
PyObject* create_view_owned(Container&& container) {
    PyTypeObject* type = get_bufferview_type<Container>();
    if (!type) {
        return nullptr;
    }

    auto* obj = PyObject_New(BufferViewObject<Container>, type);
    if (!obj) {
        return nullptr;
    }

    // Move the container to a heap allocation
    obj->container = new Container(std::move(container));
    obj->owner = nullptr;
    obj->owns_data = true;

    return reinterpret_cast<PyObject*>(obj);
}

// Create a BufferView from a container reference with an owner
// The owner object keeps the container alive
template<Bufferable Container>
PyObject* create_view_borrowed(Container& container, PyObject* owner) {
    PyTypeObject* type = get_bufferview_type<Container>();
    if (!type) {
        return nullptr;
    }

    auto* obj = PyObject_New(BufferViewObject<Container>, type);
    if (!obj) {
        return nullptr;
    }

    obj->container = &container;
    obj->owner = owner;
    obj->owns_data = false;

    // Keep owner alive
    Py_XINCREF(owner);

    return reinterpret_cast<PyObject*>(obj);
}

// ============================================================================
// Convenience: Create memoryview directly
// ============================================================================

// Create a memoryview from a container (copies data to Python-managed buffer)
// This is the safest option when container lifetime is uncertain
template<ContiguousContainer Container>
PyObject* to_memoryview(const Container& container) {
    using ValueT = typename Container::value_type;

    // Create a bytes object with the data
    const Py_ssize_t size = container.size() * sizeof(ValueT);
    PyObject* bytes = PyBytes_FromStringAndSize(
        reinterpret_cast<const char*>(container.data()),
        size
    );
    if (!bytes) {
        return nullptr;
    }

    // Create a memoryview from the bytes
    PyObject* view = PyMemoryView_FromObject(bytes);
    Py_DECREF(bytes);

    return view;
}

// Create a zero-copy memoryview from a container with explicit ownership
template<Bufferable Container>
PyObject* to_memoryview_zero_copy(Container&& container) {
    PyObject* buffer_view = create_view_owned(std::move(container));
    if (!buffer_view) {
        return nullptr;
    }

    // Create memoryview from our BufferView
    PyObject* view = PyMemoryView_FromObject(buffer_view);
    Py_DECREF(buffer_view);  // memoryview holds reference

    return view;
}

// ============================================================================
// Integration with to_python
// ============================================================================

// Concept for containers that should use zero-copy by default
template<typename T>
concept PreferZeroCopy = Bufferable<T> &&
    (sizeof(typename T::value_type) == 1 ||  // Byte containers
     std::is_floating_point_v<typename T::value_type>);  // Float arrays

} // namespace buffer
} // namespace mirror_bridge
