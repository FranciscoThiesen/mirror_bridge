#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Mirror Bridge - Automatic Python Bindings for C++ Code via C++26 Reflection
// ═══════════════════════════════════════════════════════════════════════════
//
// Generates Python bindings that expose C++ classes to Python.
//
// A showcase of modern C++26 techniques for zero-overhead metaprogramming:
//
// • C++26 Reflection (P2996): Compile-time class introspection
//   - std::meta::members_of(^^T) - discover class structure
//   - [:expr:] splicers - inject reflected code back into source
//   - consteval - guaranteed compile-time evaluation
//
// • C++20 Concepts: Type constraints without SFINAE noise
//   - Arithmetic, Container, SmartPointer concept definitions
//   - Enables elegant function overloading based on type properties
//
// • Modern Template Metaprogramming:
//   - Variadic templates with parameter packs (Args...)
//   - Fold expressions (expr, ...) for pack expansion
//   - std::index_sequence for compile-time iteration
//   - Template recursion instead of runtime loops in consteval contexts
//
// • SFINAE via std::enable_if_t: When concepts aren't enough
//   - Forward declarations for complex type hierarchies
//   - Ensures proper template instantiation for nested types
//
// Zero Runtime Overhead:
//   All binding code is generated at compile-time. No vtables, no RTTI lookups,
//   no runtime type checking. Direct member access via compile-time offsets.
//
// Requirements:
//   - Bloomberg clang-p2996 with C++26 reflection support
//   - Compile flags: -std=c++2c -freflection -freflection-latest -stdlib=libc++
//   - Python 3.7+ development headers
//
// Usage:
//   #include "mirror_bridge.hpp"
//   MIRROR_BRIDGE_MODULE(my_module,
//       mirror_bridge::bind_class<MyClass>(m, "MyClass");
//   )
//
// ═══════════════════════════════════════════════════════════════════════════

#include <meta>
#include <Python.h>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <type_traits>
#include <concepts>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>  // For uint8_t, int32_t, etc.
#include <cstdio>   // For snprintf in simple repr functions

// Include core header for GlobalTypeRegistry
#include "core/mirror_bridge_core.hpp"

// ============================================================================
// Feature Detection - Check for P2996 Reflection Support
// ============================================================================
//
// Standard feature-test macro for C++26 reflection (P2996).
// Compilers implementing P2996 should define __cpp_reflection.
//
// Usage:
//   #if !defined(__cpp_reflection) || __cpp_reflection < 202306L
//   #error "This library requires C++26 reflection support (P2996)"
//   #endif
//
// Bloomberg clang-p2996 implementation notes:
//   - Uses experimental flags: -freflection -freflection-latest
//   - May not define __cpp_reflection (experimental implementation)
//   - Alternative check: Look for <meta> header availability
//
#ifndef __cpp_reflection
  // Experimental compilers may not set this macro yet
  // If <meta> compiled successfully, we likely have reflection support
  #warning "Compiler does not define __cpp_reflection feature-test macro. " \
           "Reflection support is experimental and may be incomplete."
#elif __cpp_reflection < 202306L
  #error "This library requires C++26 reflection (P2996) from 2023-06 or later"
#endif

// Library version and capabilities
#define MIRROR_BRIDGE_VERSION_MAJOR 0
#define MIRROR_BRIDGE_VERSION_MINOR 1
#define MIRROR_BRIDGE_VERSION_PATCH 0

// Supported reflection features (for client code to check capabilities)
#define MIRROR_BRIDGE_HAS_REFLECTION 1
#define MIRROR_BRIDGE_HAS_ENUMERATORS_OF 1  // Uses std::meta::enumerators_of for enum validation
#define MIRROR_BRIDGE_HAS_TYPE_SIGNATURES 1  // Generates full type signatures with methods

namespace mirror_bridge {

// ============================================================================
// Type Traits and Concepts (inspired by simdjson's approach)
// ============================================================================

// Concept to identify arithmetic types (int, float, double, etc.)
template<typename T>
concept Arithmetic = std::is_arithmetic_v<std::remove_cvref_t<T>>;

// Concept to identify string-like types
// SAFETY NOTE: Only std::string is safe for data members (owns its data).
// std::string_view, const char*, char* are UNSAFE as data members because they
// can dangle when the Python string is freed. We disallow them in from_python
// for Bindable types to prevent lifetime issues.
template<typename T>
concept StringLike =
    std::is_same_v<std::remove_cvref_t<T>, std::string> ||
    std::is_same_v<std::remove_cvref_t<T>, std::string_view> ||
    std::is_same_v<std::remove_cvref_t<T>, const char*> ||
    std::is_same_v<std::remove_cvref_t<T>, char*>;

// Concept to identify smart pointers (unique_ptr, shared_ptr)
template<typename T>
concept SmartPointer = requires {
    typename std::remove_cvref_t<T>::element_type;
} && (std::is_same_v<std::remove_cvref_t<T>, std::unique_ptr<typename std::remove_cvref_t<T>::element_type>> ||
      std::is_same_v<std::remove_cvref_t<T>, std::shared_ptr<typename std::remove_cvref_t<T>::element_type>>);

// Helper trait to detect std::function
template<typename T>
struct is_std_function : std::false_type {};

template<typename R, typename... Args>
struct is_std_function<std::function<R(Args...)>> : std::true_type {};

// Concept to identify std::function types (for callback support)
template<typename T>
concept StdFunction = is_std_function<std::remove_cvref_t<T>>::value;

// Concept to identify C++ standard containers (vector, array, list, etc.)
// Optimized: uses begin/end check instead of ranges (avoids heavy <ranges> include)
// Requires .size() because our to_python implementation uses it.
// Uses declval to avoid requiring default-constructibility.
template<typename T>
concept Container =
    requires {
        { std::declval<T>().begin() } -> std::input_or_output_iterator;
        { std::declval<T>().end() } -> std::input_or_output_iterator;
        { std::declval<T>().size() } -> std::convertible_to<std::size_t>;  // Required by to_python
        typename std::remove_cvref_t<T>::value_type;
    } &&
    !StringLike<T> &&
    !SmartPointer<T>;

// Concept for enum types
template<typename T>
concept EnumType = std::is_enum_v<std::remove_cvref_t<T>>;

// Concept for types that can be bound to Python (classes with reflectable members)
template<typename T>
concept Bindable = std::is_class_v<std::remove_cvref_t<T>> && requires {
    { std::meta::nonstatic_data_members_of(^^std::remove_cvref_t<T>, std::meta::access_context::current()) };
};

// Concept for nested bindable classes (used as member types in other classes)
template<typename T>
concept NestedBindable = Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>;

// Forward declarations for nested bindable type conversion
// These match the SFINAE templates defined later in the file
template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    PyObject*
>
to_python(const T& obj);

template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    bool
>
from_python(PyObject* obj, T& out);

// ============================================================================
// Python Type Conversion Utilities
// ============================================================================

// Convert C++ types to Python objects
template<Arithmetic T>
PyObject* to_python(const T& value) {
    if constexpr (std::is_floating_point_v<T>) {
        return PyFloat_FromDouble(static_cast<double>(value));
    } else if constexpr (std::is_signed_v<T>) {
        return PyLong_FromLong(static_cast<long>(value));
    } else {
        return PyLong_FromUnsignedLong(static_cast<unsigned long>(value));
    }
}

// Enum to Python conversion - convert to int
template<EnumType T>
PyObject* to_python(const T& value) {
    return PyLong_FromLong(static_cast<long>(value));
}

// Enum from Python conversion - convert from int
template<EnumType T>
inline bool from_python(PyObject* obj, T& out) {
    if (!PyLong_Check(obj)) return false;
    long value = PyLong_AsLong(obj);
    if (value == -1 && PyErr_Occurred()) return false;
    out = static_cast<T>(value);
    return true;
}

template<StringLike T>
PyObject* to_python(const T& value) {
    using BaseType = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<BaseType, std::string>) {
        return PyUnicode_FromStringAndSize(value.data(), value.size());
    } else if constexpr (std::is_same_v<BaseType, std::string_view>) {
        return PyUnicode_FromStringAndSize(value.data(), value.size());
    } else {
        return PyUnicode_FromString(value);
    }
}

// Smart pointer conversion - converts pointee, not pointer itself
//
// CURRENT LIMITATION: For Bindable element types, this returns dicts (not wrapper objects).
// This is because to_python(*ptr) resolves to the Bindable overload which returns dicts.
//
// Example:
//   struct Foo { int x; };
//   std::shared_ptr<Foo> ptr;
//   to_python(ptr) → returns dict {'x': 0}, not a Foo wrapper object
//
// IDEAL BEHAVIOR: Should return a PyWrapper<Foo>* if Foo has been bound via bind_class.
//
// PROPOSED FIX:
//   1. Add Registry::is_bound<T>() to check if type has been bound
//   2. If bound: Create PyWrapper<ElementType>, set owns=false (shared ownership with smart ptr)
//   3. If not bound: Fall back to dict conversion (current behavior)
//
// This requires coordination between smart pointer lifetime and Python ref counting:
//   - shared_ptr: Can safely share ownership (increment ref count on smart ptr)
//   - unique_ptr: More complex - need to transfer ownership or use raw pointer with custom deleter
//
// For now, dict conversion is consistent with nested Bindable semantics (see documentation above).
template<SmartPointer T>
inline PyObject* to_python(const T& ptr) {
    if (!ptr) {
        Py_RETURN_NONE;
    }
    using ElementType = typename std::remove_cvref_t<T>::element_type;
    // Let overload resolution choose - non-template overloads have higher priority
    // TODO: Check if ElementType is bound and create wrapper object instead of dict
    return to_python(*ptr);
}

