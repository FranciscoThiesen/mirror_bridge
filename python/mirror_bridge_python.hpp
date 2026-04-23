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
//   - std::meta::members_of(^^T, access_context) - discover class structure
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
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <tuple>
#include <variant>
#include <optional>
#include <expected>
#include <future>
#include <chrono>
#include <type_traits>
#include <concepts>
#include <memory>
#include <functional>
#include <cstdint>  // For uint8_t, int32_t, etc.
#include <cstdio>   // For snprintf in simple repr functions
#include <mutex>    // For std::once_flag in thread-safe initialization

// Include core header for GlobalTypeRegistry
#include "core/mirror_bridge_core.hpp"

// Include P3394 annotations support for field-level binding control
// Requires -freflection-latest with Bloomberg's clang-p2996
#include "python/mirror_bridge_annotations.hpp"

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

// Library version and capabilities. Guarded so we don't redefine the
// constants already set in mirror_bridge_core.hpp (and to keep the
// two headers from disagreeing at every user build).
#ifndef MIRROR_BRIDGE_VERSION_MAJOR
  #define MIRROR_BRIDGE_VERSION_MAJOR 0
  #define MIRROR_BRIDGE_VERSION_MINOR 2
  #define MIRROR_BRIDGE_VERSION_PATCH 0
#endif

// Supported reflection features (for client code to check capabilities)
#ifndef MIRROR_BRIDGE_HAS_REFLECTION
  #define MIRROR_BRIDGE_HAS_REFLECTION 1
  #define MIRROR_BRIDGE_HAS_ENUMERATORS_OF 1  // std::meta::enumerators_of for enum validation
  #define MIRROR_BRIDGE_HAS_TYPE_SIGNATURES 1  // full type signatures with methods
#endif

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

// Helper trait to detect std::optional
template<typename T>
struct is_std_optional : std::false_type {};

template<typename T>
struct is_std_optional<std::optional<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_std_optional_v = is_std_optional<std::remove_cvref_t<T>>::value;

// Concept to identify std::optional types
template<typename T>
concept Optional = is_std_optional_v<T>;

// Helper trait to detect std::expected
template<typename T>
struct is_std_expected : std::false_type {};

template<typename T, typename E>
struct is_std_expected<std::expected<T, E>> : std::true_type {};

template<typename T>
inline constexpr bool is_std_expected_v = is_std_expected<std::remove_cvref_t<T>>::value;

// Concept to identify std::expected types
template<typename T>
concept Expected = is_std_expected_v<T>;

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

// Forward declarations for Container to_python/from_python. These are needed
// because compound-type templates (std::pair, std::tuple, std::vector of Containers,
// etc.) may be instantiated with Container-typed elements, and two-phase name
// lookup requires the overload to be visible at the compound-type template's
// definition point.
template<Container T>
PyObject* to_python(const T& container);

template<Container T>
bool from_python(PyObject* obj, T& container);

} // end namespace mirror_bridge (temporarily close so Eigen overloads
  // are declared in the right scope before compound-type templates)

// Include Eigen type converters inline so their to_python/from_python
// overloads are visible to compound-type templates (std::pair, std::tuple,
// std::optional, etc.) that may be instantiated with Eigen types. Without
// this, two-phase lookup inside these templates can't find the Eigen
// overloads because ADL looks in Eigen's namespace (no to_python there)
// and non-ADL looks at definition point.
#include "mirror_bridge_eigen.hpp"

namespace mirror_bridge {

// ============================================================================
// Exception Translation: C++ std:: exceptions → Python exception types
// ============================================================================
//
// Without this, every C++ exception thrown through a bound method surfaces
// as `RuntimeError` in Python. Open3D and most real-world C++ libraries
// throw std::invalid_argument / std::out_of_range / std::runtime_error with
// meaningful intent that Python users expect as ValueError / IndexError /
// RuntimeError respectively. This helper picks the most specific Python
// exception class via dynamic_cast.
inline void set_py_error_from_cpp_exception(const std::exception& e) {
    if (dynamic_cast<const std::out_of_range*>(&e)) {
        PyErr_SetString(PyExc_IndexError, e.what());
    } else if (dynamic_cast<const std::invalid_argument*>(&e)) {
        PyErr_SetString(PyExc_ValueError, e.what());
    } else if (dynamic_cast<const std::domain_error*>(&e)) {
        PyErr_SetString(PyExc_ValueError, e.what());
    } else if (dynamic_cast<const std::logic_error*>(&e)) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
    } else if (dynamic_cast<const std::runtime_error*>(&e)) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
    } else if (dynamic_cast<const std::bad_alloc*>(&e)) {
        PyErr_SetString(PyExc_MemoryError, e.what());
    } else {
        PyErr_SetString(PyExc_RuntimeError, e.what());
    }
}

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