// Convert Python objects to C++ types
template<Arithmetic T>
bool from_python(PyObject* obj, T& out) {
    if constexpr (std::is_floating_point_v<T>) {
        if (!PyFloat_Check(obj) && !PyLong_Check(obj)) return false;
        out = static_cast<T>(PyFloat_AsDouble(obj));
        return true;
    } else if constexpr (std::is_signed_v<T>) {
        if (!PyLong_Check(obj)) return false;
        out = static_cast<T>(PyLong_AsLong(obj));
        return true;
    } else {
        if (!PyLong_Check(obj)) return false;
        out = static_cast<T>(PyLong_AsUnsignedLong(obj));
        return true;
    }
}

template<StringLike T>
inline bool from_python(PyObject* obj, T& out) {
    using BaseType = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<BaseType, std::string>) {
        if (!PyUnicode_Check(obj)) return false;
        Py_ssize_t size;
        const char* data = PyUnicode_AsUTF8AndSize(obj, &size);
        if (!data) return false;
        out = std::string(data, size);
        return true;
    } else if constexpr (std::is_same_v<BaseType, std::string_view>) {
        // Note: string_view requires the underlying string to remain valid
        // For Python->C++ conversion, we typically need to store strings, not views
        // This is mainly useful for temporary conversions
        if (!PyUnicode_Check(obj)) return false;
        Py_ssize_t size;
        const char* data = PyUnicode_AsUTF8AndSize(obj, &size);
        if (!data) return false;
        out = std::string_view(data, size);
        return true;
    } else {
        // const char* and char* - these are less safe for conversion from Python
        // since Python strings are managed objects, but we support them for compatibility
        if (!PyUnicode_Check(obj)) return false;
        const char* data = PyUnicode_AsUTF8(obj);
        if (!data) return false;
        out = data;
        return true;
    }
}

// Smart pointer conversion from Python
// Creates a new instance from the Python value
template<SmartPointer T>
inline bool from_python(PyObject* obj, T& out) {
    using ElementType = typename std::remove_cvref_t<T>::element_type;

    if (obj == Py_None) {
        out.reset();
        return true;
    }

    ElementType value;
    // Let overload resolution choose - non-template overloads have higher priority
    if (!from_python(obj, value)) {
        return false;
    }

    if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::unique_ptr<ElementType>>) {
        out = std::make_unique<ElementType>(std::move(value));
    } else {
        out = std::make_shared<ElementType>(std::move(value));
    }
    return true;
}

// ============================================================================
// std::function Callback Support
// ============================================================================
//
// Enables passing Python callables to C++ functions that accept std::function.
//
// Example C++ code:
//   void set_callback(std::function<void(int)> cb);
//   void process(std::function<int(double, double)> compute);
//
// Python usage:
//   obj.set_callback(lambda x: print(f"Got: {x}"))
//   obj.process(lambda a, b: int(a + b))
//
// The wrapper stores a reference to the Python callable and handles:
// - GIL acquisition for thread safety
// - Argument conversion from C++ to Python
// - Return value conversion from Python to C++
// - Exception propagation from Python to C++

// Helper struct to wrap a Python callable as a C++ callable
// Uses shared_ptr to PyObject for proper reference counting
template<typename R, typename... Args>
struct PythonCallableWrapper {
    // Shared ownership of the Python callable object
    std::shared_ptr<PyObject> py_callable;

    PythonCallableWrapper(PyObject* callable)
        : py_callable(callable, [](PyObject* p) {
            // Release Python reference when the wrapper is destroyed
            // Must acquire GIL since this may be called from any thread
            PyGILState_STATE gstate = PyGILState_Ensure();
            Py_XDECREF(p);
            PyGILState_Release(gstate);
        })
    {
        Py_INCREF(callable);  // Take ownership
    }

    R operator()(Args... args) const {
        // Acquire GIL - critical for thread safety
        PyGILState_STATE gstate = PyGILState_Ensure();

        // Build argument tuple
        PyObject* py_args = PyTuple_New(sizeof...(Args));
        if (!py_args) {
            PyGILState_Release(gstate);
            throw std::runtime_error("Failed to create argument tuple");
        }

        // Convert each C++ argument to Python
        std::size_t idx = 0;
        bool conversion_ok = true;
        (([&] {
            if (!conversion_ok) return;
            PyObject* py_arg = to_python(args);
            if (!py_arg) {
                conversion_ok = false;
            } else {
                PyTuple_SET_ITEM(py_args, idx, py_arg);  // Steals reference
            }
            idx++;
        })(), ...);

        if (!conversion_ok) {
            Py_DECREF(py_args);
            PyGILState_Release(gstate);
            throw std::runtime_error("Failed to convert callback arguments to Python");
        }

        // Call the Python callable
        PyObject* result = PyObject_Call(py_callable.get(), py_args, nullptr);
        Py_DECREF(py_args);

        if (!result) {
            // Python exception occurred - convert to C++ exception
            PyObject *type, *value, *traceback;
            PyErr_Fetch(&type, &value, &traceback);

            std::string error_msg = "Python callback raised an exception";
            if (value) {
                PyObject* str_obj = PyObject_Str(value);
                if (str_obj) {
                    const char* str = PyUnicode_AsUTF8(str_obj);
                    if (str) error_msg = str;
                    Py_DECREF(str_obj);
                }
            }

            Py_XDECREF(type);
            Py_XDECREF(value);
            Py_XDECREF(traceback);
            PyGILState_Release(gstate);
            throw std::runtime_error(error_msg);
        }

        // Handle return value
        if constexpr (std::is_void_v<R>) {
            Py_DECREF(result);
            PyGILState_Release(gstate);
        } else {
            R cpp_result;
            bool ok = from_python(result, cpp_result);
            Py_DECREF(result);
            PyGILState_Release(gstate);

            if (!ok) {
                throw std::runtime_error("Failed to convert Python callback return value to C++");
            }
            return cpp_result;
        }
    }
};

// Convert Python callable to std::function
// This is the key function for callback support
template<typename R, typename... Args>
inline bool from_python(PyObject* obj, std::function<R(Args...)>& out) {
    if (!obj || obj == Py_None) {
        out = nullptr;
        return true;
    }

    if (!PyCallable_Check(obj)) {
        PyErr_SetString(PyExc_TypeError, "Expected a callable object");
        return false;
    }

    // Wrap the Python callable in a C++ functor
    out = PythonCallableWrapper<R, Args...>(obj);
    return true;
}

// Convert std::function to Python (less common, but useful for completeness)
// Returns None if the function is empty, otherwise returns a description
template<typename R, typename... Args>
inline PyObject* to_python(const std::function<R(Args...)>& func) {
    if (!func) {
        Py_RETURN_NONE;
    }
    // We can't easily convert an arbitrary std::function back to a Python callable
    // Return a string description instead
    return PyUnicode_FromString("<C++ std::function>");
}

// ============================================================================
// Byte Vector Specialization
// ============================================================================
//
// Convert vector<unsigned char> to Python bytes (not list!) for ~50x speedup.
// Note: uint8_t may or may not be the same type as unsigned char depending on
// platform. We use a single definition with unsigned char since that's the
// canonical byte type, and static_assert that uint8_t is compatible.
static_assert(sizeof(uint8_t) == sizeof(unsigned char),
              "uint8_t and unsigned char must be same size for byte conversion");

inline PyObject* to_python(const std::vector<unsigned char>& data) {
    return PyBytes_FromStringAndSize(reinterpret_cast<const char*>(data.data()), data.size());
}

inline bool from_python(PyObject* obj, std::vector<unsigned char>& out) {
    if (PyBytes_Check(obj)) {
        char* data;
        Py_ssize_t size;
        if (PyBytes_AsStringAndSize(obj, &data, &size) < 0) {
            return false;
        }
        out.assign(reinterpret_cast<unsigned char*>(data),
                   reinterpret_cast<unsigned char*>(data) + size);
        return true;
    }
    // Also accept lists for compatibility
    if (PyList_Check(obj)) {
        Py_ssize_t size = PyList_Size(obj);
        out.clear();
        out.reserve(size);
        for (Py_ssize_t i = 0; i < size; ++i) {
            PyObject* item = PyList_GetItem(obj, i);
            if (!PyLong_Check(item)) return false;
            long val = PyLong_AsLong(item);
            if (val < 0 || val > 255) return false;
            out.push_back(static_cast<unsigned char>(val));
        }
        return true;
    }
    return false;
}

// ============================================================================
// std::array Support - Fixed-size Arrays
// ============================================================================

// Trait to detect std::array
template<typename T>
struct is_std_array : std::false_type {};

template<typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template<typename T>
inline constexpr bool is_std_array_v = is_std_array<std::remove_cvref_t<T>>::value;

// Convert std::array to Python list
template<typename T, std::size_t N>
PyObject* to_python(const std::array<T, N>& arr) {
    PyObject* list = PyList_New(N);
    if (!list) return nullptr;

    for (std::size_t i = 0; i < N; ++i) {
        PyObject* py_item = to_python(arr[i]);
        if (!py_item) {
            Py_DECREF(list);
            return nullptr;
        }
        PyList_SET_ITEM(list, i, py_item);
    }
    return list;
}

// Convert Python list to std::array
template<typename T, std::size_t N>
bool from_python(PyObject* obj, std::array<T, N>& arr) {
    if (!PyList_Check(obj)) return false;

    Py_ssize_t size = PyList_Size(obj);
    if (static_cast<std::size_t>(size) != N) {
        PyErr_Format(PyExc_ValueError,
                     "Expected list of size %zu, got %zd", N, size);
        return false;
    }

    for (std::size_t i = 0; i < N; ++i) {
        PyObject* py_item = PyList_GetItem(obj, i);
        if (!from_python(py_item, arr[i])) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Buffer Protocol Support - Zero-Copy NumPy Integration
// ============================================================================
//
// The Python buffer protocol enables zero-copy access to C++ data from NumPy.
// Instead of copying data between C++ and Python, NumPy arrays can directly
// view the memory owned by C++ containers.
//
// Usage in Python:
//   import numpy as np
//   img = imgproc.Image(1920, 1080)
//   # Zero-copy view of image data
//   arr = np.asarray(img.data_view())
//   # Modify directly in-place
//   arr[0:100, 0:100] = 255
//
// SAFETY: The C++ object MUST outlive the NumPy array. Use with caution.
//
// This is implemented as a wrapper class that exposes buffer protocol,
// rather than making every container support it (which would be invasive).

// BufferView exposes a contiguous memory region via Python buffer protocol
template<typename T>
struct BufferView {
    PyObject_HEAD
    T* data;           // Pointer to start of buffer
    Py_ssize_t size;   // Number of elements
    PyObject* owner;   // Keep owner alive (prevents dangling pointer)
};

// Type trait to get numpy-compatible format string
template<typename T>
consteval const char* get_buffer_format() {
    if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, unsigned char>) {
        return "B";  // unsigned byte
    } else if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, char>) {
        return "b";  // signed byte
    } else if constexpr (std::is_same_v<T, int16_t> || std::is_same_v<T, short>) {
        return "h";  // short
    } else if constexpr (std::is_same_v<T, uint16_t> || std::is_same_v<T, unsigned short>) {
        return "H";  // unsigned short
    } else if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, int>) {
        return "i";  // int
    } else if constexpr (std::is_same_v<T, uint32_t> || std::is_same_v<T, unsigned int>) {
        return "I";  // unsigned int
    } else if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, long long>) {
        return "q";  // long long
    } else if constexpr (std::is_same_v<T, uint64_t> || std::is_same_v<T, unsigned long long>) {
        return "Q";  // unsigned long long
    } else if constexpr (std::is_same_v<T, float>) {
        return "f";  // float
    } else if constexpr (std::is_same_v<T, double>) {
        return "d";  // double
    } else {
        return "B";  // Default to bytes
    }
}

// Buffer protocol: getbuffer implementation
template<typename T>
int buffer_getbuffer(PyObject* self, Py_buffer* view, int flags) {
    auto* buf = reinterpret_cast<BufferView<T>*>(self);

    view->obj = self;
    Py_INCREF(self);
    view->buf = buf->data;
    view->len = buf->size * sizeof(T);
    view->itemsize = sizeof(T);
    view->readonly = 0;  // Allow writes
    view->ndim = 1;
    view->format = const_cast<char*>(get_buffer_format<T>());
    view->shape = &buf->size;
    view->strides = &view->itemsize;
    view->suboffsets = nullptr;
    view->internal = nullptr;

    return 0;
}

// Buffer protocol: releasebuffer (no-op since we don't allocate)
template<typename T>
void buffer_releasebuffer(PyObject* self, Py_buffer* view) {
    // Nothing to release - memory is owned by C++ object
}

// Dealloc for BufferView
template<typename T>
void buffer_dealloc(PyObject* self) {
    auto* buf = reinterpret_cast<BufferView<T>*>(self);
    Py_XDECREF(buf->owner);
    Py_TYPE(self)->tp_free(self);
}

// Create a BufferView PyTypeObject for a given element type
template<typename T>
PyTypeObject* get_buffer_view_type() {
    static PyBufferProcs buffer_procs = {
        .bf_getbuffer = buffer_getbuffer<T>,
        .bf_releasebuffer = buffer_releasebuffer<T>,
    };

    static PyTypeObject type_object = {
        PyVarObject_HEAD_INIT(nullptr, 0)
        .tp_name = "BufferView",
        .tp_basicsize = sizeof(BufferView<T>),
        .tp_itemsize = 0,
        .tp_dealloc = buffer_dealloc<T>,
        .tp_as_buffer = &buffer_procs,
        .tp_flags = Py_TPFLAGS_DEFAULT,
        .tp_doc = "Zero-copy buffer view for C++ containers",
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

// Create a BufferView from a vector, keeping owner alive
template<typename T>
PyObject* create_buffer_view(std::vector<T>& vec, PyObject* owner) {
    PyTypeObject* type = get_buffer_view_type<T>();
    if (!type) return nullptr;

    auto* buf = reinterpret_cast<BufferView<T>*>(type->tp_alloc(type, 0));
    if (!buf) return nullptr;

    buf->data = vec.data();
    buf->size = static_cast<Py_ssize_t>(vec.size());
    buf->owner = owner;
    Py_XINCREF(owner);

    return reinterpret_cast<PyObject*>(buf);
}

// ============================================================================
// Batch Processing API - Hot Loop Optimization
// ============================================================================
//
// The batch processing API enables keeping iteration entirely in C++,
// avoiding the overhead of crossing the Python/C++ boundary N times.
//
// Problem: Traditional Python->C++ calls have ~100-500ns overhead per call.
// For 1M elements, that's 100-500ms of pure overhead!
//
// Solution: Process entire arrays/vectors in a single C++ call.
//
// Usage pattern (in C++ class definition):
//
//   // Old way (called N times from Python):
//   double process(double x) { return std::sqrt(x); }
//
//   // New batch API (called once, processes N elements):
//   std::vector<double> process_batch(const std::vector<double>& input) {
//       std::vector<double> result;
//       result.reserve(input.size());
//       for (auto x : input) {
//           result.push_back(std::sqrt(x));
//       }
//       return result;
//   }
//
// From Python:
//   # Old: ~500ms for 1M calls
//   results = [sim.process(x) for x in data]
//
//   # New: ~5ms for 1M elements (100x faster!)
//   results = sim.process_batch(data)
//
// The binding system automatically handles vector<T> conversion, so batch
// methods work seamlessly with Python lists and NumPy arrays.

// Helper to transform a vector element-wise using a functor
// Returns a new vector - used for methods that return transformed data
template<typename T, typename F>
std::vector<decltype(std::declval<F>()(std::declval<T>()))>
transform_batch(const std::vector<T>& input, F&& func) {
    using ResultT = decltype(func(input[0]));
    std::vector<ResultT> result;
    result.reserve(input.size());
    for (const auto& elem : input) {
        result.push_back(func(elem));
    }
    return result;
}

// In-place batch transform - modifies the input vector
template<typename T, typename F>
void transform_batch_inplace(std::vector<T>& data, F&& func) {
    for (auto& elem : data) {
        elem = func(elem);
    }
}

// Parallel batch transform using std::transform with execution policy
// Requires C++17 parallel algorithms support
// NOTE: Disabled for now due to experimental compiler compatibility issues
// Uncomment when using a standard C++17/20 compiler with TBB support
//
// #if __has_include(<execution>) && defined(__cpp_lib_parallel_algorithm)
// #include <execution>
//
// template<typename T, typename F>
// std::vector<decltype(std::declval<F>()(std::declval<T>()))>
// transform_batch_parallel(const std::vector<T>& input, F&& func) {
//     using ResultT = decltype(func(input[0]));
//     std::vector<ResultT> result(input.size());
//     std::transform(std::execution::par_unseq, input.begin(), input.end(),
//                    result.begin(), std::forward<F>(func));
//     return result;
// }
//
// template<typename T, typename F>
// void transform_batch_parallel_inplace(std::vector<T>& data, F&& func) {
//     std::for_each(std::execution::par_unseq, data.begin(), data.end(),
//                   [&](T& elem) { elem = func(elem); });
// }
// #endif

// Reduce operation - compute aggregate value from vector
template<typename T, typename F, typename Init>
Init reduce_batch(const std::vector<T>& input, Init init, F&& func) {
    for (const auto& elem : input) {
        init = func(init, elem);
    }
    return init;
}

// Filter operation - return elements matching predicate
template<typename T, typename Pred>
std::vector<T> filter_batch(const std::vector<T>& input, Pred&& pred) {
    std::vector<T> result;
    result.reserve(input.size() / 2);  // Heuristic
    for (const auto& elem : input) {
        if (pred(elem)) {
            result.push_back(elem);
        }
    }
    return result;
}

// ============================================================================
// NumPy-Compatible Array View
// ============================================================================
//
// For methods that return array data, this helper creates a view that numpy
// can consume directly without copying.

// Helper to wrap vector data for numpy access
// The vector MUST outlive the returned numpy array!
template<typename T>
PyObject* vector_to_numpy_view(std::vector<T>& vec, PyObject* owner) {
    return create_buffer_view(vec, owner);
}

// Convert C++ containers to Python lists
// Recursively converts each element using to_python
template<Container T>
PyObject* to_python(const T& container) {
    PyObject* list = PyList_New(container.size());
    if (!list) return nullptr;

    Py_ssize_t index = 0;
    for (const auto& item : container) {
        PyObject* py_item = to_python(item);
        if (!py_item) {
            Py_DECREF(list);
            return nullptr;
        }
        PyList_SET_ITEM(list, index++, py_item);
    }
    return list;
}

// Forward declare PyWrapper for use in from_python (will be fully defined later)
template<typename T> struct PyWrapper;

// Convert Python wrapped objects to C++ types
// This handles cases like Sphere(Vec3(...), double, Vec3(...))
// where Vec3 is a bound C++ class
// IMPORTANT: Must be declared BEFORE Container from_python so it's available for element conversion
//
// NOTE: We use std::remove_cvref_t<T> to strip const/reference qualifiers.
// This is critical for methods like dot(const Vec3& other) where the parameter
// type is "const Vec3&" but the PyWrapper stores a plain Vec3*.
template<typename T>
    requires (std::is_class_v<std::remove_cvref_t<T>> &&
              !Arithmetic<T> && !StringLike<T> && !SmartPointer<T> && !Container<T>)
inline bool from_python(PyObject* obj, T& out) {
    using CleanT = std::remove_cvref_t<T>;

    if (!obj) {
        return false;
    }

    // Cast to PyWrapper with cleaned type (no const/ref qualifiers)
    auto* wrapper = reinterpret_cast<PyWrapper<CleanT>*>(obj);

    // Minimal validation: just check if the pointer is non-null
    if (!wrapper->cpp_object) {
        return false;
    }

    // Copy the C++ object
    out = *wrapper->cpp_object;
    return true;
}

// Convert Python lists to C++ containers
// Supports any container with push_back (vector, list, deque) or insert (set, etc.)
template<Container T>
bool from_python(PyObject* obj, T& container) {
    if (!PyList_Check(obj)) return false;

    Py_ssize_t size = PyList_Size(obj);
    using ValueType = typename std::remove_cvref_t<T>::value_type;

    // Clear the container if it supports clear(), otherwise skip
    if constexpr (requires { container.clear(); }) {
        container.clear();
    }
    if constexpr (requires { container.reserve(size); }) {
        container.reserve(size);
    }

    Py_ssize_t i = 0;
    for (; i < size && i < static_cast<Py_ssize_t>(container.size()); ++i) {
        PyObject* py_item = PyList_GetItem(obj, i);  // Borrowed reference
        ValueType cpp_item;
        if (!from_python(py_item, cpp_item)) {
            return false;
        }

        // For indexed containers (array), use operator[]
        if constexpr (requires { container[i]; }) {
            container[i] = std::move(cpp_item);
        }
        // For sequential containers, use push_back
        else if constexpr (requires { container.push_back(cpp_item); }) {
            container.push_back(std::move(cpp_item));
        }
        // For associative containers, use insert
        else if constexpr (requires { container.insert(cpp_item); }) {
            container.insert(std::move(cpp_item));
        } else {
            static_assert(requires { container.push_back(cpp_item); },
                         "Container must support indexing, push_back, or insert");
        }
    }

    // For push_back containers, add remaining elements if list is larger
    if constexpr (requires { container.push_back(ValueType{}); }) {
        for (; i < size; ++i) {
            PyObject* py_item = PyList_GetItem(obj, i);
            ValueType cpp_item;
            if (!from_python(py_item, cpp_item)) {
                return false;
            }
            container.push_back(std::move(cpp_item));
        }
    }

    return true;
}

// ============================================================================
// Class Metadata and Registry
// ============================================================================

// Stores metadata about a registered class for hash-based change detection
struct ClassMetadata {
    std::string name;
    std::string type_signature;  // Reflection-derived signature for change detection
    size_t hash;                 // Hash of the type signature
    PyTypeObject* py_type;       // Python type object (nullptr before binding generation)

    // Compute hash from type signature
    void compute_hash() {
        std::hash<std::string> hasher;
        hash = hasher(type_signature);
    }

    // Check if this class needs recompilation by comparing signatures
    bool needs_recompilation(const std::string& new_signature) const {
        return type_signature != new_signature;
    }
};

// Global registry for all classes that need Python bindings
class Registry {
public:
    static Registry& instance() {
        static Registry reg;
        return reg;
    }

    // Register a class with its metadata
    void register_class(const std::string& name, const std::string& signature) {
        ClassMetadata metadata{name, signature, 0, nullptr};
        metadata.compute_hash();
        classes_[name] = metadata;
    }

    // Get metadata for a registered class
    const ClassMetadata* get_metadata(const std::string& name) const {
        auto it = classes_.find(name);
        return (it != classes_.end()) ? &it->second : nullptr;
    }

    // Get all registered classes
    const std::unordered_map<std::string, ClassMetadata>& get_all() const {
        return classes_;
    }

    // Update Python type object after binding generation
    void set_py_type(const std::string& name, PyTypeObject* type) {
        auto it = classes_.find(name);
        if (it != classes_.end()) {
            it->second.py_type = type;
        }
    }

    // Get Python type object by name
    PyTypeObject* get_py_type(const std::string& name) const {
        auto it = classes_.find(name);
        return (it != classes_.end()) ? it->second.py_type : nullptr;
    }

private:
    Registry() = default;
    std::unordered_map<std::string, ClassMetadata> classes_;
};

// Type-based registry for looking up PyTypeObject* by C++ type
// Uses a static variable per type T to store the registered PyTypeObject*
template<typename T>
struct TypeRegistry {
    static inline PyTypeObject* py_type = nullptr;
};

// ============================================================================
// Reflection-Based Binding Generator
// ============================================================================

// ============================================================================
// COMPILE-TIME OPTIMIZATION: Data Member and Function Caches
// ============================================================================
//
// Problem: Calling nonstatic_data_members_of() and members_of() repeatedly in
// helper functions causes O(N) scans to execute many times, leading to O(N³)
// compile-time complexity for large classes.
//
// Solution: Cache both data members and member functions once per type in
// constexpr structures. This reduces compile-time complexity from O(N³) to O(N²).

// Cache for data member information - instantiated once per type T
template<typename T>
struct DataMemberCache {
    // Count data members - computed once at compile time
    static consteval std::size_t compute_count() {
        return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()).size();
    }

    // Get the Nth data member - computed once per Index
    static consteval auto get_at_index(std::size_t Index) {
        return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current())[Index];
    }

    static constexpr std::size_t count = compute_count();
};