// Smart pointer conversion - dereferences and converts the pointee.
//
// For bound types (registered via bind_class): returns a proper PyWrapper<T>
// object with full method and property access — NOT a dict.
//
// For unbound types: falls back to dict conversion via the Bindable overload.
//
// Null pointers return Python None.
//
// Example:
//   struct Foo { int x; double length() const; };
//   bind_class<Foo>(m, "Foo");
//   std::shared_ptr<Foo> ptr = ...;
//   to_python(ptr) → returns Foo wrapper object with .x and .length()
template<SmartPointer T>
inline PyObject* to_python(const T& ptr) {
    if (!ptr) {
        Py_RETURN_NONE;
    }
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
// Creates a new instance from the Python value. For smart pointers to abstract
// or non-default-constructible types (e.g., std::shared_ptr<OctreeNode> where
// OctreeNode is abstract), only None→reset is supported — materializing the
// pointee by value is physically impossible.
template<SmartPointer T>
inline bool from_python(PyObject* obj, T& out) {
    using ElementType = typename std::remove_cvref_t<T>::element_type;

    if (obj == Py_None) {
        out.reset();
        return true;
    }

    if constexpr (std::is_abstract_v<ElementType> ||
                  !std::is_default_constructible_v<ElementType> ||
                  !std::is_copy_assignable_v<ElementType>) {
        // Can't reconstruct an abstract/non-default-constructible pointee from
        // a Python dict representation. Leave the smart pointer as-is for non-
        // None inputs; the caller is expected to populate via typed setters.
        return true;
    } else {
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
}

// ============================================================================
// std::optional Type Conversion
// ============================================================================
//
// Enables automatic conversion between C++ std::optional<T> and Python values.
// - std::nullopt / empty optional → Python None
// - std::optional with value → Python value of type T
// - Python None → std::nullopt
// - Python value → std::optional containing converted value
//
// Example C++ code:
//   struct Config {
//       std::optional<int> timeout;
//       std::optional<std::string> name;
//   };
//
// Python usage:
//   config.timeout = 30      # Sets optional to 30
//   config.timeout = None    # Sets optional to nullopt
//   if config.name is not None:
//       print(config.name)

// Convert std::optional to Python (None if empty, otherwise convert value)
template<typename T>
inline PyObject* to_python(const std::optional<T>& opt) {
    if (!opt.has_value()) {
        Py_RETURN_NONE;
    }
    return to_python(*opt);
}

// Convert Python to std::optional (None → nullopt, otherwise convert value)
template<typename T>
inline bool from_python(PyObject* obj, std::optional<T>& out) {
    if (obj == Py_None) {
        out = std::nullopt;
        return true;
    }

    T value;
    if (!from_python(obj, value)) {
        return false;
    }
    out = std::move(value);
    return true;
}

// ============================================================================
// std::expected Type Conversion
// ============================================================================
//
// Enables automatic conversion between C++ std::expected<T, E> and Python values.
// On success (has_value): returns the converted T value to Python.
// On error (!has_value): raises a Python ValueError with the error converted to string.
//
// This lets C++ APIs use std::expected for error handling while Python callers
// get natural exception-based error propagation.
//
// Example C++ code:
//   std::expected<double, std::string> safe_divide(double a, double b) {
//       if (b == 0.0) return std::unexpected("division by zero");
//       return a / b;
//   }
//
// Python usage:
//   result = obj.safe_divide(10.0, 2.0)   # Returns 5.0
//   result = obj.safe_divide(10.0, 0.0)   # Raises ValueError: "division by zero"

// Convert std::expected to Python (value on success, raise exception on error)
template<typename T, typename E>
inline PyObject* to_python(const std::expected<T, E>& exp) {
    if (exp.has_value()) {
        if constexpr (std::is_void_v<T>) {
            Py_RETURN_NONE;
        } else {
            return to_python(*exp);
        }
    }

    // Convert the error to a string message for the Python exception
    if constexpr (std::is_same_v<std::remove_cvref_t<E>, std::string>) {
        PyErr_SetString(PyExc_ValueError, exp.error().c_str());
    } else if constexpr (std::is_arithmetic_v<std::remove_cvref_t<E>>) {
        std::string msg = "expected failed with error code: " + std::to_string(exp.error());
        PyErr_SetString(PyExc_ValueError, msg.c_str());
    } else if constexpr (Bindable<std::remove_cvref_t<E>>) {
        // For bindable error types, convert to dict repr
        PyObject* err_obj = to_python(exp.error());
        if (err_obj) {
            PyObject* repr = PyObject_Repr(err_obj);
            if (repr) {
                PyErr_SetObject(PyExc_ValueError, repr);
                Py_DECREF(repr);
            } else {
                PyErr_SetString(PyExc_ValueError, "expected contained an error");
            }
            Py_DECREF(err_obj);
        } else {
            PyErr_SetString(PyExc_ValueError, "expected contained an error");
        }
    } else {
        PyErr_SetString(PyExc_ValueError, "expected contained an error");
    }
    return nullptr;
}

// Convert Python to std::expected (always produces a success value; errors are
// represented as Python exceptions, not as expected error states)
template<typename T, typename E>
inline bool from_python(PyObject* obj, std::expected<T, E>& out) {
    if constexpr (std::is_void_v<T>) {
        out = std::expected<T, E>{};
        return true;
    } else {
        T value;
        if (!from_python(obj, value)) {
            return false;
        }
        out = std::move(value);
        return true;
    }
}

// ============================================================================
// Async/Await Support - std::future<T> to Python Awaitable
// ============================================================================
//
// Enables C++ methods returning std::future<T> to be awaited in Python async code.
//
// Example C++ code:
//   std::future<int> compute_async(int x) {
//       return std::async(std::launch::async, [x]() {
//           std::this_thread::sleep_for(std::chrono::seconds(1));
//           return x * 2;
//       });
//   }
//
// Python usage:
//   async def main():
//       result = await obj.compute_async(21)
//       print(result)  # 42
//
// The awaitable wrapper polls the future and yields control back to the event
// loop if the result is not ready yet.

// Helper trait to detect std::future
template<typename T>
struct is_std_future : std::false_type {};

template<typename T>
struct is_std_future<std::future<T>> : std::true_type {};

template<typename T>
struct is_std_future<std::shared_future<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_std_future_v = is_std_future<std::remove_cvref_t<T>>::value;

// Concept for future types
template<typename T>
concept Future = is_std_future_v<T>;

// Python awaitable wrapper for std::future<T>
template<typename T>
struct PyFutureAwaitable {
    PyObject_HEAD
    std::shared_future<T>* future;  // Use shared_future for copyability
    bool done;
};

// Dealloc for PyFutureAwaitable
template<typename T>
void future_awaitable_dealloc(PyObject* self) {
    auto* awaitable = reinterpret_cast<PyFutureAwaitable<T>*>(self);
    delete awaitable->future;
    Py_TYPE(self)->tp_free(self);
}

// __await__ method - returns self (awaitable is its own iterator)
template<typename T>
PyObject* future_awaitable_await(PyObject* self) {
    Py_INCREF(self);
    return self;
}

// __iter__ method - returns self
template<typename T>
PyObject* future_awaitable_iter(PyObject* self) {
    Py_INCREF(self);
    return self;
}

// __next__ method - checks if future is ready
template<typename T>
PyObject* future_awaitable_next(PyObject* self) {
    auto* awaitable = reinterpret_cast<PyFutureAwaitable<T>*>(self);

    if (awaitable->done) {
        PyErr_SetString(PyExc_StopIteration, "Future already completed");
        return nullptr;
    }

    // Check if future is ready (with zero timeout)
    auto status = awaitable->future->wait_for(std::chrono::seconds(0));

    if (status == std::future_status::ready) {
        // Future is ready - get result and raise StopIteration with value
        awaitable->done = true;

        try {
            if constexpr (std::is_void_v<T>) {
                awaitable->future->get();
                // For void futures, StopIteration value is None
                PyErr_SetNone(PyExc_StopIteration);
            } else {
                T result = awaitable->future->get();
                PyObject* py_result = to_python(result);
                if (!py_result) {
                    return nullptr;
                }
                // Set StopIteration with the result value
                PyErr_SetObject(PyExc_StopIteration, py_result);
                Py_DECREF(py_result);
            }
        } catch (const std::exception& e) {
            set_py_error_from_cpp_exception(e);
        }
        return nullptr;
    }

    // Future not ready - yield None to give control back to event loop
    Py_RETURN_NONE;
}

// Create the PyTypeObject for PyFutureAwaitable<T>
template<typename T>
PyTypeObject* get_future_awaitable_type() {
    static PyAsyncMethods async_methods = {
        .am_await = future_awaitable_await<T>,
    };

    static PyTypeObject type_object = {
        PyVarObject_HEAD_INIT(nullptr, 0)
        .tp_name = "FutureAwaitable",
        .tp_basicsize = sizeof(PyFutureAwaitable<T>),
        .tp_itemsize = 0,
        .tp_dealloc = future_awaitable_dealloc<T>,
        .tp_as_async = &async_methods,
        .tp_flags = Py_TPFLAGS_DEFAULT,
        .tp_doc = "Awaitable wrapper for C++ std::future",
        .tp_iter = future_awaitable_iter<T>,
        .tp_iternext = future_awaitable_next<T>,
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

// Convert std::future<T> to Python awaitable
template<typename T>
inline PyObject* to_python(std::future<T>&& fut) {
    PyTypeObject* type = get_future_awaitable_type<T>();
    if (!type) return nullptr;

    auto* awaitable = reinterpret_cast<PyFutureAwaitable<T>*>(type->tp_alloc(type, 0));
    if (!awaitable) return nullptr;

    // Convert to shared_future for copyability
    awaitable->future = new std::shared_future<T>(std::move(fut));
    awaitable->done = false;

    return reinterpret_cast<PyObject*>(awaitable);
}

// Convert std::shared_future<T> to Python awaitable
template<typename T>
inline PyObject* to_python(const std::shared_future<T>& fut) {
    PyTypeObject* type = get_future_awaitable_type<T>();
    if (!type) return nullptr;

    auto* awaitable = reinterpret_cast<PyFutureAwaitable<T>*>(type->tp_alloc(type, 0));
    if (!awaitable) return nullptr;

    awaitable->future = new std::shared_future<T>(fut);
    awaitable->done = false;

    return reinterpret_cast<PyObject*>(awaitable);
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
// C-style Array Support (e.g., float[4], int[10])
// ============================================================================
//
// Some C/C++ classes use raw arrays as members (e.g., Open3D's MaterialParameter
// with float f4[4]). Convert to/from Python lists.

template<typename T, std::size_t N>
    requires (!std::is_same_v<std::remove_cv_t<T>, char>)  // exclude char[] (string-like)
PyObject* to_python(const T (&arr)[N]) {
    PyObject* list = PyList_New(N);
    if (!list) return nullptr;
    for (std::size_t i = 0; i < N; ++i) {
        PyObject* elem = to_python(arr[i]);
        if (!elem) { Py_DECREF(list); return nullptr; }
        PyList_SET_ITEM(list, i, elem);
    }
    return list;
}

template<typename T, std::size_t N>
    requires (!std::is_same_v<std::remove_cv_t<T>, char>)
bool from_python(PyObject* obj, T (&arr)[N]) {
    if (!PySequence_Check(obj) || PySequence_Size(obj) != static_cast<Py_ssize_t>(N)) {
        return false;
    }
    for (std::size_t i = 0; i < N; ++i) {
        PyObject* item = PySequence_GetItem(obj, i);
        if (!item) return false;
        T val;
        if (!from_python(item, val)) {
            Py_DECREF(item);
            return false;
        }
        arr[i] = val;
        Py_DECREF(item);
    }
    return true;
}

// ============================================================================
// std::map / std::unordered_map Support - Associative Containers
// ============================================================================
//
// Enables automatic conversion between C++ maps and Python dicts.
//
// Example C++ code:
//   struct Config {
//       std::map<std::string, int> settings;
//       std::unordered_map<int, std::string> labels;
//   };
//
// Python usage:
//   config.settings = {"timeout": 30, "retries": 3}
//   config.labels = {1: "error", 2: "warning", 3: "info"}
//   print(config.settings["timeout"])  # 30

// Helper traits to detect map types
template<typename T>
struct is_std_map : std::false_type {};

template<typename K, typename V, typename... Args>
struct is_std_map<std::map<K, V, Args...>> : std::true_type {};

template<typename T>
struct is_std_unordered_map : std::false_type {};

template<typename K, typename V, typename... Args>
struct is_std_unordered_map<std::unordered_map<K, V, Args...>> : std::true_type {};

template<typename T>
inline constexpr bool is_map_like_v =
    is_std_map<std::remove_cvref_t<T>>::value ||
    is_std_unordered_map<std::remove_cvref_t<T>>::value;

// Concept for map-like types
template<typename T>
concept MapLike = is_map_like_v<T>;

// Convert std::map to Python dict
template<typename K, typename V, typename... Args>
PyObject* to_python(const std::map<K, V, Args...>& map) {
    PyObject* dict = PyDict_New();
    if (!dict) return nullptr;

    for (const auto& [key, value] : map) {
        PyObject* py_key = to_python(key);
        if (!py_key) {
            Py_DECREF(dict);
            return nullptr;
        }
        PyObject* py_value = to_python(value);
        if (!py_value) {
            Py_DECREF(py_key);
            Py_DECREF(dict);
            return nullptr;
        }
        if (PyDict_SetItem(dict, py_key, py_value) < 0) {
            Py_DECREF(py_key);
            Py_DECREF(py_value);
            Py_DECREF(dict);
            return nullptr;
        }
        Py_DECREF(py_key);
        Py_DECREF(py_value);
    }
    return dict;
}

// Convert std::unordered_map to Python dict
template<typename K, typename V, typename... Args>
PyObject* to_python(const std::unordered_map<K, V, Args...>& map) {
    PyObject* dict = PyDict_New();
    if (!dict) return nullptr;

    for (const auto& [key, value] : map) {
        PyObject* py_key = to_python(key);
        if (!py_key) {
            Py_DECREF(dict);
            return nullptr;
        }
        PyObject* py_value = to_python(value);
        if (!py_value) {
            Py_DECREF(py_key);
            Py_DECREF(dict);
            return nullptr;
        }
        if (PyDict_SetItem(dict, py_key, py_value) < 0) {
            Py_DECREF(py_key);
            Py_DECREF(py_value);
            Py_DECREF(dict);
            return nullptr;
        }
        Py_DECREF(py_key);
        Py_DECREF(py_value);
    }
    return dict;
}

// Convert Python dict to std::map
template<typename K, typename V, typename... Args>
bool from_python(PyObject* obj, std::map<K, V, Args...>& map) {
    if (!PyDict_Check(obj)) return false;

    map.clear();

    PyObject *py_key, *py_value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(obj, &pos, &py_key, &py_value)) {
        K cpp_key;
        V cpp_value;

        if (!from_python(py_key, cpp_key)) return false;
        if (!from_python(py_value, cpp_value)) return false;

        map[std::move(cpp_key)] = std::move(cpp_value);
    }
    return true;
}

// Convert Python dict to std::unordered_map
template<typename K, typename V, typename... Args>
bool from_python(PyObject* obj, std::unordered_map<K, V, Args...>& map) {
    if (!PyDict_Check(obj)) return false;

    map.clear();

    PyObject *py_key, *py_value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(obj, &pos, &py_key, &py_value)) {
        K cpp_key;
        V cpp_value;

        if (!from_python(py_key, cpp_key)) return false;
        if (!from_python(py_value, cpp_value)) return false;

        map[std::move(cpp_key)] = std::move(cpp_value);
    }
    return true;
}

// ============================================================================
// std::set / std::unordered_set Support - Set Containers
// ============================================================================

// Helper traits to detect set types
template<typename T>
struct is_std_set : std::false_type {};

template<typename V, typename... Args>
struct is_std_set<std::set<V, Args...>> : std::true_type {};

template<typename T>
struct is_std_unordered_set : std::false_type {};

template<typename V, typename... Args>
struct is_std_unordered_set<std::unordered_set<V, Args...>> : std::true_type {};

// Convert std::set to Python set
template<typename V, typename... Args>
PyObject* to_python(const std::set<V, Args...>& set) {
    PyObject* py_set = PySet_New(nullptr);
    if (!py_set) return nullptr;

    for (const auto& value : set) {
        PyObject* py_value = to_python(value);
        if (!py_value) {
            Py_DECREF(py_set);
            return nullptr;
        }
        if (PySet_Add(py_set, py_value) < 0) {
            Py_DECREF(py_value);
            Py_DECREF(py_set);
            return nullptr;
        }
        Py_DECREF(py_value);
    }
    return py_set;
}

// Convert std::unordered_set to Python set
template<typename V, typename... Args>
PyObject* to_python(const std::unordered_set<V, Args...>& set) {
    PyObject* py_set = PySet_New(nullptr);
    if (!py_set) return nullptr;

    for (const auto& value : set) {
        PyObject* py_value = to_python(value);
        if (!py_value) {
            Py_DECREF(py_set);
            return nullptr;
        }
        if (PySet_Add(py_set, py_value) < 0) {
            Py_DECREF(py_value);
            Py_DECREF(py_set);
            return nullptr;
        }
        Py_DECREF(py_value);
    }
    return py_set;
}

// Convert Python set to std::set
template<typename V, typename... Args>
bool from_python(PyObject* obj, std::set<V, Args...>& set) {
    if (!PySet_Check(obj) && !PyFrozenSet_Check(obj)) return false;

    set.clear();

    PyObject* iterator = PyObject_GetIter(obj);
    if (!iterator) return false;

    PyObject* item;
    while ((item = PyIter_Next(iterator))) {
        V cpp_value;
        if (!from_python(item, cpp_value)) {
            Py_DECREF(item);
            Py_DECREF(iterator);
            return false;
        }
        set.insert(std::move(cpp_value));
        Py_DECREF(item);
    }
    Py_DECREF(iterator);

    return !PyErr_Occurred();
}

// Convert Python set to std::unordered_set
template<typename V, typename... Args>
bool from_python(PyObject* obj, std::unordered_set<V, Args...>& set) {
    if (!PySet_Check(obj) && !PyFrozenSet_Check(obj)) return false;

    set.clear();

    PyObject* iterator = PyObject_GetIter(obj);
    if (!iterator) return false;

    PyObject* item;
    while ((item = PyIter_Next(iterator))) {
        V cpp_value;
        if (!from_python(item, cpp_value)) {
            Py_DECREF(item);
            Py_DECREF(iterator);
            return false;
        }
        set.insert(std::move(cpp_value));
        Py_DECREF(item);
    }
    Py_DECREF(iterator);

    return !PyErr_Occurred();
}

// ============================================================================
// std::tuple Support - Heterogeneous Fixed-Size Containers
// ============================================================================
//
// Enables automatic conversion between C++ std::tuple and Python tuple.
//
// Example C++ code:
//   std::tuple<int, std::string, double> get_info() {
//       return {42, "hello", 3.14};
//   }
//
// Python usage:
//   num, text, val = obj.get_info()
//   print(f"{text}: {num}, {val}")  # hello: 42, 3.14

// Helper to convert tuple elements to Python
template<typename Tuple, std::size_t... Is>
PyObject* tuple_to_python_impl(const Tuple& t, std::index_sequence<Is...>) {
    PyObject* py_tuple = PyTuple_New(sizeof...(Is));
    if (!py_tuple) return nullptr;

    bool success = true;
    (void)std::initializer_list<int>{(
        [&]() {
            if (!success) return;
            PyObject* elem = to_python(std::get<Is>(t));
            if (!elem) {
                success = false;
                return;
            }
            PyTuple_SET_ITEM(py_tuple, Is, elem);
        }(), 0
    )...};

    if (!success) {
        Py_DECREF(py_tuple);
        return nullptr;
    }
    return py_tuple;
}

// Convert std::tuple to Python tuple
template<typename... Ts>
PyObject* to_python(const std::tuple<Ts...>& t) {
    return tuple_to_python_impl(t, std::index_sequence_for<Ts...>{});
}

// Helper to convert Python tuple elements to C++ tuple
template<typename Tuple, std::size_t... Is>
bool tuple_from_python_impl(PyObject* obj, Tuple& t, std::index_sequence<Is...>) {
    if (!PyTuple_Check(obj) && !PyList_Check(obj)) return false;

    Py_ssize_t size = PyTuple_Check(obj) ? PyTuple_Size(obj) : PyList_Size(obj);
    if (static_cast<std::size_t>(size) != sizeof...(Is)) return false;

    bool success = true;
    (void)std::initializer_list<int>{(
        [&]() {
            if (!success) return;
            PyObject* elem = PyTuple_Check(obj) ?
                PyTuple_GetItem(obj, Is) : PyList_GetItem(obj, Is);
            if (!from_python(elem, std::get<Is>(t))) {
                success = false;
            }
        }(), 0
    )...};

    return success;
}

// Convert Python tuple/list to std::tuple
template<typename... Ts>
bool from_python(PyObject* obj, std::tuple<Ts...>& t) {
    return tuple_from_python_impl(obj, t, std::index_sequence_for<Ts...>{});
}

// ============================================================================
// std::pair Support - Two-Element Tuple
// ============================================================================

// Convert std::pair to Python tuple
template<typename T1, typename T2>
PyObject* to_python(const std::pair<T1, T2>& p) {
    PyObject* py_tuple = PyTuple_New(2);
    if (!py_tuple) return nullptr;

    PyObject* first = to_python(p.first);
    if (!first) {
        Py_DECREF(py_tuple);
        return nullptr;
    }
    PyTuple_SET_ITEM(py_tuple, 0, first);

    PyObject* second = to_python(p.second);
    if (!second) {
        Py_DECREF(py_tuple);
        return nullptr;
    }
    PyTuple_SET_ITEM(py_tuple, 1, second);

    return py_tuple;
}

// Convert Python tuple/list to std::pair
template<typename T1, typename T2>
bool from_python(PyObject* obj, std::pair<T1, T2>& p) {
    if (!PyTuple_Check(obj) && !PyList_Check(obj)) return false;

    Py_ssize_t size = PyTuple_Check(obj) ? PyTuple_Size(obj) : PyList_Size(obj);
    if (size != 2) return false;

    PyObject* first = PyTuple_Check(obj) ? PyTuple_GetItem(obj, 0) : PyList_GetItem(obj, 0);
    PyObject* second = PyTuple_Check(obj) ? PyTuple_GetItem(obj, 1) : PyList_GetItem(obj, 1);

    return from_python(first, p.first) && from_python(second, p.second);
}

// ============================================================================
// std::variant Support - Type-Safe Unions
// ============================================================================
//
// Enables automatic conversion between C++ std::variant and Python values.
// The variant is converted to/from the currently active alternative.
//
// Example C++ code:
//   using Value = std::variant<int, std::string, double>;
//   Value get_value() { return "hello"; }
//   void set_value(Value v) { ... }
//
// Python usage:
//   val = obj.get_value()  # Returns "hello" (string)
//   obj.set_value(42)      # Sets int alternative
//   obj.set_value(3.14)    # Sets double alternative

// Convert std::variant to Python (converts the active alternative)
template<typename... Ts>
PyObject* to_python(const std::variant<Ts...>& v) {
    return std::visit([](const auto& val) -> PyObject* {
        return to_python(val);
    }, v);
}

// Helper to try converting Python value to each variant alternative
template<typename Variant, typename T, typename... Rest>
bool try_variant_alternatives(PyObject* obj, Variant& v) {
    T value;
    if (from_python(obj, value)) {
        v = std::move(value);
        return true;
    }
    PyErr_Clear();  // Clear error from failed conversion

    if constexpr (sizeof...(Rest) > 0) {
        return try_variant_alternatives<Variant, Rest...>(obj, v);
    }
    return false;
}

// Convert Python to std::variant (tries each alternative in order)
template<typename... Ts>
bool from_python(PyObject* obj, std::variant<Ts...>& v) {
    if (try_variant_alternatives<std::variant<Ts...>, Ts...>(obj, v)) {
        return true;
    }
    PyErr_SetString(PyExc_TypeError, "Value does not match any variant alternative");
    return false;
}

// ============================================================================
// Custom Type Converter Extension Point
// ============================================================================
//
// Allows users to add conversion support for custom types (e.g., Eigen matrices,
// custom math types, domain-specific objects) by specializing the converter
// template or providing to_python/from_python overloads.
//
// METHOD 1: Function overloads (simpler)
// ----------------------------------------
// Simply define to_python/from_python functions for your type in the
// mirror_bridge namespace BEFORE including the binding header:
//
//   namespace mirror_bridge {
//       inline PyObject* to_python(const Eigen::Vector3d& v) {
//           PyObject* list = PyList_New(3);
//           PyList_SET_ITEM(list, 0, PyFloat_FromDouble(v.x()));
//           PyList_SET_ITEM(list, 1, PyFloat_FromDouble(v.y()));
//           PyList_SET_ITEM(list, 2, PyFloat_FromDouble(v.z()));
//           return list;
//       }
//
//       inline bool from_python(PyObject* obj, Eigen::Vector3d& v) {
//           if (!PyList_Check(obj) || PyList_Size(obj) != 3) return false;
//           v.x() = PyFloat_AsDouble(PyList_GetItem(obj, 0));
//           v.y() = PyFloat_AsDouble(PyList_GetItem(obj, 1));
//           v.z() = PyFloat_AsDouble(PyList_GetItem(obj, 2));
//           return !PyErr_Occurred();
//       }
//   }
//
// METHOD 2: Template specialization (more control)
// -------------------------------------------------
// Specialize the CustomTypeConverter template for complete control:
//
//   template<>
//   struct mirror_bridge::CustomTypeConverter<MyType> {
//       static PyObject* to_python(const MyType& value) { ... }
//       static bool from_python(PyObject* obj, MyType& value) { ... }
//   };

// Base template for custom type conversion - users can specialize this
template<typename T, typename Enable = void>
struct CustomTypeConverter {
    // Default: no custom conversion available
    static constexpr bool has_custom_conversion = false;
};

// Helper to detect if a custom converter exists
template<typename T>
concept HasCustomConverter = requires {
    { CustomTypeConverter<T>::has_custom_conversion } -> std::convertible_to<bool>;
} && CustomTypeConverter<T>::has_custom_conversion;

// Convert using custom converter if available
template<typename T>
    requires HasCustomConverter<T>
PyObject* to_python(const T& value) {
    return CustomTypeConverter<T>::to_python(value);
}

template<typename T>
    requires HasCustomConverter<T>
bool from_python(PyObject* obj, T& value) {
    return CustomTypeConverter<T>::from_python(obj, value);
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

// Trait to detect std::vector specifically (for bulk transfer optimization)
template<typename T>
struct is_std_vector : std::false_type {};
template<typename T, typename A>
struct is_std_vector<std::vector<T, A>> : std::true_type {};

// Convert C++ containers to Python lists (or use bulk transfer for numeric vectors)
//
// For std::vector of numeric types (int, float, double, etc.), uses a single
// memcpy into a Python array.array object instead of element-by-element
// conversion, giving ~10-50x speedup for large arrays.
//
// For other containers or non-numeric element types, falls back to per-element
// conversion via to_python(item).
template<Container T>
PyObject* to_python(const T& container) {
    using ValueType = typename std::remove_cvref_t<T>::value_type;

    // Bulk transfer fast path for numeric vectors.
    // Excluded: unsigned char (already handled as bytes above), bool (the
    // std::vector<bool> specialization has no .data() and is proxy-based).
    if constexpr (is_std_vector<std::remove_cvref_t<T>>::value &&
                  std::is_arithmetic_v<ValueType> &&
                  !std::is_same_v<ValueType, unsigned char> &&
                  !std::is_same_v<ValueType, bool>) {
        // Use Python's array module for typed numeric arrays.
        // This does a single memcpy instead of N Python object allocations.
        // Initialization is thread-safe via std::call_once (GIL must be held
        // by the caller, which is guaranteed in Python C API callbacks).
        static PyObject* array_module = nullptr;
        static PyObject* array_type = nullptr;
        static std::once_flag array_init_flag;
        std::call_once(array_init_flag, []() {
            array_module = PyImport_ImportModule("array");
            if (array_module) {
                array_type = PyObject_GetAttrString(array_module, "array");
            }
        });

        if (array_type) {
            // Map C++ type to Python array typecode
            const char* typecode = nullptr;
            if constexpr (std::is_same_v<ValueType, float>) typecode = "f";
            else if constexpr (std::is_same_v<ValueType, double>) typecode = "d";
            else if constexpr (std::is_same_v<ValueType, int32_t> || (std::is_same_v<ValueType, int> && sizeof(int) == 4)) typecode = "i";
            else if constexpr (std::is_same_v<ValueType, int64_t> || (std::is_same_v<ValueType, long long>)) typecode = "q";
            else if constexpr (std::is_same_v<ValueType, uint32_t> || (std::is_same_v<ValueType, unsigned int> && sizeof(unsigned int) == 4)) typecode = "I";
            else if constexpr (std::is_same_v<ValueType, uint64_t> || (std::is_same_v<ValueType, unsigned long long>)) typecode = "Q";
            else if constexpr (std::is_same_v<ValueType, int16_t> || std::is_same_v<ValueType, short>) typecode = "h";
            else if constexpr (std::is_same_v<ValueType, uint16_t> || std::is_same_v<ValueType, unsigned short>) typecode = "H";

            if (typecode) {
                PyObject* raw_bytes = PyBytes_FromStringAndSize(
                    reinterpret_cast<const char*>(container.data()),
                    container.size() * sizeof(ValueType));
                if (!raw_bytes) return nullptr;

                PyObject* typecode_obj = PyUnicode_FromString(typecode);
                if (!typecode_obj) {
                    Py_DECREF(raw_bytes);
                    return nullptr;
                }

                PyObject* args = PyTuple_Pack(2, typecode_obj, raw_bytes);
                Py_DECREF(typecode_obj);
                Py_DECREF(raw_bytes);
                if (!args) return nullptr;

                PyObject* result = PyObject_Call(array_type, args, nullptr);
                Py_DECREF(args);
                return result;
            }
        }
        // Fall through to element-by-element if array module unavailable
    }

    // General path: element-by-element conversion
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

// Convert Python sequences to C++ containers.
// Accepts list, tuple, array.array, and any type supporting the sequence protocol.
// This ensures round-trip compatibility: to_python returns array.array for numeric
// vectors, and from_python accepts it back.
template<Container T>
bool from_python(PyObject* obj, T& container) {
    if (!PySequence_Check(obj) || PyUnicode_Check(obj) || PyBytes_Check(obj)) {
        return false;
    }

    Py_ssize_t size = PySequence_Size(obj);
    if (size < 0) return false;

    using ValueType = typename std::remove_cvref_t<T>::value_type;

    if constexpr (requires { container.clear(); }) {
        container.clear();
    }
    if constexpr (requires { container.reserve(size); }) {
        container.reserve(size);
    }

    Py_ssize_t i = 0;
    for (; i < size && i < static_cast<Py_ssize_t>(container.size()); ++i) {
        PyObject* py_item = PySequence_GetItem(obj, i);  // New reference
        if (!py_item) return false;
        ValueType cpp_item;
        if (!from_python(py_item, cpp_item)) {
            Py_DECREF(py_item);
            return false;
        }
        Py_DECREF(py_item);

        if constexpr (requires { container[i]; }) {
            container[i] = std::move(cpp_item);
        } else if constexpr (requires { container.push_back(cpp_item); }) {
            container.push_back(std::move(cpp_item));
        } else if constexpr (requires { container.insert(cpp_item); }) {
            container.insert(std::move(cpp_item));
        } else {
            static_assert(requires { container.push_back(cpp_item); },
                         "Container must support indexing, push_back, or insert");
        }
    }

    if constexpr (requires { container.push_back(ValueType{}); }) {
        for (; i < size; ++i) {
            PyObject* py_item = PySequence_GetItem(obj, i);  // New reference
            if (!py_item) return false;
            ValueType cpp_item;
            if (!from_python(py_item, cpp_item)) {
                Py_DECREF(py_item);
                return false;
            }
            Py_DECREF(py_item);
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

// Cache for data member information — includes fields inherited from base
// classes. Walks T and its bases breadth-first; deeper classes contribute only
// fields with names not already seen (C++-style name hiding). Splicing an
// inherited field onto a derived object — `(derived).[:base_field:]` — compiles
// and reads/writes the field through the implicit base subobject.
//
// Same static-span caching as MemberFunctionCache: the BFS runs once, then
// per-index lookups are O(1).
template<typename T>
struct DataMemberCache {
    static consteval std::vector<std::meta::info> collect_fields() {
        constexpr auto ctx = std::meta::access_context::current();
        std::vector<std::meta::info> result;
        std::vector<std::string_view> seen_names;

        std::vector<std::meta::info> layer = { ^^T };
        while (!layer.empty()) {
            std::vector<std::meta::info> next_layer;
            std::vector<std::string_view> layer_names;

            for (auto cls : layer) {
                for (auto f : std::meta::nonstatic_data_members_of(cls, ctx)) {
                    auto name = std::meta::identifier_of(f);

                    bool hidden = false;
                    for (auto seen : seen_names) {
                        if (seen == name) { hidden = true; break; }
                    }
                    if (hidden) continue;

                    result.push_back(f);
                    layer_names.push_back(name);
                }
                for (auto b : std::meta::bases_of(cls, ctx)) {
                    next_layer.push_back(std::meta::type_of(b));
                }
            }

            for (auto n : layer_names) seen_names.push_back(n);
            layer = std::move(next_layer);
        }

        return result;
    }

    static constexpr auto fields = std::define_static_array(collect_fields());
    static constexpr std::size_t count = fields.size();

    static consteval auto get_at_index(std::size_t Index) {
        return fields[Index];
    }
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

// Cache for member function information — instantiated once per type T.
// Includes methods inherited from base classes via BFS over bases_of, with
// C++-style name hiding (derived's method hides base's method of same name).
// This matches pybind11's behavior and makes inherited virtuals / plain
// getters transparently usable from the target language.
//
// The BFS result is promoted to a constexpr static span via
// std::define_static_array, so per-index lookups are O(1) span access rather
// than re-running the full BFS. Without this cache, a class with N methods
// would trigger O(N³) constexpr work across MethodNameCache construction
// (44-method classes like PointCloud would hit the compiler step limit).
template<typename T>
struct MemberFunctionCache {
    static consteval bool is_bindable_method(std::meta::info member) {
        return std::meta::is_function(member) &&
               !std::meta::is_static_member(member) &&
               !std::meta::is_constructor(member) &&
               !std::meta::is_special_member_function(member) &&
               !std::meta::is_operator_function(member);
    }

    // Walk T and its bases breadth-first, returning every non-hidden bindable
    // method. Overloads at the same inheritance level all stay. A name that
    // appears at one level hides the same name from deeper bases.
    static consteval std::vector<std::meta::info> collect_methods() {
        constexpr auto ctx = std::meta::access_context::current();
        std::vector<std::meta::info> result;
        std::vector<std::string_view> seen_names;

        std::vector<std::meta::info> layer = { ^^T };
        while (!layer.empty()) {
            std::vector<std::meta::info> next_layer;
            std::vector<std::string_view> layer_names;

            for (auto cls : layer) {
                for (auto m : std::meta::members_of(cls, ctx)) {
                    if (!is_bindable_method(m)) continue;
                    auto name = std::meta::identifier_of(m);

                    bool hidden = false;
                    for (auto seen : seen_names) {
                        if (seen == name) { hidden = true; break; }
                    }
                    if (hidden) continue;

                    result.push_back(m);
                    layer_names.push_back(name);
                }
                for (auto b : std::meta::bases_of(cls, ctx)) {
                    next_layer.push_back(std::meta::type_of(b));
                }
            }

            for (auto n : layer_names) seen_names.push_back(n);
            layer = std::move(next_layer);
        }

        return result;
    }

    // Materialize the BFS result into a constexpr static span. Subsequent
    // per-index lookups hit this cached span instead of rerunning collect.
    static constexpr auto methods = std::define_static_array(collect_methods());
    static constexpr std::size_t count = methods.size();

    static consteval auto get_at_index(std::size_t Index) {
        return methods[Index];
    }
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

// Cache for static data members (including constexpr) - instantiated once per type T
template<typename T>
struct StaticDataMemberCache {
    // Helper to check if a member is a static data member (not a function)
    static consteval bool is_static_data_member(std::meta::info member) {
        return std::meta::is_variable(member) &&
               std::meta::is_static_member(member);
    }

    // Count static data members - computed once at compile time
    static consteval std::size_t compute_count() {
        auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
        std::size_t count = 0;
        for (auto member : all_members) {
            if (is_static_data_member(member)) {
                count++;
            }
        }
        return count;
    }

    // Get the Nth static data member - computed once per Index
    static consteval auto get_at_index(std::size_t Index) {
        auto all_members = std::meta::members_of(^^T, std::meta::access_context::current());
        std::size_t member_index = 0;
        for (auto member : all_members) {
            if (is_static_data_member(member)) {
                if (member_index == Index) {
                    return member;
                }
                member_index++;
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

// Helper templates for static data member reflection
template<typename T>
consteval std::size_t get_static_data_member_count() {
    return StaticDataMemberCache<T>::count;
}

// Get the Nth static data member by using cached lookup
template<typename T, std::size_t Index>
consteval auto get_static_data_member() {
    return StaticDataMemberCache<T>::get_at_index(Index);
}

template<typename T, std::size_t Index>
consteval const char* get_static_data_member_name() {
    constexpr auto member = get_static_data_member<T, Index>();
    return std::meta::identifier_of(member).data();
}

// Type of the Nth static data member
template<typename T, std::size_t Index>
using StaticDataMemberType = typename [:std::meta::type_of(get_static_data_member<T, Index>()):];

// Get the value of a static data member and convert to Python
template<typename T, std::size_t Index>
PyObject* get_static_data_member_value() {
    constexpr auto member = get_static_data_member<T, Index>();
    using MemberType = std::remove_cvref_t<StaticDataMemberType<T, Index>>;
    // Access the static member via splice
    const auto& value = [:member:];
    return to_python<MemberType>(value);
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

// Helper to conditionally forward arguments based on parameter type.
// For lvalue reference parameters (T&), pass as lvalue (no move).
// For value or rvalue reference parameters (T or T&&), use std::move.
// For pointer-storage (abstract/non-copyable types): dereference back to reference.
// This enables both ref parameters (like Body&), move-only types (unique_ptr),
// and abstract-class parameters (const Base&).
template<typename OriginalParamType, typename StoredType>
decltype(auto) forward_arg(StoredType& arg) {
    if constexpr (std::is_pointer_v<StoredType>) {
        // Pointer storage: dereference back to the underlying object.
        // This is used when the parameter type can't be held by value
        // (e.g., abstract class) and we stored a pointer into a Python
        // wrapper's cpp_object instead.
        return *arg;
    } else if constexpr (std::is_lvalue_reference_v<OriginalParamType>) {
        return arg;  // Pass as lvalue for T& parameters
    } else {
        return std::move(arg);  // Move for T and T&& parameters
    }
}

// from_python for pointer storage: extract cpp_object from Python wrapper.
// All PyWrapper<X> instances share a common layout (PyObject_HEAD, X*, bool),
// so we can read the cpp_object pointer generically and cast to the target
// pointer type. The cast is safe as long as the user passes a Python wrapper
// whose underlying C++ object is a T* or T-derived*.
template<typename T>
inline bool from_python_pointer(PyObject* obj, T*& out) {
    if (!obj || obj == Py_None) { out = nullptr; return obj == Py_None; }
    // Generic layout shared by every PyWrapper<X>
    struct GenericPyWrapper {
        PyObject_HEAD
        void* cpp_object;
        bool owns;
    };
    auto* wrapper = reinterpret_cast<GenericPyWrapper*>(obj);
    if (!wrapper->cpp_object) return false;
    out = static_cast<T*>(wrapper->cpp_object);
    return true;
}

// Dispatcher: value storage vs pointer storage.
// Called in method dispatch to populate the tuple of stored args.
template<typename ParamType, typename Storage>
inline bool extract_param(PyObject* obj, Storage& out) {
    if constexpr (std::is_pointer_v<Storage>) {
        return from_python_pointer(obj, out);
    } else {
        return from_python(obj, out);
    }
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

// ============================================================================
// Trampoline support — let Python subclasses override C++ virtual methods
// ============================================================================
//
// P2996R13 does NOT include code injection (P3294), so we can't synthesize an
// override for every virtual method purely from reflection. The solution has
// two halves:
//
//   1. A TrampolineBase<T> base class that stores the back-reference to the
//      Python self-object and provides the dispatch machinery (look up the
//      Python attribute, marshal args, invoke, marshal result).
//
//   2. A generated trampoline class (emitted by `mirror_bridge generate
//      --trampolines`) that inherits from both T and TrampolineBase<T>, and
//      overrides every virtual method of T — each override delegates to the
//      Python side when an override exists, and falls through to T::method()
//      otherwise. Non-virtual methods are unaffected.
//
// This matches pybind11's PYBIND11_OVERRIDE pattern in capability, but the
// trampoline class itself is fully auto-generated by our tool — the user
// doesn't write any glue.

// Distinguishing a Python override from the C++ binding:
//   A getattr on the instance returns either a Python bound method (when the
//   subclass shadows the name) or a PyCFunction object (our C++ binding).
//   PyMethod_Check is true only for the former.
inline bool has_python_override(PyObject* py_self, const char* name) {
    if (!py_self) return false;
    PyObject* method = PyObject_GetAttrString(py_self, name);
    if (!method) { PyErr_Clear(); return false; }
    bool is_py = PyMethod_Check(method) || PyFunction_Check(method);
    Py_DECREF(method);
    return is_py;
}

template<typename Base>
class TrampolineBase {
public:
    // Back-reference to the Python wrapper. Set by bind_class's tp_new after
    // allocating the trampoline instance.
    PyObject* py_self_ = nullptr;

    void _mirror_bridge_set_py_self(PyObject* self) noexcept { py_self_ = self; }

    // Invoke a Python-side override by name, marshalling args and result.
    // The trampoline's generated override body calls this when
    // has_python_override returns true.
    template<typename Ret, typename... Args>
    Ret dispatch_python(const char* name, Args&&... args) const {
        if (!py_self_) {
            throw std::runtime_error(std::string("TrampolineBase: no py_self for ") + name);
        }
        PyObject* method = PyObject_GetAttrString(py_self_, name);
        if (!method) {
            PyErr_Clear();
            throw std::runtime_error(std::string("No Python override for ") + name);
        }

        PyObject* py_args = PyTuple_New(sizeof...(Args));
        if constexpr (sizeof...(Args) > 0) {
            std::size_t i = 0;
            ((PyTuple_SET_ITEM(py_args, i++, to_python(std::forward<Args>(args)))), ...);
        }

        PyObject* result = PyObject_CallObject(method, py_args);
        Py_DECREF(py_args);
        Py_DECREF(method);

        if (!result) {
            // Python exception is already set; convert to C++ for the caller
            // stack. The outer method dispatcher will translate back to
            // Python if we re-enter its handler.
            PyErr_Print();
            throw std::runtime_error(std::string("Python override raised for ") + name);
        }

        if constexpr (std::is_void_v<Ret>) {
            Py_DECREF(result);
        } else {
            Ret r;
            if (!from_python(result, r)) {
                Py_DECREF(result);
                throw std::runtime_error(
                    std::string("Couldn't convert Python return value for ") + name);
            }
            Py_DECREF(result);
            return r;
        }
    }
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

// Check if all constructor params are bindable (value-bindable or pointer-holdable).
// Constructors whose params are raw-pointer-to-user-type aren't safely bindable
// (ambiguous ownership/output semantics) and are skipped.
template<typename T, std::size_t CtorIndex>
consteval bool constructor_params_all_bindable() {
    constexpr std::size_t param_count = get_constructor_param_count<T, CtorIndex>();
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (mirror_bridge::core::is_param_bindable<
            typename [:get_constructor_param_type<T, CtorIndex, Is>():]>() && ...);
    }(std::make_index_sequence<param_count>{});
}

// Variadic helper to call constructor with N parameters.
// Uses the same pointer-holder pattern as method dispatch: params that can't
// be held by value in std::tuple (abstract types, types with protected ctors)
// are stored as pointers extracted from the caller's Python wrapper.
template<typename T, std::size_t CtorIndex, std::size_t... Is>
T* call_constructor_impl(PyObject* args, std::index_sequence<Is...>) {
    // Abstract classes can't be constructed — no allocation possible.
    if constexpr (std::is_abstract_v<T>) {
        PyErr_SetString(PyExc_TypeError,
            "Cannot instantiate abstract class. Bind concrete derived classes instead.");
        return nullptr;
    } else {
        // Parameter storage with pointer-holder for non-value-bindable types
        // (e.g., abstract base classes passed by const-ref).
        std::tuple<mirror_bridge::core::param_storage_t<
            typename [:get_constructor_param_type<T, CtorIndex, Is>():]>...> cpp_args;

        bool success = true;
        ([&] {
            if (!success) return;
            if (!extract_param<typename [:get_constructor_param_type<T, CtorIndex, Is>():]>(
                    PyTuple_GET_ITEM(args, Is), std::get<Is>(cpp_args))) {
                success = false;
            }
        }(), ...);

        if (!success) {
            return nullptr;
        }

        // Call constructor with unpacked arguments. forward_arg dereferences
        // pointer-stored params back into references.
        return new T(forward_arg<typename [:get_constructor_param_type<T, CtorIndex, Is>():]>(
            std::get<Is>(cpp_args))...);
    }
}

// Trampoline-aware constructor caller. Allocates Alloc (== T normally, or
// Trampoline for classes that support Python subclassing) with T's matching
// constructor. Alloc must inherit from T and expose T's constructors (the
// canonical idiom is `using T::T;` in the trampoline).
template<typename T, typename Alloc, std::size_t CtorIndex, std::size_t... Is>
Alloc* call_constructor_impl_alloc(PyObject* args, std::index_sequence<Is...>) {
    if constexpr (std::is_abstract_v<T>) {
        PyErr_SetString(PyExc_TypeError,
            "Cannot instantiate abstract class. Bind concrete derived classes instead.");
        return nullptr;
    } else {
        std::tuple<mirror_bridge::core::param_storage_t<
            typename [:get_constructor_param_type<T, CtorIndex, Is>():]>...> cpp_args;

        bool success = true;
        ([&] {
            if (!success) return;
            if (!extract_param<typename [:get_constructor_param_type<T, CtorIndex, Is>():]>(
                    PyTuple_GET_ITEM(args, Is), std::get<Is>(cpp_args))) {
                success = false;
            }
        }(), ...);

        if (!success) return nullptr;

        return new Alloc(forward_arg<typename [:get_constructor_param_type<T, CtorIndex, Is>():]>(
            std::get<Is>(cpp_args))...);
    }
}

// Initialize Python wrapper with parameterized constructor. If Trampoline != T,
// allocates a Trampoline (which must derive from T) and sets its py_self back-
// reference so virtual overrides can dispatch to Python.
template<typename T, typename Trampoline = T>
int py_init(PyObject* self, PyObject* args, PyObject* kwds) {
    auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);

    Py_ssize_t nargs = PyTuple_Size(args);

    // Default constructor case — only if Trampoline is default constructible.
    if (nargs == 0) {
        if constexpr (std::is_default_constructible_v<Trampoline>) {
            auto* obj = new Trampoline();
            wrapper->cpp_object = obj;
            wrapper->owns = true;
            if constexpr (!std::is_same_v<T, Trampoline>) {
                static_cast<Trampoline*>(obj)->_mirror_bridge_set_py_self(self);
            }
            return 0;
        } else {
            PyErr_SetString(PyExc_TypeError, "This class requires constructor arguments");
            return -1;
        }
    }

    // Try to find matching constructor by parameter count.
    constexpr std::size_t ctor_count = get_constructor_count<T>();

    bool found = false;
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            if (found) return;

            if constexpr (constructor_params_all_bindable<T, Is>()) {
                constexpr std::size_t param_count = get_constructor_param_count<T, Is>();
                if (nargs == static_cast<Py_ssize_t>(param_count)) {
                    Trampoline* obj = call_constructor_impl_alloc<T, Trampoline, Is>(
                        args, std::make_index_sequence<param_count>{});

                    if (obj) {
                        wrapper->cpp_object = obj;
                        wrapper->owns = true;
                        if constexpr (!std::is_same_v<T, Trampoline>) {
                            obj->_mirror_bridge_set_py_self(self);
                        }
                        found = true;
                    }
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

// Fallback constructor for classes without parameterized constructors.
// Only works for default-constructible types. Trampoline-aware: if Trampoline
// differs from T, allocates Trampoline and wires py_self so virtual overrides
// can dispatch to Python.
template<typename T, typename Trampoline = T>
PyObject* py_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    auto* self = reinterpret_cast<PyWrapper<T>*>(type->tp_alloc(type, 0));
    if (self) {
        if constexpr (std::is_default_constructible_v<Trampoline>) {
            auto* obj = new Trampoline();
            self->cpp_object = obj;
            self->owns = true;
            if constexpr (!std::is_same_v<T, Trampoline>) {
                static_cast<Trampoline*>(obj)->_mirror_bridge_set_py_self(
                    reinterpret_cast<PyObject*>(self));
            }
        } else {
            // Not default constructible — caller should have used tp_init path.
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

    if constexpr (std::is_array_v<MemberType>) {
        // C-style arrays can't be whole-assigned. Fill in-place.
        if (!from_python(value, (*wrapper->cpp_object).[:member:])) {
            PyErr_SetString(PyExc_TypeError, "Type conversion failed");
            return -1;
        }
    } else {
        MemberType cpp_value;
        if (!from_python(value, cpp_value)) {
            PyErr_SetString(PyExc_TypeError, "Type conversion failed");
            return -1;
        }

        // Use move semantics for move-only types (like unique_ptr)
        (*wrapper->cpp_object).[:member:] = std::move(cpp_value);
    }
    return 0;
}

// ============================================================================
// Visible Member Getter/Setter (P3394 Annotation-Aware)
// ============================================================================
//
// These versions work with visible member indices (excluding fields marked with
// [[=exclude{}]] annotation). Used by generate_visible_getsetters.

// Get visible member info - returns the Nth non-excluded member
template<typename T, std::size_t VisibleIndex>
consteval auto get_visible_member_info() {
    return annotations::get_visible_member<T>(VisibleIndex);
}

// Getter for visible class members (same logic as py_getter but uses visible index)
template<typename T, std::size_t VisibleIndex>
PyObject* py_visible_getter(PyObject* self, void* closure) {
    auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);
    if (!wrapper->cpp_object) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid C++ object");
        return nullptr;
    }

    constexpr auto member = get_visible_member_info<T, VisibleIndex>();
    using MemberType = typename [:std::meta::type_of(member):];

    auto& value = (*wrapper->cpp_object).[:member:];
    return to_python(value);
}

// Setter for visible class members (same logic as py_setter but uses visible index).
// C-style arrays require in-place population — they can't be assigned as whole
// objects in C++, so we dispatch to a from_python overload that writes element-wise.
template<typename T, std::size_t VisibleIndex>
int py_visible_setter(PyObject* self, PyObject* value, void* closure) {
    auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);
    if (!wrapper->cpp_object) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid C++ object");
        return -1;
    }

    if (!value) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete attribute");
        return -1;
    }

    constexpr auto member = get_visible_member_info<T, VisibleIndex>();
    using MemberType = typename [:std::meta::type_of(member):];

    if constexpr (std::is_array_v<MemberType>) {
        // Array: fill in-place. from_python<T[N]> writes directly into the array.
        if (!from_python(value, (*wrapper->cpp_object).[:member:])) {
            PyErr_SetString(PyExc_TypeError, "Type conversion failed");
            return -1;
        }
    } else {
        MemberType cpp_value;
        if (!from_python(value, cpp_value)) {
            PyErr_SetString(PyExc_TypeError, "Type conversion failed");
            return -1;
        }
        (*wrapper->cpp_object).[:member:] = std::move(cpp_value);
    }
    return 0;
}

// Readonly setter - always fails (used for readonly annotated members)
template<typename T, std::size_t VisibleIndex>
int py_readonly_setter(PyObject* self, PyObject* value, void* closure) {
    PyErr_SetString(PyExc_AttributeError, "Attribute is read-only");
    return -1;
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
    using ReturnType = typename [:return_type:];

    // Parameter storage: for value-bindable params, store by value;
    // for abstract/non-copyable params, store as pointer into the Python
    // wrapper's cpp_object (the object lives in the wrapper's heap memory,
    // we just hold a pointer to it). This is mirror_bridge's equivalent
    // of pybind11's shared_ptr holder, but inline and lightweight.
    std::tuple<mirror_bridge::core::param_storage_t<typename [:get_method_param_type<T, FuncIndex, Is>():]>...> cpp_args;

    bool success = true;
    ([&] {
        if (!success) return;
        if (!extract_param<typename [:get_method_param_type<T, FuncIndex, Is>():]>(
                PyTuple_GET_ITEM(args, Is), std::get<Is>(cpp_args))) {
            PyErr_Format(PyExc_TypeError, "Argument %zu type conversion failed", Is);
            success = false;
        }
    }(), ...);

    if (!success) {
        return nullptr;
    }

    // Call: forward_arg now handles pointer storage by dereferencing.
    if constexpr (std::is_void_v<ReturnType>) {
        ((*wrapper->cpp_object).[:member_func:])(
            forward_arg<typename [:get_method_param_type<T, FuncIndex, Is>():]>(std::get<Is>(cpp_args))...
        );
        Py_RETURN_NONE;
    } else {
        ReturnType result = ((*wrapper->cpp_object).[:member_func:])(
            forward_arg<typename [:get_method_param_type<T, FuncIndex, Is>():]>(std::get<Is>(cpp_args))...
        );
        return to_python(result);
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
        set_py_error_from_cpp_exception(e);
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

    // Parameter storage with pointer-holder for abstract/non-copyable types
    std::tuple<mirror_bridge::core::param_storage_t<typename [:get_static_method_param_type<T, Index, Is>():]>...> cpp_args;

    bool success = true;
    ([&] {
        if (!success) return;
        if (!extract_param<typename [:get_static_method_param_type<T, Index, Is>():]>(
                PyTuple_GET_ITEM(args, Is), std::get<Is>(cpp_args))) {
            PyErr_Format(PyExc_TypeError, "Argument %zu type conversion failed", Is);
            success = false;
        }
    }(), ...);

    if (!success) {
        return nullptr;
    }

    // Call the static C++ method using reflection splicer
    // Use forward_arg to conditionally move: move for value/rvalue params, no move for lvalue ref params
    if constexpr (std::is_void_v<ReturnType>) {
        [:static_func:](
            forward_arg<typename [:get_static_method_param_type<T, Index, Is>():]>(std::get<Is>(cpp_args))...
        );
        Py_RETURN_NONE;
    } else {
        ReturnType result = [:static_func:](
            forward_arg<typename [:get_static_method_param_type<T, Index, Is>():]>(std::get<Is>(cpp_args))...
        );
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
        set_py_error_from_cpp_exception(e);
        return nullptr;
    } catch (...) {
        PyErr_SetString(PyExc_RuntimeError, "Unknown C++ exception");
        return nullptr;
    }
}

// ============================================================================
// Method Overloading Dispatch (pybind11/nanobind style)
// ============================================================================
//
// Instead of name-mangling overloaded methods, we use a dispatch system:
// 1. Each unique method name gets ONE Python method entry
// 2. That method dispatches to the correct C++ overload based on argument types
// 3. Uses two-pass resolution: strict type match first, then with conversion
//
// This provides a Pythonic API: obj.add(1) instead of obj.add_int(1)

// Sentinel value indicating "try next overload"
// Using a value that can't be a valid PyObject* (odd address)
inline PyObject* const OVERLOAD_TRY_NEXT = reinterpret_cast<PyObject*>(1);

// Cache of all method name string_views for a class, computed once.
// Without this cache, is_canonical_method<T, Index> calls
// get_member_function_name<T, Earlier> for every earlier index, and each
// such call re-runs std::meta::members_of() — producing O(N³) reflection
// work for a class with N methods. Open3D's PointCloud (44 methods) hits
// the constexpr step limit under the naive approach.
//
// With the cache, names are computed once (O(N²) via members_of per index),
// and subsequent canonicality checks are O(N) string_view comparisons.
template<typename T>
struct MethodNameCache {
    static constexpr std::size_t count = get_member_function_count<T>();

    static consteval auto compute_names() {
        std::array<std::string_view, (count == 0 ? 1 : count)> result{};
        if constexpr (count > 0) {
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((result[Is] = std::string_view(get_member_function_name<T, Is>())), ...);
            }(std::make_index_sequence<count>{});
        }
        return result;
    }

    static constexpr auto names = compute_names();
};

// Check if method at Index is the first BINDABLE method with its name.
// Methods whose params can't be stored either by value or via pointer-holder
// (e.g., raw pointer to a user container — ambiguous ownership) are excluded,
// but if another overload of the same name IS bindable, we still expose the
// method. Only the unbindable overloads are skipped. Uses MethodNameCache<T>
// to avoid redundant reflection work.
template<typename T, std::size_t Index>
consteval bool is_canonical_method() {
    if constexpr (!mirror_bridge::core::method_params_all_bindable<T, Index>()) {
        return false;
    } else {
        constexpr auto& names = MethodNameCache<T>::names;
        return []<std::size_t... Earlier>(std::index_sequence<Earlier...>) {
            // Canonical iff no EARLIER bindable method shares this name.
            // An earlier unbindable overload doesn't disqualify us.
            return (... && (names[Earlier] != names[Index] ||
                            !mirror_bridge::core::method_params_all_bindable<T, Earlier>()));
        }(std::make_index_sequence<Index>{});
    }
}

// Count unique method names (number of canonical methods)
template<typename T>
consteval std::size_t count_unique_method_names() {
    constexpr std::size_t total = get_member_function_count<T>();
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return ((is_canonical_method<T, Is>() ? 1 : 0) + ... + 0);
    }(std::make_index_sequence<total>{});
}

// Try to call a method overload. Returns:
// - Valid PyObject* on success
// - nullptr with Python error set on C++ exception
// - OVERLOAD_TRY_NEXT if argument count/types don't match (no error set)
template<typename T, std::size_t FuncIndex, std::size_t... Is>
PyObject* try_overload_impl(PyWrapper<T>* wrapper, PyObject* args, std::index_sequence<Is...>) {
    constexpr auto member_func = get_member_function<T, FuncIndex>();
    constexpr auto return_type = get_method_return_type<T, FuncIndex>();
    using ReturnType = typename [:return_type:];

    // Parameter storage with pointer-holder for abstract/non-copyable types
    std::tuple<mirror_bridge::core::param_storage_t<typename [:get_method_param_type<T, FuncIndex, Is>():]>...> cpp_args;

    bool conversion_ok = true;
    ([&] {
        if (!conversion_ok) return;
        PyObject* py_arg = PyTuple_GET_ITEM(args, Is);
        if (!extract_param<typename [:get_method_param_type<T, FuncIndex, Is>():]>(
                py_arg, std::get<Is>(cpp_args))) {
            conversion_ok = false;
        }
    }(), ...);

    if (!conversion_ok) {
        // Clear any error that from_python may have set
        PyErr_Clear();
        return OVERLOAD_TRY_NEXT;
    }

    // Conversion succeeded - call the method
    try {
        if constexpr (std::is_void_v<ReturnType>) {
            ((*wrapper->cpp_object).[:member_func:])(
                forward_arg<typename [:get_method_param_type<T, FuncIndex, Is>():]>(std::get<Is>(cpp_args))...
            );
            Py_RETURN_NONE;
        } else {
            ReturnType result = ((*wrapper->cpp_object).[:member_func:])(
                forward_arg<typename [:get_method_param_type<T, FuncIndex, Is>():]>(std::get<Is>(cpp_args))...
            );
            return to_python(result);
        }
    } catch (const std::exception& e) {
        set_py_error_from_cpp_exception(e);
        return nullptr;
    } catch (...) {
        PyErr_SetString(PyExc_RuntimeError, "Unknown C++ exception");
        return nullptr;
    }
}

// Reflection-driven parameter metadata helpers. These let the dispatcher
// support keyword arguments and default values without hand-written binding
// code.
//
// - param_name: identifier of the Ith parameter (empty string_view if the
//   parameter was declared unnamed in the header).
// - param_has_default: true if the Ith parameter has a C++ default-argument.
// - min_required_args: index of the first defaulted parameter — i.e. the
//   smallest positional-arg count that can legally be passed from Python.
template<typename T, std::size_t FuncIndex, std::size_t ParamIndex>
consteval std::string_view param_name() {
    constexpr auto func = get_member_function<T, FuncIndex>();
    auto p = std::meta::parameters_of(func)[ParamIndex];
    if (std::meta::has_identifier(p)) return std::meta::identifier_of(p);
    return {};
}

template<typename T, std::size_t FuncIndex, std::size_t ParamIndex>
consteval bool param_has_default() {
    constexpr auto func = get_member_function<T, FuncIndex>();
    return std::meta::has_default_argument(std::meta::parameters_of(func)[ParamIndex]);
}

template<typename T, std::size_t FuncIndex>
consteval std::size_t min_required_args() {
    constexpr std::size_t P = get_method_param_count<T, FuncIndex>();
    // Smallest i such that param i has a default. All params AFTER the first
    // defaulted one also have defaults in well-formed C++.
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> std::size_t {
        std::size_t result = P;
        (((result == P && param_has_default<T, FuncIndex, Is>())
            ? (result = Is) : 0), ...);
        return result;
    }(std::make_index_sequence<P>{});
}

// Invoke the method with the first N of the resolved args; params [N, P) use
// their C++ default arguments (filled in by the compiler at the splice call).
//
// Implementation note: we inline all reflection splicers inside the lambda
// body rather than using `constexpr auto member_func = …` at this scope,
// because std::meta::info is a consteval-only type that can't be captured
// through a runtime-callable lambda. Expanding [:get_member_function...:]
// inside the lambda keeps every reflection operation in a constant-evaluated
// context.
template<typename T, std::size_t FuncIndex, std::size_t N>
PyObject* invoke_with_n_args(PyWrapper<T>* wrapper, PyObject** resolved) {
    using ReturnType = typename [:get_method_return_type<T, FuncIndex>():];

    return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> PyObject* {
        std::tuple<mirror_bridge::core::param_storage_t<
            typename [:get_method_param_type<T, FuncIndex, Is>():]>...> cpp_args;

        bool conversion_ok = true;
        ([&] {
            if (!conversion_ok) return;
            if (!extract_param<typename [:get_method_param_type<T, FuncIndex, Is>():]>(
                    resolved[Is], std::get<Is>(cpp_args))) {
                conversion_ok = false;
            }
        }(), ...);

        if (!conversion_ok) {
            PyErr_Clear();
            return OVERLOAD_TRY_NEXT;
        }

        try {
            if constexpr (std::is_void_v<ReturnType>) {
                ((*wrapper->cpp_object).[:get_member_function<T, FuncIndex>():])(
                    forward_arg<typename [:get_method_param_type<T, FuncIndex, Is>():]>(std::get<Is>(cpp_args))...
                );
                Py_RETURN_NONE;
            } else {
                ReturnType result = ((*wrapper->cpp_object).[:get_member_function<T, FuncIndex>():])(
                    forward_arg<typename [:get_method_param_type<T, FuncIndex, Is>():]>(std::get<Is>(cpp_args))...
                );
                return to_python(result);
            }
        } catch (const std::exception& e) {
            set_py_error_from_cpp_exception(e);
            return nullptr;
        } catch (...) {
            PyErr_SetString(PyExc_RuntimeError, "Unknown C++ exception");
            return nullptr;
        }
    }(std::make_index_sequence<N>{});
}

// Keyword-aware overload attempt. Accepts positional args + kwds, resolves
// each into the correct param slot using reflection-provided names, and
// invokes the method with the resolved leading prefix (remaining params
// fill in from C++ defaults).
template<typename T, std::size_t FuncIndex>
PyObject* try_overload(PyWrapper<T>* wrapper, PyObject* args, PyObject* kwds) {
    constexpr std::size_t P = get_method_param_count<T, FuncIndex>();
    constexpr std::size_t MIN = min_required_args<T, FuncIndex>();

    const Py_ssize_t n_pos = args ? PyTuple_Size(args) : 0;
    const Py_ssize_t n_kw = kwds ? PyDict_Size(kwds) : 0;

    if (n_pos > static_cast<Py_ssize_t>(P)) return OVERLOAD_TRY_NEXT;
    if (n_pos + n_kw > static_cast<Py_ssize_t>(P)) return OVERLOAD_TRY_NEXT;
    if (n_pos + n_kw < static_cast<Py_ssize_t>(MIN)) return OVERLOAD_TRY_NEXT;

    // Resolve positional + keyword args into a contiguous array of PyObject*.
    // resolved[i] holds the arg for the i-th parameter slot.
    PyObject* resolved[P > 0 ? P : 1] = {};
    for (Py_ssize_t i = 0; i < n_pos; ++i) {
        resolved[i] = PyTuple_GET_ITEM(args, i);
    }

    // Match each kwarg by name to a parameter index. We check every kwarg
    // against every param name; parameter names come from reflection via
    // identifier_of, so this is O(kw * P) per call (small for typical APIs).
    if (kwds && n_kw > 0) {
        PyObject* key;
        PyObject* value;
        Py_ssize_t pos = 0;
        while (PyDict_Next(kwds, &pos, &key, &value)) {
            Py_ssize_t key_len;
            const char* key_str = PyUnicode_AsUTF8AndSize(key, &key_len);
            if (!key_str) return OVERLOAD_TRY_NEXT;
            std::string_view key_view(key_str, key_len);

            bool matched = false;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((!matched && param_name<T, FuncIndex, Is>() == key_view
                    ? (resolved[Is] && true  // already filled by positional — conflict
                        ? (matched = true, void())  // mark matched to avoid error loop
                        : (resolved[Is] = value, matched = true, void()))
                    : void()), ...);
            }(std::make_index_sequence<P>{});
            if (!matched) return OVERLOAD_TRY_NEXT;  // unknown keyword
        }
    }

    // Find N: the number of leading resolved slots that are populated. All
    // remaining slots must be defaulted params (we already verified counts).
    std::size_t n_used = static_cast<std::size_t>(n_pos + n_kw);
    // If a kwarg filled a slot past the first gap, the caller provided a
    // non-contiguous prefix — reject (can't skip a required param).
    for (std::size_t i = 0; i < n_used; ++i) {
        if (!resolved[i]) return OVERLOAD_TRY_NEXT;
    }

    // Dispatch to the compile-time variant matching n_used. The index
    // sequence covers {MIN, MIN+1, ..., P}.
    PyObject* result = OVERLOAD_TRY_NEXT;
    [&]<std::size_t... Ns>(std::index_sequence<Ns...>) {
        ((n_used == Ns + MIN
            ? (result = invoke_with_n_args<T, FuncIndex, Ns + MIN>(wrapper, resolved), void())
            : void()), ...);
    }(std::make_index_sequence<P - MIN + 1>{});

    return result;
}

// Dispatch function that tries all overloads with a given name.
// CanonicalIndex is the first method with this name; we try ALL methods with
// matching name. Keyword arguments are supported via each overload's
// try_overload<...>, which uses reflection to map kwarg names -> param slots.
template<typename T, std::size_t CanonicalIndex, std::size_t... AllIndices>
PyObject* py_method_dispatch_impl(PyObject* self, PyObject* args, PyObject* kwds,
                                   std::index_sequence<AllIndices...>) {
    auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);
    if (!wrapper->cpp_object) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid C++ object");
        return nullptr;
    }

    constexpr auto target_name = get_member_function_name<T, CanonicalIndex>();
    PyObject* result = OVERLOAD_TRY_NEXT;

    ([&] {
        if (result != OVERLOAD_TRY_NEXT) return;

        constexpr auto this_name = get_member_function_name<T, AllIndices>();
        if constexpr (std::string_view(this_name) == std::string_view(target_name) &&
                      mirror_bridge::core::method_params_all_bindable<T, AllIndices>()) {
            result = try_overload<T, AllIndices>(wrapper, args, kwds);
        }
    }(), ...);

    if (result == OVERLOAD_TRY_NEXT) {
        PyErr_Format(PyExc_TypeError,
            "No matching overload for '%s' with %zd positional argument(s)%s",
            target_name,
            args ? PyTuple_Size(args) : 0,
            (kwds && PyDict_Size(kwds) > 0) ? " and keyword argument(s)" : "");
        return nullptr;
    }

    return result;
}

// Entry point registered in the PyMethodDef table. METH_VARARGS | METH_KEYWORDS.
template<typename T, std::size_t CanonicalIndex>
PyObject* py_method_dispatch(PyObject* self, PyObject* args, PyObject* kwds) {
    constexpr std::size_t total_methods = get_member_function_count<T>();
    return py_method_dispatch_impl<T, CanonicalIndex>(
        self, args, kwds, std::make_index_sequence<total_methods>{});
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

// Generate Python getters/setters for visible members only (P3394 annotation-aware)
// Skips members marked with [[=exclude{}]] and uses readonly setter for [[=readonly{}]]
template<typename T, std::size_t... VisibleIndices>
consteval auto generate_visible_getsetters(std::index_sequence<VisibleIndices...>) {
    constexpr std::size_t count = sizeof...(VisibleIndices);

    std::array<PyGetSetDef, count + 1> getsets{};

    // Populate entries for visible members
    ((getsets[VisibleIndices] = PyGetSetDef{
        .name = annotations::get_visible_member_name<T, VisibleIndices>(),
        .get = py_visible_getter<T, VisibleIndices>,
        // Use readonly setter if member has [[=readonly{}]] annotation
        .set = annotations::is_visible_member_readonly<T, VisibleIndices>()
            ? py_readonly_setter<T, VisibleIndices>
            : py_visible_setter<T, VisibleIndices>,
        .doc = nullptr,
        .closure = nullptr
    }), ...);

    // Sentinel entry
    getsets[count] = PyGetSetDef{nullptr, nullptr, nullptr, nullptr, nullptr};

    return getsets;
}

// Generate Python methods for all member functions of a class
// Uses dispatch system for overloading - one entry per unique name
template<typename T, std::size_t... Indices>
auto generate_methods(std::index_sequence<Indices...>) {
    // Count unique method names (canonical methods only)
    constexpr std::size_t unique_count = count_unique_method_names<T>();

    // Build methods array - one entry per unique name + sentinel
    static std::array<PyMethodDef, unique_count + 1> methods{};
    static bool initialized = false;

    if (!initialized) {
        std::size_t method_idx = 0;

        // Only add entries for canonical methods (first occurrence of each name)
        ([&] {
            if constexpr (is_canonical_method<T, Indices>()) {
                constexpr auto method_name = get_member_function_name<T, Indices>();
                methods[method_idx] = PyMethodDef{
                    .ml_name = method_name,
                    .ml_meth = reinterpret_cast<PyCFunction>(py_method_dispatch<T, Indices>),
                    // METH_KEYWORDS lets the dispatcher see the kwds dict so it
                    // can resolve keyword arguments by reflection-provided name.
                    .ml_flags = METH_VARARGS | METH_KEYWORDS,
                    .ml_doc = nullptr
                };
                method_idx++;
            }
        }(), ...);

        // Sentinel entry
        methods[unique_count] = PyMethodDef{nullptr, nullptr, 0, nullptr};
        initialized = true;
    }

    return methods;
}

// Generate Python static methods for all static member functions of a class.
// Static methods with params that can neither be stored by value nor held as
// pointers (e.g., raw pointer to user container — ambiguous ownership) are
// skipped. Their PyMethodDef entry is left zero-initialized so Python never
// sees them, but the rest of the class still binds.
template<typename T, std::size_t... Indices>
auto generate_static_methods(std::index_sequence<Indices...>) {
    constexpr std::size_t count = sizeof...(Indices);

    // Build methods array
    std::array<PyMethodDef, count + 1> methods{};

    // Populate entries only for methods whose params are all bindable.
    ([&] {
        if constexpr (mirror_bridge::core::static_method_params_all_bindable<T, Indices>()) {
            constexpr auto func_name = get_static_member_function_name<T, Indices>();
            methods[Indices] = PyMethodDef{
                .ml_name = func_name,
                .ml_meth = reinterpret_cast<PyCFunction>(py_static_method<T, Indices>),
                .ml_flags = METH_VARARGS,  // No METH_STATIC - we wrap with PyStaticMethod_New instead
                .ml_doc = nullptr
            };
        }
    }(), ...);

    // Sentinel entry
    methods[count] = PyMethodDef{nullptr, nullptr, 0, nullptr};

    return methods;
}

// ============================================================================
// Operator Overloading — Reflection-Driven Python Slot Binding
// ============================================================================
//
// P2996 exposes operators via `is_operator_function(info)` + `operator_of(info)`,
// which together let us detect every overloaded operator and dispatch each to
// the right Python slot. No per-class glue required — if the user declares
// `operator+`, `operator==`, `operator[]` etc. on their C++ type, the Python
// binding automatically gains `__add__`, comparison, subscript, etc.
//
// Python slots come in two flavours: number/mapping/sequence sub-tables
// (tp_as_number, tp_as_mapping, tp_as_sequence) and direct PyTypeObject
// fields (tp_richcompare, tp_call). We populate the right one per operator.

namespace ops {

// Classification of a reflected operator into the Python slot it maps to.
enum class Slot {
    Unknown,
    Add, Sub, Mul, Div, Mod,                     // nb_*
    IAdd, ISub, IMul, IDiv, IMod,                 // nb_inplace_*
    Eq, Ne, Lt, Gt, Le, Ge,                      // tp_richcompare
    Subscript,                                   // mp_subscript
    Call,                                        // tp_call
    Negative, Positive,                          // unary +, -
};

consteval Slot classify(std::meta::operators op, bool is_unary) {
    using O = std::meta::operators;
    switch (op) {
        case O::op_plus:           return is_unary ? Slot::Positive : Slot::Add;
        case O::op_minus:          return is_unary ? Slot::Negative : Slot::Sub;
        case O::op_star:           return Slot::Mul;
        case O::op_slash:          return Slot::Div;
        case O::op_percent:        return Slot::Mod;
        case O::op_plus_equals:    return Slot::IAdd;
        case O::op_minus_equals:   return Slot::ISub;
        case O::op_star_equals:    return Slot::IMul;
        case O::op_slash_equals:   return Slot::IDiv;
        case O::op_percent_equals: return Slot::IMod;
        case O::op_equals_equals:  return Slot::Eq;
        case O::op_exclamation_equals: return Slot::Ne;
        case O::op_less:           return Slot::Lt;
        case O::op_greater:        return Slot::Gt;
        case O::op_less_equals:    return Slot::Le;
        case O::op_greater_equals: return Slot::Ge;
        case O::op_square_brackets: return Slot::Subscript;
        case O::op_parentheses:    return Slot::Call;
        default:                   return Slot::Unknown;
    }
}

// Enumerate operator methods declared on T (not its bases — operators are
// rarely inherited meaningfully in user code; keeping it local avoids
// surprising polymorphic-operator resolution). Returns infos for non-static
// member operators only.
template<typename T>
consteval std::vector<std::meta::info> collect_operators() {
    auto ctx = std::meta::access_context::current();
    std::vector<std::meta::info> out;
    for (auto m : std::meta::members_of(^^T, ctx)) {
        if (!std::meta::is_function(m)) continue;
        if (std::meta::is_static_member(m)) continue;
        if (!std::meta::is_operator_function(m)) continue;
        // Skip ones we don't map
        auto params = std::meta::parameters_of(m);
        bool is_unary = params.empty();
        if (classify(std::meta::operator_of(m), is_unary) == Slot::Unknown) continue;
        out.push_back(m);
    }
    return out;
}

template<typename T>
struct OperatorCache {
    static constexpr auto operators = std::define_static_array(collect_operators<T>());
    static constexpr std::size_t count = operators.size();

    static consteval std::meta::info at(std::size_t i) { return operators[i]; }

    static consteval Slot slot_at(std::size_t i) {
        auto m = operators[i];
        auto params = std::meta::parameters_of(m);
        return classify(std::meta::operator_of(m), params.empty());
    }
};

// Find the first operator in T's cache matching a given Slot, or size() if none.
template<typename T>
consteval std::size_t find_op(Slot target) {
    for (std::size_t i = 0; i < OperatorCache<T>::count; ++i) {
        if (OperatorCache<T>::slot_at(i) == target) return i;
    }
    return OperatorCache<T>::count;
}

// Helpers: the parameter count / type of T's OpIndex-th operator. We can't
// hold an std::meta::info-typed constexpr variable (it's consteval-only), so
// wrap each lookup in a fresh consteval function that the splicer consumes.
template<typename T, std::size_t OpIndex>
consteval std::size_t op_param_count() {
    return std::meta::parameters_of(OperatorCache<T>::at(OpIndex)).size();
}

// Wrapper for a binary operator: `(a OP b) -> result`. When `InPlace` is true,
// the semantics are compound-assignment: the wrapper returns `self` with the
// wrapper object incref'd to match Python's in-place protocol.
template<typename T, std::size_t OpIndex, bool InPlace>
PyObject* binary_op_wrapper(PyObject* self, PyObject* other) {
    auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);
    if (!wrapper->cpp_object) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid C++ object");
        return nullptr;
    }

    if constexpr (op_param_count<T, OpIndex>() != 1) {
        Py_RETURN_NOTIMPLEMENTED;
    } else {
        using OtherParam = typename [:std::meta::type_of(
            std::meta::parameters_of(OperatorCache<T>::at(OpIndex))[0]):];

        // Convert `other` Python arg. If conversion fails (e.g., Vec3 + int),
        // return NotImplemented so Python tries the reverse operand's
        // __radd__ and eventually raises TypeError if no handler accepts.
        mirror_bridge::core::param_storage_t<OtherParam> storage;
        if (!extract_param<OtherParam>(other, storage)) {
            PyErr_Clear();
            Py_RETURN_NOTIMPLEMENTED;
        }

        try {
            if constexpr (InPlace) {
                ((*wrapper->cpp_object).[:OperatorCache<T>::at(OpIndex):])(
                    forward_arg<OtherParam>(storage));
                Py_INCREF(self);
                return self;
            } else {
                using Ret = typename [:std::meta::return_type_of(
                    OperatorCache<T>::at(OpIndex)):];
                if constexpr (std::is_void_v<Ret>) {
                    Py_RETURN_NONE;
                } else {
                    Ret result = ((*wrapper->cpp_object).[:OperatorCache<T>::at(OpIndex):])(
                        forward_arg<OtherParam>(storage));
                    return to_python(result);
                }
            }
        } catch (const std::exception& e) {
            set_py_error_from_cpp_exception(e);
            return nullptr;
        }
    }
}

// Unary operator wrapper for `-a`, `+a` (no arguments).
template<typename T, std::size_t OpIndex>
PyObject* unary_op_wrapper(PyObject* self) {
    auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);
    if (!wrapper->cpp_object) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid C++ object");
        return nullptr;
    }
    using Ret = typename [:std::meta::return_type_of(OperatorCache<T>::at(OpIndex)):];
    try {
        if constexpr (std::is_void_v<Ret>) {
            ((*wrapper->cpp_object).[:OperatorCache<T>::at(OpIndex):])();
            Py_RETURN_NONE;
        } else {
            Ret result = ((*wrapper->cpp_object).[:OperatorCache<T>::at(OpIndex):])();
            return to_python(result);
        }
    } catch (const std::exception& e) {
        set_py_error_from_cpp_exception(e);
        return nullptr;
    }
}

// Subscript: a[i] → a.operator[](i). Returns the result converted to Python.
template<typename T, std::size_t OpIndex>
PyObject* subscript_wrapper(PyObject* self, PyObject* key) {
    return binary_op_wrapper<T, OpIndex, /*InPlace=*/false>(self, key);
}

// Call: a(args...) → a.operator()(args...). For simplicity we support only
// single-arg operator() (common shape); extending to multi-arg follows the
// dispatch we built for regular methods.
template<typename T, std::size_t OpIndex>
PyObject* call_wrapper(PyObject* self, PyObject* args, PyObject* /*kwds*/) {
    auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);
    if (!wrapper->cpp_object) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid C++ object");
        return nullptr;
    }

    if constexpr (op_param_count<T, OpIndex>() != 1) {
        PyErr_SetString(PyExc_TypeError, "operator() arity unsupported");
        return nullptr;
    } else {
        if (PyTuple_Size(args) != 1) {
            PyErr_SetString(PyExc_TypeError, "operator() expects 1 argument");
            return nullptr;
        }
        return binary_op_wrapper<T, OpIndex, /*InPlace=*/false>(
            self, PyTuple_GET_ITEM(args, 0));
    }
}

// Single tp_richcompare that dispatches to whichever comparison the class has.
// C++ comparison operators return bool. We convert to PyBool (Py_True /
// Py_False) rather than PyLong so Python's `x == y` reads as a proper
// boolean, not as 0/1.
template<typename T>
PyObject* richcompare_wrapper(PyObject* self, PyObject* other, int op) {
    auto normalize_bool = [](PyObject* r) -> PyObject* {
        if (!r || r == OVERLOAD_TRY_NEXT) return r;
        if (PyBool_Check(r)) return r;
        if (PyLong_Check(r)) {
            long v = PyLong_AsLong(r);
            Py_DECREF(r);
            return PyBool_FromLong(v);
        }
        return r;
    };

    auto try_slot = [&]<Slot S, int PyOpCode>() -> PyObject* {
        if (op != PyOpCode) return OVERLOAD_TRY_NEXT;
        constexpr std::size_t idx = find_op<T>(S);
        if constexpr (idx == OperatorCache<T>::count) {
            return OVERLOAD_TRY_NEXT;
        } else {
            return normalize_bool(
                binary_op_wrapper<T, idx, /*InPlace=*/false>(self, other));
        }
    };

    PyObject* r = OVERLOAD_TRY_NEXT;
    (void)((r = try_slot.template operator()<Slot::Eq, Py_EQ>(), r != OVERLOAD_TRY_NEXT) ||
           (r = try_slot.template operator()<Slot::Ne, Py_NE>(), r != OVERLOAD_TRY_NEXT) ||
           (r = try_slot.template operator()<Slot::Lt, Py_LT>(), r != OVERLOAD_TRY_NEXT) ||
           (r = try_slot.template operator()<Slot::Gt, Py_GT>(), r != OVERLOAD_TRY_NEXT) ||
           (r = try_slot.template operator()<Slot::Le, Py_LE>(), r != OVERLOAD_TRY_NEXT) ||
           (r = try_slot.template operator()<Slot::Ge, Py_GE>(), r != OVERLOAD_TRY_NEXT));
    if (r == OVERLOAD_TRY_NEXT) Py_RETURN_NOTIMPLEMENTED;
    return r;
}

// Build PyNumberMethods with slots filled according to T's operators.
template<typename T>
PyNumberMethods* build_number_methods() {
    static PyNumberMethods nm{};
    constexpr std::size_t add_idx = find_op<T>(Slot::Add);
    constexpr std::size_t sub_idx = find_op<T>(Slot::Sub);
    constexpr std::size_t mul_idx = find_op<T>(Slot::Mul);
    constexpr std::size_t div_idx = find_op<T>(Slot::Div);
    constexpr std::size_t mod_idx = find_op<T>(Slot::Mod);
    constexpr std::size_t iadd_idx = find_op<T>(Slot::IAdd);
    constexpr std::size_t isub_idx = find_op<T>(Slot::ISub);
    constexpr std::size_t imul_idx = find_op<T>(Slot::IMul);
    constexpr std::size_t idiv_idx = find_op<T>(Slot::IDiv);
    constexpr std::size_t imod_idx = find_op<T>(Slot::IMod);
    constexpr std::size_t neg_idx = find_op<T>(Slot::Negative);
    constexpr std::size_t pos_idx = find_op<T>(Slot::Positive);
    constexpr std::size_t N = OperatorCache<T>::count;

    if constexpr (add_idx < N)  nm.nb_add = binary_op_wrapper<T, add_idx, false>;
    if constexpr (sub_idx < N)  nm.nb_subtract = binary_op_wrapper<T, sub_idx, false>;
    if constexpr (mul_idx < N)  nm.nb_multiply = binary_op_wrapper<T, mul_idx, false>;
    if constexpr (div_idx < N)  nm.nb_true_divide = binary_op_wrapper<T, div_idx, false>;
    if constexpr (mod_idx < N)  nm.nb_remainder = binary_op_wrapper<T, mod_idx, false>;
    if constexpr (iadd_idx < N) nm.nb_inplace_add = binary_op_wrapper<T, iadd_idx, true>;
    if constexpr (isub_idx < N) nm.nb_inplace_subtract = binary_op_wrapper<T, isub_idx, true>;
    if constexpr (imul_idx < N) nm.nb_inplace_multiply = binary_op_wrapper<T, imul_idx, true>;
    if constexpr (idiv_idx < N) nm.nb_inplace_true_divide = binary_op_wrapper<T, idiv_idx, true>;
    if constexpr (imod_idx < N) nm.nb_inplace_remainder = binary_op_wrapper<T, imod_idx, true>;
    if constexpr (neg_idx < N)  nm.nb_negative = unary_op_wrapper<T, neg_idx>;
    if constexpr (pos_idx < N)  nm.nb_positive = unary_op_wrapper<T, pos_idx>;

    // Return null if nothing was populated, so tp_as_number stays null.
    if constexpr (add_idx >= N && sub_idx >= N && mul_idx >= N && div_idx >= N &&
                  mod_idx >= N && iadd_idx >= N && isub_idx >= N && imul_idx >= N &&
                  idiv_idx >= N && imod_idx >= N && neg_idx >= N && pos_idx >= N) {
        return nullptr;
    }
    return &nm;
}

// Build PyMappingMethods with mp_subscript set if operator[] exists.
template<typename T>
PyMappingMethods* build_mapping_methods() {
    static PyMappingMethods mm{};
    constexpr std::size_t sub_idx = find_op<T>(Slot::Subscript);
    if constexpr (sub_idx < OperatorCache<T>::count) {
        mm.mp_subscript = subscript_wrapper<T, sub_idx>;
        return &mm;
    }
    return nullptr;
}

template<typename T>
ternaryfunc build_call() {
    constexpr std::size_t idx = find_op<T>(Slot::Call);
    if constexpr (idx < OperatorCache<T>::count) {
        return call_wrapper<T, idx>;
    }
    return nullptr;
}

template<typename T>
richcmpfunc build_richcompare() {
    constexpr std::size_t eq = find_op<T>(Slot::Eq);
    constexpr std::size_t ne = find_op<T>(Slot::Ne);
    constexpr std::size_t lt = find_op<T>(Slot::Lt);
    constexpr std::size_t gt = find_op<T>(Slot::Gt);
    constexpr std::size_t le = find_op<T>(Slot::Le);
    constexpr std::size_t ge = find_op<T>(Slot::Ge);
    constexpr std::size_t N  = OperatorCache<T>::count;
    if constexpr (eq < N || ne < N || lt < N || gt < N || le < N || ge < N) {
        return richcompare_wrapper<T>;
    }
    return nullptr;
}

}  // namespace ops

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
// ============================================================================
// Interned Member Name Cache
// ============================================================================
//
// Pre-interns Python string objects for member names at first use, avoiding
// repeated string allocation in tight loops. PyUnicode_InternFromString
// creates an immortal string that's reused by identity comparison in dict
// lookups, making both PyDict_SetItem and PyDict_GetItem faster.

template<typename T>
struct InternedMemberNames {
    static constexpr std::size_t count = get_nested_member_count<T>();

    // Lazily-initialized cache of interned Python string objects.
    // Thread-safe via std::call_once (GIL is held by callers).
    static std::array<PyObject*, count>& get() {
        static std::array<PyObject*, count> names{};
        static std::once_flag init_flag;
        std::call_once(init_flag, [&]() {
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((names[Is] = PyUnicode_InternFromString(get_nested_member_name<T, Is>())), ...);
            }(std::make_index_sequence<count>{});
        });
        return names;
    }
};

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
        auto& interned_names = InternedMemberNames<T>::get();
        bool success = true;

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (!success) return;

                constexpr auto member = get_nested_member<T, Is>();

                const auto& value = obj.[:member:];
                PyObject* py_value = to_python(value);

                // Use interned string key for O(1) identity-based dict lookup
                if (!py_value || PyDict_SetItem(dict, interned_names[Is], py_value) < 0) {
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

    // Generate from_python overload at compile time.
    // Handles three special cases via if-constexpr:
    //   1. Array members (T[N]): fill in-place, no whole-array assignment.
    //   2. Abstract types: can't be default-constructed; skip (leave member
    //      at its existing value — caller presumably set it via setter).
    //   3. Non-assignable types: skip (leave member as-is).
    // The normal case (value-semantic type): default-construct, populate,
    // move-assign.
    static bool from_python_impl(PyObject* obj, T& out) {
        if (!PyDict_Check(obj)) return false;

        constexpr std::size_t member_count = get_nested_member_count<T>();
        auto& interned_names = InternedMemberNames<T>::get();
        bool success = true;

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (!success) return;

                constexpr auto member = get_nested_member<T, Is>();
                using MemberType = NestedMemberType<T, Is>;

                PyObject* py_value = PyDict_GetItem(obj, interned_names[Is]);
                if (!py_value) {
                    // Missing key is not necessarily fatal — leave member as-is
                    return;
                }

                if constexpr (std::is_array_v<MemberType>) {
                    // Array: fill in-place via from_python overload for T[N]
                    if (!from_python(py_value, out.[:member:])) {
                        success = false;
                    }
                } else if constexpr (std::is_abstract_v<MemberType> ||
                                     !std::is_default_constructible_v<MemberType> ||
                                     !std::is_move_assignable_v<MemberType>) {
                    // Abstract or non-assignable member: can't reconstruct from
                    // dict. Silently skip — user should populate via setters.
                    return;
                } else {
                    MemberType cpp_value;
                    if (!from_python(py_value, cpp_value)) {
                        success = false;
                        return;
                    }
                    out.[:member:] = std::move(cpp_value);
                }
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

    // Polymorphic return: if CleanT has a vtable, check typeid(obj) for the
    // dynamic type and prefer a wrapper of that derived type. Without this,
    // `shared_ptr<Geometry>` returning a PointCloud would give Python a
    // Geometry wrapper, missing the derived class's methods.
    PyTypeObject* py_type = nullptr;
    if constexpr (std::is_polymorphic_v<CleanT>) {
        PyObject* registry = get_python_type_registry();
        if (registry) {
            const char* dyn_name = typeid(obj).name();
            PyObject* derived = PyDict_GetItemString(registry, dyn_name);
            if (derived) py_type = reinterpret_cast<PyTypeObject*>(derived);
        }
    }

    // Fall back to static-type lookup
    if (!py_type) py_type = lookup_type_in_python<CleanT>();
    if (!py_type) py_type = TypeRegistry<CleanT>::py_type;

    if (py_type) {
        // Create a wrapper object.
        // For concrete types: copy the value into a heap-allocated instance.
        // For abstract types: we can't copy/construct (no complete type to allocate),
        // but the caller might still want the wrapper — return a wrapper with
        // nullptr cpp_object. Callers should use bind_class for derived concrete
        // types in practice. This avoids hard-failing compilation when binding
        // happens to encounter an abstract base class in a return type chain.
        if constexpr (std::is_abstract_v<CleanT> || !std::is_copy_constructible_v<CleanT>) {
            // Cannot copy-construct — return a dict representation instead.
            // This is a graceful fallback: the user gets a read-only snapshot of
            // the abstract object's members rather than a broken wrapper.
            return ConversionOverloadGenerator<T>::to_python_impl(obj);
        } else {
            auto* wrapper = reinterpret_cast<PyWrapper<CleanT>*>(py_type->tp_alloc(py_type, 0));
            if (!wrapper) {
                return nullptr;
            }
            wrapper->cpp_object = new CleanT(obj);  // Copy the object
            wrapper->owns = true;
            return reinterpret_cast<PyObject*>(wrapper);
        }
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

// Main binding function — generates Python type object for a C++ class.
//
// Takes an optional `Trampoline` template parameter (defaulting to T). When
// set to a trampoline class that derives from T and overrides every virtual,
// Python subclasses can override those virtuals transparently: the trampoline
// forwards each virtual call to the Python override if one exists.
// The `mirror_bridge generate --trampolines` tool emits the trampoline class.
template<Bindable T, typename Trampoline = T>
PyTypeObject* bind_class(PyObject* module, const char* name, const char* file_hash = nullptr) {
    static_assert(std::is_base_of_v<T, Trampoline> || std::is_same_v<T, Trampoline>,
        "Trampoline must derive from T (or be T itself)");
    // Compile-time validation: ensure all member types are convertible
    static_assert(core::validate_bindable_members<T>(),
        "bind_class<T>: T contains members with types that mirror_bridge cannot convert. "
        "Mark unconvertible members with [[=exclude{}]] or add a custom type converter. "
        "Supported: arithmetic, std::string, containers, smart pointers, "
        "std::optional, std::expected, enums, and nested bindable classes.");

    // Generate type signature for hash-based change detection
    auto signature = generate_type_signature<T>(file_hash);

    // Register class in the global registry
    Registry::instance().register_class(name, signature);

    // Get visible member count (excludes fields marked with [[=exclude{}]])
    constexpr std::size_t visible_member_count = annotations::count_visible_members<T>();

    // Generate getters/setters for visible members only
    // Readonly fields (marked with [[=readonly{}]]) get a setter that raises AttributeError
    static auto getsetters = generate_visible_getsetters<T>(std::make_index_sequence<visible_member_count>{});

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

    // Generate a repr that shows class name + each member value. Falls back
    // to the original `<ClassName at 0xaddr>` form only if every member fails
    // to convert to Python (which shouldn't happen once bind_class passed
    // validate_bindable_members).
    //
    // Example output: PointCloud(points=[[0,0,0],[1,0,0]], colors=[])
    static auto py_repr_func = +[](PyObject* self) -> PyObject* {
        auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);
        if (!wrapper->cpp_object) {
            return PyUnicode_FromFormat("<%s object (invalid)>", class_name);
        }

        constexpr std::size_t member_count = annotations::count_visible_members<T>();
        if constexpr (member_count == 0) {
            return PyUnicode_FromFormat("%s()", class_name);
        } else {
            PyObject* parts = PyList_New(0);
            if (!parts) return nullptr;

            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ([&] {
                    constexpr auto member = annotations::get_visible_member<T>(Is);
                    constexpr const char* name = std::meta::identifier_of(member).data();
                    const auto& value = (*wrapper->cpp_object).[:member:];
                    PyObject* py_val = to_python(value);
                    if (py_val) {
                        PyObject* py_val_repr = PyObject_Repr(py_val);
                        Py_DECREF(py_val);
                        if (py_val_repr) {
                            PyObject* part = PyUnicode_FromFormat("%s=%U", name, py_val_repr);
                            Py_DECREF(py_val_repr);
                            if (part) {
                                PyList_Append(parts, part);
                                Py_DECREF(part);
                            }
                        }
                    }
                }(), ...);
            }(std::make_index_sequence<member_count>{});

            PyObject* sep = PyUnicode_FromString(", ");
            PyObject* joined = PyUnicode_Join(sep, parts);
            Py_DECREF(sep);
            Py_DECREF(parts);

            PyObject* result = PyUnicode_FromFormat("%s(%U)", class_name, joined);
            Py_DECREF(joined);
            return result;
        }
    };

    // Operator slots derived from reflection. Each build_* returns a valid
    // pointer only if T declares at least one operator of that category,
    // so types with no operators pay nothing.
    static PyNumberMethods* num_slots = ops::build_number_methods<T>();
    static PyMappingMethods* map_slots = ops::build_mapping_methods<T>();
    static ternaryfunc call_slot = ops::build_call<T>();
    static richcmpfunc rich_slot = ops::build_richcompare<T>();

    // Define the Python type structure
    // Note: Inheritance is implicitly supported - reflection sees all members including inherited ones
    static PyTypeObject type_object = {
        PyVarObject_HEAD_INIT(nullptr, 0)
        .tp_name = name,
        .tp_basicsize = sizeof(PyWrapper<T>),
        .tp_itemsize = 0,
        .tp_dealloc = py_dealloc<T>,
        .tp_repr = (reprfunc)py_repr_func,
        .tp_as_number = num_slots,
        .tp_as_mapping = map_slots,
        .tp_call = call_slot,
        .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  // Allow subclassing
        .tp_doc = "Auto-generated binding via mirror_bridge reflection",
        .tp_richcompare = rich_slot,
        .tp_methods = methods.data(),
        .tp_getset = getsetters.data(),
        .tp_init = (ctor_count > 0) ? (initproc)py_init<T, Trampoline> : nullptr,
        .tp_new = (ctor_count > 0) ? py_new_with_init<T> : py_new<T, Trampoline>,
    };

    // Initialize and register the type with Python
    if (PyType_Ready(&type_object) < 0) {
        return nullptr;
    }

    // Add static methods to the type dictionary
    // Python's PyMethodDef with METH_STATIC doesn't work - we need to manually wrap them.
    // Skip entries whose ml_name is null — generate_static_methods leaves those
    // zero-initialized for static methods we couldn't bind (e.g., raw-pointer
    // params to user types). Creating a PyCFunction with a null ml_name
    // dereferences it via PyErr_Format and segfaults.
    if constexpr (static_method_count > 0) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (!static_methods[Is].ml_name) return;
                constexpr auto func_name = get_static_member_function_name<T, Is>();
                PyObject* func = PyCFunction_New(&static_methods[Is], nullptr);
                if (func) {
                    PyObject* static_method = PyStaticMethod_New(func);
                    Py_DECREF(func);
                    if (static_method) {
                        PyObject* type_dict = type_object.tp_dict;
                        PyDict_SetItemString(type_dict, func_name, static_method);
                        Py_DECREF(static_method);
                    }
                }
            }(), ...);
        }(std::make_index_sequence<static_method_count>{});
    }

    // Add static data members (including constexpr) to the type dictionary
    // These are added as read-only class attributes
    constexpr std::size_t static_data_member_count = get_static_data_member_count<T>();
    if constexpr (static_data_member_count > 0) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                constexpr auto member_name = get_static_data_member_name<T, Is>();
                // Get the Python value for this static member
                PyObject* py_value = get_static_data_member_value<T, Is>();
                if (py_value) {
                    // Add directly to type dict - Python will make it a class attribute
                    PyObject* type_dict = type_object.tp_dict;
                    PyDict_SetItemString(type_dict, member_name, py_value);
                    Py_DECREF(py_value);
                }
            }(), ...);
        }(std::make_index_sequence<static_data_member_count>{});
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