// Helper templates for data member reflection - use the cache
template<typename T>
consteval std::size_t get_data_member_count() {
    return DataMemberCache<T>::count;
}

template<typename T, std::size_t Index>
consteval auto get_data_member() {
    return DataMemberCache<T>::get_at_index(Index);
}

// Helper templates for data member type info - use cache for efficiency
template<typename T, std::size_t Index>
consteval const char* get_data_member_name() {
    return std::meta::identifier_of(get_data_member<T, Index>()).data();
}

template<typename T, std::size_t Index>
consteval const char* get_data_member_type() {
    return std::meta::display_string_of(
        std::meta::type_of(get_data_member<T, Index>())
    ).data();
}

// Helper aliases for nested bindable classes - use cache
template<typename T>
consteval std::size_t get_nested_member_count() {
    return get_data_member_count<T>();
}

template<typename T, std::size_t Index>
consteval auto get_nested_member() {
    return get_data_member<T, Index>();
}

template<typename T, std::size_t Index>
consteval const char* get_nested_member_name() {
    return std::meta::identifier_of(get_data_member<T, Index>()).data();
}

template<typename T, std::size_t Index>
using NestedMemberType = typename [:std::meta::type_of(get_data_member<T, Index>()):];

// Cache for member function information - instantiated once per type T
template<typename T>
struct MemberFunctionCache {
    // Helper to check if a member is a bindable method
    static consteval bool is_bindable_method(std::meta::info member) {
        return std::meta::is_function(member) &&
               !std::meta::is_static_member(member) &&
               !std::meta::is_constructor(member) &&
               !std::meta::is_special_member_function(member) &&
               !std::meta::is_operator_function(member);
    }

    // Count methods - computed once at compile time
    static consteval std::size_t compute_count() {
        auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
        std::size_t count = 0;
        for (auto member : all_members) {
            if (is_bindable_method(member)) {
                count++;
            }
        }
        return count;
    }

    // Get the Nth method - computed once per Index
    static consteval auto get_at_index(std::size_t Index) {
        auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
        std::size_t func_index = 0;
        for (auto member : all_members) {
            if (is_bindable_method(member)) {
                if (func_index == Index) {
                    return member;
                }
                func_index++;
            }
        }
        // Should never reach here if Index is valid
        return all_members[0];
    }

    static constexpr std::size_t count = compute_count();
};

// Cache for static member functions - instantiated once per type T
template<typename T>
struct StaticMemberFunctionCache {
    // Helper to check if a member is a bindable static method
    static consteval bool is_bindable_static_method(std::meta::info member) {
        return std::meta::is_function(member) &&
               std::meta::is_static_member(member) &&
               !std::meta::is_constructor(member) &&
               !std::meta::is_special_member_function(member) &&
               !std::meta::is_operator_function(member);
    }

    // Count static methods - computed once at compile time
    static consteval std::size_t compute_count() {
        auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
        std::size_t count = 0;
        for (auto member : all_members) {
            if (is_bindable_static_method(member)) {
                count++;
            }
        }
        return count;
    }

    // Get the Nth static method - computed once per Index
    static consteval auto get_at_index(std::size_t Index) {
        auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
        std::size_t func_index = 0;
        for (auto member : all_members) {
            if (is_bindable_static_method(member)) {
                if (func_index == Index) {
                    return member;
                }
                func_index++;
            }
        }
        // Should never reach here if Index < count
        return all_members[0];
    }

    static constexpr std::size_t count = compute_count();
};

// Helper templates for member function reflection
// Now use the cache instead of calling members_of repeatedly
template<typename T>
consteval std::size_t get_member_function_count() {
    return MemberFunctionCache<T>::count;
}

// Helper templates for static member function reflection
template<typename T>
consteval std::size_t get_static_member_function_count() {
    return StaticMemberFunctionCache<T>::count;
}

// Get the Nth member function by using cached lookup
template<typename T, std::size_t Index>
consteval auto get_member_function() {
    return MemberFunctionCache<T>::get_at_index(Index);
}

// Get the Nth static member function by using cached lookup
template<typename T, std::size_t Index>
consteval auto get_static_member_function() {
    return StaticMemberFunctionCache<T>::get_at_index(Index);
}

template<typename T, std::size_t Index>
consteval const char* get_member_function_name() {
    constexpr auto func = get_member_function<T, Index>();
    return std::meta::identifier_of(func).data();
}

template<typename T, std::size_t Index>
consteval const char* get_static_member_function_name() {
    constexpr auto func = get_static_member_function<T, Index>();
    return std::meta::identifier_of(func).data();
}

// Check if a method at given Index is overloaded (same name as another method)
template<typename T, std::size_t Index, std::size_t CheckIndex = 0>
consteval bool is_method_overloaded() {
    constexpr std::size_t func_count = get_member_function_count<T>();

    // Base case: checked all methods, no overload found
    if constexpr (CheckIndex >= func_count) {
        return false;
    }
    // Skip comparing a method with itself
    else if constexpr (CheckIndex == Index) {
        return is_method_overloaded<T, Index, CheckIndex + 1>();
    }
    // Compare names
    else {
        constexpr auto name1 = get_member_function_name<T, Index>();
        constexpr auto name2 = get_member_function_name<T, CheckIndex>();
        if (std::string_view(name1) == std::string_view(name2)) {
            return true;  // Found overload!
        }
        return is_method_overloaded<T, Index, CheckIndex + 1>();
    }
}

// Helper functions for method parameter reflection
template<typename T, std::size_t FuncIndex>
consteval std::size_t get_method_param_count() {
    constexpr auto member_func = get_member_function<T, FuncIndex>();
    return std::meta::parameters_of(member_func).size();
}

template<typename T, std::size_t FuncIndex, std::size_t ParamIndex>
consteval auto get_method_param_type() {
    constexpr auto member_func = get_member_function<T, FuncIndex>();
    auto params = std::meta::parameters_of(member_func);
    return std::meta::type_of(params[ParamIndex]);
}

// Static method parameter type - similar to instance methods
template<typename T, std::size_t FuncIndex, std::size_t ParamIndex>
consteval auto get_static_method_param_type() {
    constexpr auto static_func = get_static_member_function<T, FuncIndex>();
    auto params = std::meta::parameters_of(static_func);
    return std::meta::type_of(params[ParamIndex]);
}

template<typename T, std::size_t FuncIndex>
consteval auto get_method_return_type() {
    constexpr auto member_func = get_member_function<T, FuncIndex>();
    return std::meta::return_type_of(member_func);
}

// Static method return type
template<typename T, std::size_t FuncIndex>
consteval auto get_static_method_return_type() {
    constexpr auto static_func = get_static_member_function<T, FuncIndex>();
    return std::meta::return_type_of(static_func);
}

// Generate a type signature for a given class using reflection
template<typename T>
std::string generate_type_signature(const char* file_hash = nullptr) {
    // Use display_string_of instead of identifier_of to support template specializations
    // identifier_of fails for types like Container<int> since they're not simple identifiers
    std::string sig{std::meta::display_string_of(^^T)};
    sig += "{";

    // Data members section - use cached count
    constexpr std::size_t data_member_count = get_data_member_count<T>();

    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            if (Is > 0) sig += ",";
            sig += get_data_member_type<T, Is>();
            sig += " ";
            sig += get_data_member_name<T, Is>();
        }(), ...);
    }(std::make_index_sequence<data_member_count>{});

    // Member functions section - include method signatures for change detection
    sig += "|methods:";

    constexpr std::size_t method_count = get_member_function_count<T>();
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            if (Is > 0) sig += ",";
            sig += get_member_function_name<T, Is>();
            // Note: We skip parameter types in the signature to avoid nested pack expansion issues
            // Method count + names are sufficient for detecting API changes
        }(), ...);
    }(std::make_index_sequence<method_count>{});

    // Include file content hash if provided by build system
    // This catches implementation changes that reflection cannot detect
    if (file_hash && file_hash[0] != '\0') {
        sig += "|file_hash:";
        sig += file_hash;
    }

    sig += "}";
    return sig;
}

// Python object wrapper for C++ classes
template<typename T>
struct PyWrapper {
    PyObject_HEAD
    T* cpp_object;  // Pointer to the actual C++ object
    bool owns;      // Whether this wrapper owns the object (for cleanup)
};

// Deallocator for Python wrapper objects
template<typename T>
void py_dealloc(PyObject* self) {
    auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);
    if (wrapper->owns && wrapper->cpp_object) {
        delete wrapper->cpp_object;
    }
    Py_TYPE(self)->tp_free(self);
}

// ============================================================================
// Constructor Binding Support
// ============================================================================

// Count constructors (exclude default, copy, move)
template<typename T>
consteval std::size_t get_constructor_count() {
    auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
    std::size_t count = 0;
    for (auto member : all_members) {
        if (std::meta::is_constructor(member) &&
            !std::meta::is_copy_constructor(member) &&
            !std::meta::is_move_constructor(member)) {
            auto params = std::meta::parameters_of(member);
            // Only count non-default constructors
            if (params.size() > 0) {
                count++;
            }
        }
    }
    return count;
}

// Get the Nth non-default constructor
template<typename T, std::size_t Index>
consteval auto get_constructor() {
    auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
    std::size_t ctor_index = 0;
    for (auto member : all_members) {
        if (std::meta::is_constructor(member) &&
            !std::meta::is_copy_constructor(member) &&
            !std::meta::is_move_constructor(member)) {
            auto params = std::meta::parameters_of(member);
            if (params.size() > 0) {
                if (ctor_index == Index) {
                    return member;
                }
                ctor_index++;
            }
        }
    }
    return all_members[0];
}

// Get constructor parameter count
template<typename T, std::size_t CtorIndex>
consteval std::size_t get_constructor_param_count() {
    constexpr auto ctor = get_constructor<T, CtorIndex>();
    return std::meta::parameters_of(ctor).size();
}

// Get constructor parameter type
template<typename T, std::size_t CtorIndex, std::size_t ParamIndex>
consteval auto get_constructor_param_type() {
    constexpr auto ctor = get_constructor<T, CtorIndex>();
    auto params = std::meta::parameters_of(ctor);
    return std::meta::type_of(params[ParamIndex]);
}

// Variadic helper to call constructor with N parameters
// This uses compile-time index sequence to extract and convert each parameter
template<typename T, std::size_t CtorIndex, std::size_t... Is>
T* call_constructor_impl(PyObject* args, std::index_sequence<Is...>) {
    // Extract each parameter type
    using ParamTypes = std::tuple<
        std::remove_cvref_t<typename [:get_constructor_param_type<T, CtorIndex, Is>():]>...
    >;

    // Convert each Python argument to C++ type
    std::tuple<std::remove_cvref_t<typename [:get_constructor_param_type<T, CtorIndex, Is>():]>...> cpp_args;

    bool success = true;
    ([&] {
        if (!success) return;
        // Let overload resolution choose - non-template overloads have higher priority
        if (!from_python(PyTuple_GET_ITEM(args, Is), std::get<Is>(cpp_args))) {
            success = false;
        }
    }(), ...);

    if (!success) {
        return nullptr;
    }

    // Call constructor with unpacked arguments
    return new T(std::move(std::get<Is>(cpp_args))...);
}

// Initialize Python wrapper with parameterized constructor
// Uses reflection to find the best matching constructor
template<typename T>
int py_init(PyObject* self, PyObject* args, PyObject* kwds) {
    auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);

    Py_ssize_t nargs = PyTuple_Size(args);

    // Default constructor case - only if T is default constructible
    if (nargs == 0) {
        if constexpr (std::is_default_constructible_v<T>) {
            wrapper->cpp_object = new T();
            wrapper->owns = true;
            return 0;
        } else {
            PyErr_SetString(PyExc_TypeError, "This class requires constructor arguments");
            return -1;
        }
    }

    // Try to find matching constructor by parameter count
    constexpr std::size_t ctor_count = get_constructor_count<T>();

    bool found = false;
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            if (found) return;

            constexpr std::size_t param_count = get_constructor_param_count<T, Is>();
            if (nargs == static_cast<Py_ssize_t>(param_count)) {
                // Try to call this constructor
                T* obj = [&]() {
                    return call_constructor_impl<T, Is>(args,
                        std::make_index_sequence<param_count>{});
                }();

                if (obj) {
                    wrapper->cpp_object = obj;
                    wrapper->owns = true;
                    found = true;
                }
            }
        }(), ...);
    }(std::make_index_sequence<ctor_count>{});

    if (!found) {
        PyErr_SetString(PyExc_TypeError, "No matching constructor found");
        return -1;
    }

    return 0;
}

// Allocator for Python wrapper objects (used with tp_init)
template<typename T>
PyObject* py_new_with_init(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    auto* self = reinterpret_cast<PyWrapper<T>*>(type->tp_alloc(type, 0));
    if (self) {
        self->cpp_object = nullptr;  // Will be set by py_init
        self->owns = false;
    }
    return reinterpret_cast<PyObject*>(self);
}

// Fallback constructor for classes without parameterized constructors
// Only works for default-constructible types
template<typename T>
PyObject* py_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    auto* self = reinterpret_cast<PyWrapper<T>*>(type->tp_alloc(type, 0));
    if (self) {
        if constexpr (std::is_default_constructible_v<T>) {
            self->cpp_object = new T();
            self->owns = true;
        } else {
            // This path should not be reached if bind_class properly uses tp_init
            self->cpp_object = nullptr;
            self->owns = false;
        }
    }
    return reinterpret_cast<PyObject*>(self);
}

// ============================================================================
// Member Access Binding
// ============================================================================