// ============================================================================
// Free Function Binding
// ============================================================================
//
// Bind namespace-level (free) functions to Python modules using the same
// reflection-based approach as class methods.
//
// Usage:
//   mirror_bridge::bind_function<&my_namespace::my_func>(module, "my_func");

// Helper to extract function signature info from a function pointer type
template<typename>
struct FunctionTraits;

template<typename R, typename... Args>
struct FunctionTraits<R(*)(Args...)> {
    using ReturnType = R;
    using ArgsTuple = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
};

// Helper to get the storage type for function arguments
// References need to be stored as values, then passed as references
template<typename T>
using StorageType = std::remove_cvref_t<T>;

// Wrapper for calling a free function with Python arguments
template<auto FuncPtr, std::size_t... Is>
PyObject* call_free_function_impl(PyObject* args, std::index_sequence<Is...>) {
    using Traits = FunctionTraits<decltype(FuncPtr)>;
    using ArgsTuple = typename Traits::ArgsTuple;
    using ReturnType = typename Traits::ReturnType;
    constexpr std::size_t arity = Traits::arity;

    // Check argument count
    Py_ssize_t nargs = PyTuple_Size(args);
    if (nargs != static_cast<Py_ssize_t>(arity)) {
        PyErr_Format(PyExc_TypeError,
            "Function takes %zu argument(s) but %zd were given",
            arity, nargs);
        return nullptr;
    }

    // Storage tuple holds actual values (not references)
    using StorageTuple = std::tuple<StorageType<std::tuple_element_t<Is, ArgsTuple>>...>;
    StorageTuple cpp_args;
    bool conversion_ok = true;

    // Use fold expression to convert each argument
    ([&] {
        if (!conversion_ok) return;
        using ArgType = std::tuple_element_t<Is, ArgsTuple>;
        using CleanArgType = std::remove_cvref_t<ArgType>;
        PyObject* py_arg = PyTuple_GetItem(args, Is);

        CleanArgType value;
        if (from_python(py_arg, value)) {
            std::get<Is>(cpp_args) = std::move(value);
        } else {
            if (!PyErr_Occurred()) {
                PyErr_Format(PyExc_TypeError, "Argument %zu: type conversion failed", Is + 1);
            }
            conversion_ok = false;
        }
    }(), ...);

    if (!conversion_ok) {
        return nullptr;
    }

    // Call the function - stored values are passed, references work because
    // the storage tuple is still in scope
    if constexpr (std::is_void_v<ReturnType>) {
        FuncPtr(std::get<Is>(cpp_args)...);
        Py_RETURN_NONE;
    } else {
        auto result = FuncPtr(std::get<Is>(cpp_args)...);
        return to_python(result);
    }
}

template<auto FuncPtr>
PyObject* call_free_function(PyObject* /*self*/, PyObject* args) {
    using Traits = FunctionTraits<decltype(FuncPtr)>;
    return call_free_function_impl<FuncPtr>(args, std::make_index_sequence<Traits::arity>{});
}

// Bind a free function to a Python module
// FuncPtr is a non-type template parameter (the actual function pointer)
template<auto FuncPtr>
bool bind_function(PyObject* module, const char* name) {
    // Create a static PyMethodDef for this function
    static PyMethodDef method_def = {
        name,
        reinterpret_cast<PyCFunction>(call_free_function<FuncPtr>),
        METH_VARARGS,
        nullptr  // doc
    };

    // Create a function object and add to module
    PyObject* func = PyCFunction_New(&method_def, nullptr);
    if (!func) {
        return false;
    }

    if (PyModule_AddObject(module, name, func) < 0) {
        Py_DECREF(func);
        return false;
    }

    return true;
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