// Helper to get member reflection info at compile-time using index - use cache
template<typename T, std::size_t Index>
consteval auto get_member_info() {
    return get_data_member<T, Index>();
}

// ============================================================================
// Getter for class members using reflection
// ============================================================================
// Template parameters:
//   T - The C++ class type being bound
//   Index - Compile-time index of the member to access (0, 1, 2, ...)
//
// Performance: This function is instantiated once per member at compile-time.
// The compiler generates specialized code for each member type, eliminating
// runtime type dispatch overhead present in traditional template-based bindings.
//
// Example generated code for "int counter":
//   return PyLong_FromLong(wrapper->cpp_object->counter);
// vs pybind11 (simplified):
//   return type_caster<int>::cast(getter_function<T, &T::counter>());
//
template<typename T, std::size_t Index>
PyObject* py_getter(PyObject* self, void* closure) {
    auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);
    if (!wrapper->cpp_object) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid C++ object");
        return nullptr;
    }

    // Reflection splice: Get the Nth member metadata at compile-time
    // [:member:] is a C++26 splicer that injects the actual member name
    constexpr auto member = get_member_info<T, Index>();
    using MemberType = typename [:std::meta::type_of(member):];

    // Direct member access via reflection - zero runtime overhead
    // Dereference pointer first to work around compiler bug with -> operator
    auto& value = (*wrapper->cpp_object).[:member:];

    // Type conversion to Python uses overload resolution:
    // - Arithmetic → PyLong_FromLong / PyFloat_FromDouble (direct C API)
    // - String → PyUnicode_FromStringAndSize (direct C API)
    // - Container → Recursive to_python for elements
    return to_python(value);
}

// Setter for class members using reflection
template<typename T, std::size_t Index>
int py_setter(PyObject* self, PyObject* value, void* closure) {
    auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);
    if (!wrapper->cpp_object) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid C++ object");
        return -1;
    }

    if (!value) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete attribute");
        return -1;
    }

    // Use reflection to access the member at the given index
    constexpr auto member = get_member_info<T, Index>();
    using MemberType = typename [:std::meta::type_of(member):];

    // Convert Python value to C++ type and set the member
    MemberType cpp_value;
    // Let overload resolution choose - non-template overloads have higher priority
    if (!from_python(value, cpp_value)) {
        PyErr_SetString(PyExc_TypeError, "Type conversion failed");
        return -1;
    }

    // Use move semantics for move-only types (like unique_ptr)
    // Dereference pointer first to work around compiler bug with -> operator
    (*wrapper->cpp_object).[:member:] = std::move(cpp_value);
    return 0;
}

// ============================================================================
// Method Binding Support with Variadic Parameters
// ============================================================================
//
// (Member function cache and all helper functions defined earlier in file)

// Modern C++ Variadic Parameter Handling
//
// This function demonstrates advanced C++26 techniques:
// 1. Template parameter packs (Is...) for compile-time iteration
// 2. Reflection splicers ([:expr:]) to "inject" reflected code
// 3. Fold expressions for elegant parameter pack expansion
// 4. std::tuple for heterogeneous parameter storage
//
// Flow: Python tuple → C++ tuple → method call with perfect unpacking
template<typename T, std::size_t FuncIndex, std::size_t... Is>
PyObject* call_method_impl(PyWrapper<T>* wrapper, PyObject* args, std::index_sequence<Is...>) {
    // Use reflection to get method metadata at compile-time
    constexpr auto member_func = get_member_function<T, FuncIndex>();
    constexpr auto return_type = get_method_return_type<T, FuncIndex>();
    using ReturnType = typename [:return_type:];  // Reflection splicer: inject type

    // Create tuple with exact types from reflection
    // The tuple type is: std::tuple<Param1Type, Param2Type, ...>
    // Each type is extracted via reflection and stripped of cv-qualifiers
    std::tuple<std::remove_cvref_t<typename [:get_method_param_type<T, FuncIndex, Is>():]>...> cpp_args;

    // Modern C++ fold expression: (expr, ...)
    // Expands to: lambda(0), lambda(1), lambda(2), ... for each Is
    // Each lambda converts one Python argument to its C++ type
    bool success = true;
    ([&] {
        if (!success) return;  // Short-circuit on first failure
        // from_python is overloaded - compiler picks the right one at compile-time
        if (!from_python(PyTuple_GET_ITEM(args, Is), std::get<Is>(cpp_args))) {
            PyErr_Format(PyExc_TypeError, "Argument %zu type conversion failed", Is);
            success = false;
        }
    }(), ...);  // <- Fold expression: repeat for each Is

    if (!success) {
        return nullptr;
    }

    // Call the C++ method using reflection splicer and parameter pack expansion
    // [:member_func:] is injected as the actual member function (e.g., &T::foo)
    // std::get<Is>(cpp_args)... expands to: get<0>(cpp_args), get<1>(cpp_args), ...
    // Note: We pass by lvalue reference (not std::move) to support methods that take T& parameters.
    // For value parameters, the compiler will copy; for ref parameters, it passes the ref.
    // Dereference pointer first to work around compiler bug with -> operator
    if constexpr (std::is_void_v<ReturnType>) {
        ((*wrapper->cpp_object).[:member_func:])(std::get<Is>(cpp_args)...);
        Py_RETURN_NONE;
    } else {
        ReturnType result = ((*wrapper->cpp_object).[:member_func:])(std::get<Is>(cpp_args)...);
        return to_python(result);  // Convert C++ return value back to Python
    }
}

// Python method wrapper template - now supports arbitrary parameter count!
template<typename T, std::size_t Index>
PyObject* py_method(PyObject* self, PyObject* args) {
    auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);
    if (!wrapper->cpp_object) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid C++ object");
        return nullptr;
    }

    constexpr std::size_t param_count = get_method_param_count<T, Index>();

    if (PyTuple_Size(args) != static_cast<Py_ssize_t>(param_count)) {
        PyErr_SetString(PyExc_TypeError, "Incorrect number of arguments");
        return nullptr;
    }

    try {
        // Dispatch to variadic helper with compile-time index sequence
        return call_method_impl<T, Index>(wrapper, args, std::make_index_sequence<param_count>{});
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    } catch (...) {
        PyErr_SetString(PyExc_RuntimeError, "Unknown C++ exception");
        return nullptr;
    }
}

// ============================================================================
// Static Method Support
// ============================================================================

// Helper to get static method parameter count
// Note: Static methods are plain functions, not member functions
template<typename T, std::size_t Index>
consteval std::size_t get_method_param_count_static() {
    constexpr auto func = get_static_member_function<T, Index>();
    // Get the function type directly from the splicer
    using FuncType = decltype([:func:]);
    // Count parameters - for static functions this is straightforward
    return std::meta::parameters_of(func).size();
}

// Helper to call static methods (no `self` parameter)
template<typename T, std::size_t Index, std::size_t... Is>
PyObject* call_static_method_impl(PyObject* args, std::index_sequence<Is...>) {
    constexpr auto static_func = get_static_member_function<T, Index>();
    constexpr auto return_type = get_static_method_return_type<T, Index>();
    using ReturnType = typename [:return_type:];

    // Create tuple with exact types from reflection
    std::tuple<std::remove_cvref_t<typename [:get_static_method_param_type<T, Index, Is>():]>...> cpp_args;

    // Extract parameters from Python tuple
    bool success = true;
    ([&] {
        if (!success) return;
        if (!from_python(PyTuple_GET_ITEM(args, Is), std::get<Is>(cpp_args))) {
            PyErr_Format(PyExc_TypeError, "Argument %zu type conversion failed", Is);
            success = false;
        }
    }(), ...);

    if (!success) {
        return nullptr;
    }

    // Call the static C++ method using reflection splicer
    // Use lvalue refs (not std::move) to support T& parameters
    if constexpr (std::is_void_v<ReturnType>) {
        [:static_func:](std::get<Is>(cpp_args)...);
        Py_RETURN_NONE;
    } else {
        ReturnType result = [:static_func:](std::get<Is>(cpp_args)...);
        return to_python(result);
    }
}

// Python static method wrapper
template<typename T, std::size_t Index>
PyObject* py_static_method(PyObject* /* self */, PyObject* args) {
    constexpr std::size_t param_count = get_method_param_count_static<T, Index>();

    if (PyTuple_Size(args) != static_cast<Py_ssize_t>(param_count)) {
        PyErr_SetString(PyExc_TypeError, "Incorrect number of arguments");
        return nullptr;
    }

    try {
        return call_static_method_impl<T, Index>(args, std::make_index_sequence<param_count>{});
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    } catch (...) {
        PyErr_SetString(PyExc_RuntimeError, "Unknown C++ exception");
        return nullptr;
    }
}

// ============================================================================
// Method Overloading Support via Name Mangling
// ============================================================================

// Generate a mangled name for a method based on its parameter types
// E.g., "foo(int, double)" -> "foo_int_double"
template<typename T, std::size_t FuncIndex>
consteval auto generate_mangled_method_name() {
    constexpr auto func = get_member_function<T, FuncIndex>();
    constexpr auto base_name = std::meta::identifier_of(func);
    constexpr std::size_t param_count = get_method_param_count<T, FuncIndex>();

    // For zero parameters, no mangling needed
    if constexpr (param_count == 0) {
        return base_name;
    } else {
        // Build mangled name with parameter types
        // Note: We return the base name here as the mangling happens at runtime
        // in generate_methods() below
        return base_name;
    }
}

// Helper to generate type suffix for mangling
// Uses simple string concatenation instead of ostringstream for faster compilation
template<typename T, std::size_t FuncIndex>
std::string get_method_type_suffix() {
    constexpr std::size_t param_count = get_method_param_count<T, FuncIndex>();

    if (param_count == 0) {
        return "";
    }

    std::string result;
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr auto param_type = get_method_param_type<T, FuncIndex, Is>();
            auto type_str = std::meta::display_string_of(param_type);

            // Simplify type name: strip namespaces, cv-qualifiers, references
            std::string simplified;
            bool in_template = false;
            for (char c : std::string_view(type_str.data(), type_str.size())) {
                if (c == '<') in_template = true;
                if (c == '>') in_template = false;
                if (!in_template && (c == ':' || c == ' ' || c == '&' || c == '*')) continue;
                if (c == ',') c = '_';
                simplified += c;
            }

            result += "_";
            result += simplified;
        }(), ...);
    }(std::make_index_sequence<param_count>{});

    return result;
}

// ============================================================================
// Code Generation Helpers
// ============================================================================

// Helper template to get member info at compile-time - uses cache
template<typename T, std::size_t Index>
consteval const char* get_member_name() {
    return std::meta::identifier_of(get_data_member<T, Index>()).data();
}

// Generate Python getters/setters for all members of a class
template<typename T, std::size_t... Indices>
consteval auto generate_getsetters(std::index_sequence<Indices...>) {
    constexpr std::size_t count = sizeof...(Indices);

    // Create array of PyGetSetDef structures (one per member + sentinel)
    std::array<PyGetSetDef, count + 1> getsets{};

    // Populate entries using fold expression
    ((getsets[Indices] = PyGetSetDef{
        .name = get_member_name<T, Indices>(),
        .get = py_getter<T, Indices>,
        .set = py_setter<T, Indices>,
        .doc = nullptr,
        .closure = nullptr
    }), ...);

    // Sentinel entry (all nulls)
    getsets[count] = PyGetSetDef{nullptr, nullptr, nullptr, nullptr, nullptr};

    return getsets;
}

// Generate Python methods for all member functions of a class
// Handles overloading by mangling names with type suffixes
template<typename T, std::size_t... Indices>
auto generate_methods(std::index_sequence<Indices...>) {
    constexpr std::size_t count = sizeof...(Indices);

    // Pre-allocate storage for mangled names (if needed)
    static std::array<std::string, count> mangled_names;

    // Build methods array
    std::array<PyMethodDef, count + 1> methods{};

    // Populate entries - check each method individually
    ([&] {
        constexpr auto base_name = get_member_function_name<T, Indices>();
        constexpr bool is_overloaded = is_method_overloaded<T, Indices>();

        if constexpr (is_overloaded) {
            // Generate mangled name with type suffix
            mangled_names[Indices] = std::string(base_name) + get_method_type_suffix<T, Indices>();
            methods[Indices] = PyMethodDef{
                .ml_name = mangled_names[Indices].c_str(),
                .ml_meth = reinterpret_cast<PyCFunction>(py_method<T, Indices>),
                .ml_flags = METH_VARARGS,
                .ml_doc = nullptr
            };
        } else {
            // No overload, use base name
            methods[Indices] = PyMethodDef{
                .ml_name = base_name,
                .ml_meth = reinterpret_cast<PyCFunction>(py_method<T, Indices>),
                .ml_flags = METH_VARARGS,
                .ml_doc = nullptr
            };
        }
    }(), ...);

    // Sentinel entry
    methods[count] = PyMethodDef{nullptr, nullptr, 0, nullptr};

    return methods;
}

// Generate Python static methods for all static member functions of a class
template<typename T, std::size_t... Indices>
auto generate_static_methods(std::index_sequence<Indices...>) {
    constexpr std::size_t count = sizeof...(Indices);

    // Build methods array
    std::array<PyMethodDef, count + 1> methods{};

    // Populate entries
    ([&] {
        constexpr auto func_name = get_static_member_function_name<T, Indices>();
        methods[Indices] = PyMethodDef{
            .ml_name = func_name,
            .ml_meth = reinterpret_cast<PyCFunction>(py_static_method<T, Indices>),
            .ml_flags = METH_VARARGS,  // No METH_STATIC - we wrap with PyStaticMethod_New instead
            .ml_doc = nullptr
        };
    }(), ...);

    // Sentinel entry
    methods[count] = PyMethodDef{nullptr, nullptr, 0, nullptr};

    return methods;
}

// ============================================================================
// Nested Bindable Conversion: dict vs. Wrapper Object Semantics
// ============================================================================
//
// DESIGN DECISION: Nested Bindable objects are converted to Python dicts,
// NOT to wrapper objects (PyWrapper<T>). This is a fundamental semantic choice.
//
// Example:
//   struct Inner { int x; };
//   struct Outer { Inner inner; };
//
//   bind_class<Outer>(m, "Outer");  // Creates Python type "Outer"
//   bind_class<Inner>(m, "Inner");  // Creates Python type "Inner"
//
// In Python:
//   obj = Outer()
//   obj.inner         # Returns a plain dict: {'x': 0}
//   type(obj.inner)   # <class 'dict'>, NOT <class 'Inner'>
//
// RATIONALE:
// 1. **Simplicity**: No need to track which nested types have been bound
// 2. **Consistency**: Works even if Inner is never explicitly bound
// 3. **Performance**: Avoids creating wrapper objects for temporary values
// 4. **Matches common patterns**: Similar to JSON/dict-based APIs
//
// TRADE-OFFS:
// - ✓ PRO: Simple, predictable, no registration dependencies
// - ✓ PRO: Works with any Bindable type, even unbound ones
// - ✗ CON: Nested objects lose their Python type identity
// - ✗ CON: No method calls on nested objects (dict has no methods)
//
// COMPARISON TO OTHER LIBRARIES:
// - pybind11/nanobind: Return wrapper objects if type is bound, otherwise error
// - Boost.Python: Always return wrapper objects (requires pre-registration)
// - SWIG: Returns proxy objects with full type information
//
// FUTURE EXTENSIONS:
// Could add policy customization via reflection attributes:
//   struct Outer {
//       [[mirror_bridge::as_object]] Inner inner;  // Return wrapper, not dict
//   };
//
// Or a global policy flag:
//   bind_class<Outer, NestedAsObject>(m, "Outer");
//
// Helper to auto-generate non-template conversion overloads using reflection
// This ensures proper overload resolution for types used in smart pointers
template<typename T>
struct ConversionOverloadGenerator {
    // Generate to_python overload at compile time
    // NOTE: Converts nested Bindable objects to dicts (see documentation above)
    static PyObject* to_python_impl(const T& obj) {
        PyObject* dict = PyDict_New();
        if (!dict) return nullptr;

        constexpr std::size_t member_count = get_nested_member_count<T>();
        bool success = true;

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (!success) return;

                constexpr auto member = get_nested_member<T, Is>();
                constexpr auto name = get_nested_member_name<T, Is>();

                const auto& value = obj.[:member:];
                PyObject* py_value = to_python(value);

                if (!py_value || PyDict_SetItemString(dict, name, py_value) < 0) {
                    success = false;
                    Py_XDECREF(py_value);
                } else {
                    Py_DECREF(py_value);
                }
            }(), ...);
        }(std::make_index_sequence<member_count>{});

        if (!success) {
            Py_DECREF(dict);
            return nullptr;
        }

        return dict;
    }

    // Generate from_python overload at compile time
    static bool from_python_impl(PyObject* obj, T& out) {
        if (!PyDict_Check(obj)) return false;

        constexpr std::size_t member_count = get_nested_member_count<T>();
        bool success = true;

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (!success) return;

                constexpr auto member = get_nested_member<T, Is>();
                constexpr auto name = get_nested_member_name<T, Is>();
                using MemberType = NestedMemberType<T, Is>;

                PyObject* py_value = PyDict_GetItemString(obj, name);
                if (!py_value) {
                    success = false;
                    return;
                }

                MemberType cpp_value;
                if (!from_python(py_value, cpp_value)) {
                    success = false;
                    return;
                }

                out.[:member:] = std::move(cpp_value);
            }(), ...);
        }(std::make_index_sequence<member_count>{});

        return success;
    }
};

// ============================================================================
// Python-Based Global Type Registry
// ============================================================================
//
// Problem: C++ inline static variables are per-shared-library, so types
// registered in vertex.so can't be found by bbox.so's to_python().
//
// Solution: Use Python's sys.modules to store type information globally.
// We create a hidden module "_mirror_bridge_types" that holds a dict mapping
// C++ type names (via typeid) to Python type objects. This dict is shared
// across all .so files since it lives in Python's runtime.

// Get or create the global type registry dict in Python
// This is NOT in an anonymous namespace so it can be used by the public API below
inline PyObject* get_python_type_registry() {
    // Get sys.modules
    PyObject* sys_modules = PyImport_GetModuleDict();
    if (!sys_modules) return nullptr;

    // Check if our registry module exists
    const char* registry_name = "_mirror_bridge_types";
    PyObject* registry = PyDict_GetItemString(sys_modules, registry_name);

    if (!registry) {
        // Create a new dict to serve as our registry
        registry = PyDict_New();
        if (!registry) return nullptr;

        // Store it in sys.modules so it persists across imports
        PyDict_SetItemString(sys_modules, registry_name, registry);
        Py_DECREF(registry);  // sys.modules now owns it

        // Get it back (borrowed reference)
        registry = PyDict_GetItemString(sys_modules, registry_name);
    }

    return registry;
}

namespace {
    // Register a type in the Python-based global registry
    template<typename T>
    void register_type_in_python(PyTypeObject* py_type) {
        PyObject* registry = get_python_type_registry();
        if (!registry) return;

        // Use typeid name as the key (unique per type across all modules)
        const char* type_name = typeid(T).name();
        PyDict_SetItemString(registry, type_name, reinterpret_cast<PyObject*>(py_type));
    }

    // Look up a type from the Python-based global registry
    template<typename T>
    PyTypeObject* lookup_type_in_python() {
        PyObject* registry = get_python_type_registry();
        if (!registry) return nullptr;

        const char* type_name = typeid(T).name();
        PyObject* py_type = PyDict_GetItemString(registry, type_name);

        return py_type ? reinterpret_cast<PyTypeObject*>(py_type) : nullptr;
    }
}

// ============================================================================
// Public API for Type Registry Management
// ============================================================================
//
// The global type registry enables cross-module type sharing. These functions
// allow inspection and cleanup of the registry.
//
// Usage:
//   // In Python, you can also access directly:
//   import sys
//   registry = sys.modules.get('_mirror_bridge_types', {})
//   print(f"Registered types: {len(registry)}")
//
//   // Or use the C++ API from binding code:
//   mirror_bridge::clear_type_registry();  // Clear all registrations
//

// Clear all type registrations (useful for testing or module reloading)
inline void clear_type_registry() {
    PyObject* registry = get_python_type_registry();
    if (registry) {
        PyDict_Clear(registry);
    }
}

// Get the number of registered types
inline Py_ssize_t get_registered_type_count() {
    PyObject* registry = get_python_type_registry();
    return registry ? PyDict_Size(registry) : 0;
}

// Unregister a specific type (useful when reloading a module)
template<typename T>
void unregister_type() {
    PyObject* registry = get_python_type_registry();
    if (!registry) return;

    const char* type_name = typeid(T).name();
    PyDict_DelItemString(registry, type_name);
    PyErr_Clear();  // Ignore KeyError if type wasn't registered
}

// Auto-generated conversion overloads using SFINAE (not requires clause)
// SFINAE ensures clean template symbols without constraint mangling
//
// UPDATED: When a type is bound via bind_class<T>, we return a proper wrapper
// object instead of a dict. This allows methods to work on returned objects.
//
// CROSS-MODULE SUPPORT: Uses Python-based global registry to look up types
// that may have been registered in a different shared library (.so).
// This enables scenarios like:
//   - vertex.so binds Vertex class
//   - bbox.so binds BoundingBox class that has Vertex members
//   - BoundingBox.get_corner() returns a proper Vertex wrapper (not a dict)
template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    PyObject*
>
to_python(const T& obj) {
    using CleanT = std::remove_cvref_t<T>;

    // First check Python-based global registry (works across shared libraries)
    PyTypeObject* py_type = lookup_type_in_python<CleanT>();

    // Fall back to local TypeRegistry for single-module scenarios
    if (!py_type) {
        py_type = TypeRegistry<CleanT>::py_type;
    }

    if (py_type) {
        // Create a wrapper object with a copy of the C++ object
        auto* wrapper = reinterpret_cast<PyWrapper<CleanT>*>(py_type->tp_alloc(py_type, 0));
        if (!wrapper) {
            return nullptr;
        }
        wrapper->cpp_object = new CleanT(obj);  // Copy the object
        wrapper->owns = true;
        return reinterpret_cast<PyObject*>(wrapper);
    }

    // Fall back to dict conversion for unregistered types
    return ConversionOverloadGenerator<T>::to_python_impl(obj);
}

template<typename T>
std::enable_if_t<
    Bindable<T> && !StringLike<T> && !Container<T> && !Arithmetic<T> && !SmartPointer<T>,
    bool
>
from_python(PyObject* obj, T& out) {
    return ConversionOverloadGenerator<T>::from_python_impl(obj, out);
}

// Main binding function - generates Python type object for a C++ class
// Optionally accepts a file content hash for implementation change detection
template<Bindable T>
PyTypeObject* bind_class(PyObject* module, const char* name, const char* file_hash = nullptr) {
    // Generate type signature for hash-based change detection
    auto signature = generate_type_signature<T>(file_hash);

    // Register class in the global registry
    Registry::instance().register_class(name, signature);

    // Get member count for generating getters/setters - use cache
    constexpr std::size_t member_count = get_data_member_count<T>();

    // Generate getters/setters using reflection
    static auto getsetters = generate_getsetters<T>(std::make_index_sequence<member_count>{});

    // Generate instance methods using reflection
    constexpr std::size_t method_count = get_member_function_count<T>();
    static auto methods = generate_methods<T>(std::make_index_sequence<method_count>{});

    // Generate static methods using reflection (will be added separately to type dict)
    constexpr std::size_t static_method_count = get_static_member_function_count<T>();
    static auto static_methods = generate_static_methods<T>(std::make_index_sequence<static_method_count>{});

    // Check if class has parameterized constructors
    constexpr std::size_t ctor_count = get_constructor_count<T>();

    // Store class name statically for repr
    static const char* class_name = name;

    // Generic repr function for all types (optimized: avoid ostringstream overhead)
    static auto py_repr_func = +[](PyObject* self) -> PyObject* {
        auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "<%s object at %p>", class_name, static_cast<void*>(wrapper->cpp_object));
        return PyUnicode_FromString(buffer);
    };

    // Define the Python type structure
    // Note: Inheritance is implicitly supported - reflection sees all members including inherited ones
    static PyTypeObject type_object = {
        PyVarObject_HEAD_INIT(nullptr, 0)
        .tp_name = name,
        .tp_basicsize = sizeof(PyWrapper<T>),
        .tp_itemsize = 0,
        .tp_dealloc = py_dealloc<T>,
        .tp_repr = (reprfunc)py_repr_func,
        .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  // Allow subclassing
        .tp_doc = "Auto-generated binding via mirror_bridge reflection",
        .tp_methods = methods.data(),
        .tp_getset = getsetters.data(),
        .tp_init = (ctor_count > 0) ? (initproc)py_init<T> : nullptr,
        .tp_new = (ctor_count > 0) ? py_new_with_init<T> : py_new<T>,
    };

    // Initialize and register the type with Python
    if (PyType_Ready(&type_object) < 0) {
        return nullptr;
    }

    // Add static methods to the type dictionary
    // Python's PyMethodDef with METH_STATIC doesn't work - we need to manually wrap them
    if constexpr (static_method_count > 0) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                constexpr auto func_name = get_static_member_function_name<T, Is>();
                // Create a PyCFunction for the static method
                PyObject* func = PyCFunction_New(&static_methods[Is], nullptr);
                if (func) {
                    // Wrap it as a static method
                    PyObject* static_method = PyStaticMethod_New(func);
                    Py_DECREF(func);
                    if (static_method) {
                        // Add to type dict
                        PyObject* type_dict = type_object.tp_dict;
                        PyDict_SetItemString(type_dict, func_name, static_method);
                        Py_DECREF(static_method);
                    }
                }
            }(), ...);
        }(std::make_index_sequence<static_method_count>{});
    }

    Py_INCREF(&type_object);
    if (PyModule_AddObject(module, name, reinterpret_cast<PyObject*>(&type_object)) < 0) {
        Py_DECREF(&type_object);
        return nullptr;
    }

    // Update registry with the Python type object
    Registry::instance().set_py_type(name, &type_object);

    // Register type in TypeRegistry for type-based lookup in to_python
    TypeRegistry<T>::py_type = &type_object;

    // Register in Python-based global registry for cross-module type sharing
    // This allows types bound in one .so file to be properly returned
    // from methods in another .so file (C++ static variables are per-.so)
    register_type_in_python<T>(&type_object);

    return &type_object;
}

} // namespace mirror_bridge

// ============================================================================
// Convenience Macros
// ============================================================================

// Register a class for Python binding
#define MIRROR_BRIDGE_REGISTER(ClassName) \
    MIRROR_BRIDGE_REGISTER_WITH_HASH(ClassName, nullptr)

// Register a class with an explicit file content hash
#define MIRROR_BRIDGE_REGISTER_WITH_HASH(ClassName, FileHash) \
    namespace { \
        struct ClassName##_Registrar { \
            ClassName##_Registrar() { \
                auto sig = mirror_bridge::generate_type_signature<ClassName>(FileHash); \
                mirror_bridge::Registry::instance().register_class(#ClassName, sig); \
            } \
        }; \
        static ClassName##_Registrar ClassName##_registrar_instance; \
    }

// ============================================================================
// Extern Template Declarations - Reduce Redundant Instantiations
// ============================================================================
//
// For common types (int, double, std::string), we declare extern templates
// to avoid recompiling the same instantiations in every translation unit.
// This is a key technique used by nanobind to speed up compilation.
//
// The actual instantiations should be in a single .cpp file (if you have one),
// or you can let the first binding file instantiate them implicitly.

// Uncomment these if you have multiple binding files and want to explicitly
// control template instantiation:
//
// extern template PyObject* to_python<int>(const int&);
// extern template PyObject* to_python<double>(const double&);
// extern template PyObject* to_python<std::string>(const std::string&);
// extern template bool from_python<int>(PyObject*, int&);
// extern template bool from_python<double>(PyObject*, double&);
// extern template bool from_python<std::string>(PyObject*, std::string&);

// ============================================================================
// Module Definition Macro
// ============================================================================

// Create a Python module with registered classes
#define MIRROR_BRIDGE_MODULE(module_name, ...) \
    static PyModuleDef module_name##_def = { \
        PyModuleDef_HEAD_INIT, \
        #module_name, \
        "Auto-generated module via mirror_bridge", \
        -1, \
        nullptr \
    }; \
    PyMODINIT_FUNC PyInit_##module_name() { \
        PyObject* m = PyModule_Create(&module_name##_def); \
        if (!m) return nullptr; \
        __VA_ARGS__ \
        return m; \
    }
