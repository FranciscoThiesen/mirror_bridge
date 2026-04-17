#pragma once

// ============================================================================
// Auto-Trampoline: Python-overridable C++ virtuals without hand-written glue.
// ============================================================================
//
// P2996R13 doesn't ship code injection (P3294), so we can't synthesize new
// method declarations purely from reflection. But we CAN enumerate virtuals,
// generate per-virtual dispatcher functions with exact signatures (via
// reflection splicing of return/parameter types), and swap an object's vptr
// to point at a custom vtable containing those dispatchers.
//
// Mechanism (Itanium C++ ABI — Linux/macOS, not MSVC):
//   1. For each virtual V of T (enumerated via std::meta::bases_of +
//      std::meta::is_virtual), reflection gives us V's return type and
//      parameter type list. We generate a free function `dispatcher_V`
//      with signature `R(*)(const T*, ParamTypes...)` — the exact layout
//      a vtable slot holds under Itanium.
//   2. On bind_class_auto<T>, we build a custom vtable whose preamble
//      (offset_to_top, typeinfo) is copied from T's original vtable, whose
//      dtor slots point at T's originals, and whose method slots point at
//      our dispatchers. The vtable is built once per T at first construction.
//   3. When Python instantiates a subclass, tp_new allocates a T, saves the
//      original vptr in a per-instance side-map along with the PyObject*
//      back-reference, then overwrites the object's vptr with the custom
//      vtable entry point.
//   4. Any C++ virtual call on that instance — from user code, from other
//      C++ libraries, from itself — lands in our dispatcher. The dispatcher
//      checks whether Python has overridden this method (via `py_self_` and
//      has_python_override); if yes it marshals args and calls Python via
//      the existing dispatch_python<Ret, Args...> helper; if no it looks up
//      the original vtable slot we saved and calls through (preserving the
//      normal C++ polymorphic behavior for non-overridden methods).
//
// Trade-offs:
//   - Itanium ABI only. MSVC uses a different vtable layout.
//   - Must avoid full devirtualization (use pointer access, not local object).
//   - Abstract base virtuals without a Python override hit
//     __cxa_pure_virtual and abort; callers must provide overrides for
//     every pure virtual (same as pybind11).
//   - Non-trivial tracing cost: each virtual call goes C++ → dispatcher →
//     either Python or original slot. Measured overhead is a function-pointer
//     hop plus an unordered_map lookup.
//
// This is a magic-mode alternative to the explicit TrampolineBase pattern.
// The plain TrampolineBase (in mirror_bridge_python.hpp) remains the
// portable fallback for MSVC or users who prefer auditable trampoline code.

#include "mirror_bridge_python.hpp"

#include <unordered_map>
#include <shared_mutex>

namespace mirror_bridge {

namespace detail {

// Per-type side-map: instance pointer -> (original vptr, PyObject* py_self).
template<typename T>
struct AutoTrampolineRegistry {
    struct Entry {
        void** orig_vptr;
        PyObject* py_self;
    };
    inline static std::unordered_map<const void*, Entry> data;
    inline static std::shared_mutex mtx;

    static void register_instance(T* obj, void** orig_vptr, PyObject* py_self) {
        std::unique_lock lk(mtx);
        data[obj] = {orig_vptr, py_self};
    }

    static void unregister(const T* obj) {
        std::unique_lock lk(mtx);
        data.erase(obj);
    }

    static Entry* lookup(const T* obj) {
        std::shared_lock lk(mtx);
        auto it = data.find(obj);
        return it == data.end() ? nullptr : &it->second;
    }
};

// Enumerate directly-declared virtuals of T in declaration order (the order
// Itanium places them in the vtable). We deliberately do NOT walk bases —
// the vtable inherits base slots at fixed positions, and overriding in T
// replaces the base's slot. For the common single-inheritance-from-abstract
// pattern used by Open3D and similar, this matches vtable layout.
template<typename T>
consteval std::vector<std::meta::info> auto_virtuals_of() {
    std::vector<std::meta::info> out;
    auto ctx = std::meta::access_context::current();
    for (auto m : std::meta::members_of(^^T, ctx)) {
        if (!std::meta::is_function(m)) continue;
        if (!std::meta::is_virtual(m)) continue;
        if (std::meta::is_special_member_function(m)) continue;
        out.push_back(m);
    }
    return out;
}

template<typename T, std::size_t I>
consteval std::meta::info nth_virtual() {
    return auto_virtuals_of<T>()[I];
}

template<typename T>
consteval std::size_t virtual_count() {
    return auto_virtuals_of<T>().size();
}

// Generate a dispatcher function for the I-th virtual of T. The returned
// function pointer has the exact signature of that virtual (including
// receiver type T*), matching what an Itanium vtable slot must hold.
template<typename T, std::size_t VIndex>
constexpr void* make_auto_dispatcher() {
    constexpr auto M = nth_virtual<T, VIndex>();
    constexpr auto params = std::meta::parameters_of(M);

    return []<std::size_t... Is>(std::index_sequence<Is...>) -> void* {
        using Ret = typename [:std::meta::return_type_of(nth_virtual<T, VIndex>()):];
        using SlotFn = Ret(*)(const T*,
            typename [:std::meta::type_of(std::meta::parameters_of(nth_virtual<T, VIndex>())[Is]):]...);

        // Method name for Python lookup (stable pointer from reflection).
        constexpr auto name_sv = std::meta::identifier_of(nth_virtual<T, VIndex>());
        static constexpr const char* name = name_sv.data();

        // Itanium preamble contributes 2 dtor slots before user virtuals.
        constexpr std::size_t dtor_slots = 2;
        constexpr std::size_t orig_slot = dtor_slots + VIndex;

        return reinterpret_cast<void*>(+[](const T* self,
                typename [:std::meta::type_of(std::meta::parameters_of(nth_virtual<T, VIndex>())[Is]):]... args) -> Ret {
            auto* entry = AutoTrampolineRegistry<T>::lookup(self);
            if (entry && entry->py_self &&
                    mirror_bridge::has_python_override(entry->py_self, name)) {
                // Dispatch to Python. We piggyback on the existing TrampolineBase
                // helper's logic but inline it here because we don't have a
                // TrampolineBase instance — py_self lives in the registry.
                PyObject* method = PyObject_GetAttrString(entry->py_self, name);
                if (!method) {
                    PyErr_Clear();
                    // Shouldn't happen after has_python_override returned true
                    if constexpr (std::is_void_v<Ret>) return;
                    else return Ret{};
                }
                PyObject* py_args = PyTuple_New(sizeof...(Is));
                if constexpr (sizeof...(Is) > 0) {
                    std::size_t idx = 0;
                    ((PyTuple_SET_ITEM(py_args, idx++, to_python(args))), ...);
                }
                PyObject* result = PyObject_CallObject(method, py_args);
                Py_DECREF(py_args);
                Py_DECREF(method);
                if (!result) {
                    PyErr_Print();
                    throw std::runtime_error(std::string("Python override for ") + name + " raised");
                }
                if constexpr (std::is_void_v<Ret>) {
                    Py_DECREF(result);
                    return;
                } else {
                    Ret r;
                    if (!from_python(result, r)) {
                        Py_DECREF(result);
                        throw std::runtime_error(std::string("Couldn't convert return of ") + name);
                    }
                    Py_DECREF(result);
                    return r;
                }
            }

            // No Python override — fall through to the original C++ slot.
            if (entry) {
                void** orig = entry->orig_vptr;
                return reinterpret_cast<SlotFn>(orig[orig_slot])(self, args...);
            }

            // Shouldn't reach here for registered instances; defend anyway.
            if constexpr (std::is_void_v<Ret>) return;
            else return Ret{};
        });
    }(std::make_index_sequence<params.size()>{});
}

template<typename T>
struct CustomVTable {
    static inline void** vtable = nullptr;

    template<std::size_t... Is>
    static void build_slots(void** vt, std::index_sequence<Is...>) {
        constexpr std::size_t preamble = 2;
        constexpr std::size_t dtor_slots = 2;
        ((vt[preamble + dtor_slots + Is] = make_auto_dispatcher<T, Is>()), ...);
    }

    static void ensure_built(T* seed) {
        if (vtable) return;
        void** orig = *(void***)seed;
        constexpr std::size_t preamble = 2;
        constexpr std::size_t dtor_slots = 2;
        constexpr std::size_t N = virtual_count<T>();

        vtable = new void*[preamble + dtor_slots + N];
        vtable[0] = orig[-2];  // offset_to_top
        vtable[1] = orig[-1];  // typeinfo
        vtable[2] = orig[0];   // complete dtor
        vtable[3] = orig[1];   // deleting dtor

        build_slots(vtable, std::make_index_sequence<N>{});
    }

    // Caller vptr points here (past the preamble).
    static void** entry_point() { return &vtable[2]; }
};

// Called from bind_class_auto's tp_new. Allocates T, installs the custom
// vtable, registers py_self so dispatchers can find it, and returns the
// PyWrapper<T>*.
template<typename T>
inline PyObject* py_new_auto(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    auto* self = reinterpret_cast<PyWrapper<T>*>(type->tp_alloc(type, 0));
    if (!self) return nullptr;

    if constexpr (std::is_default_constructible_v<T>) {
        T* obj = new T();
        self->cpp_object = obj;
        self->owns = true;

        // Only install vtable swap for instances that are Python subclasses.
        // If the user instantiated the bound type directly with no override
        // intent, we could skip installation; but installing is cheap and
        // the fallback path is correct, so we always install.
        CustomVTable<T>::ensure_built(obj);
        void** orig_vptr = *(void***)obj;
        AutoTrampolineRegistry<T>::register_instance(
            obj, orig_vptr, reinterpret_cast<PyObject*>(self));
        *(void***)obj = CustomVTable<T>::entry_point();
    } else {
        self->cpp_object = nullptr;
        self->owns = false;
    }
    return reinterpret_cast<PyObject*>(self);
}

template<typename T>
inline void py_dealloc_auto(PyObject* self) {
    auto* wrapper = reinterpret_cast<PyWrapper<T>*>(self);
    if (wrapper->owns && wrapper->cpp_object) {
        // Restore original vptr before deletion so T's real destructor runs
        // through the correct vtable.
        auto* entry = AutoTrampolineRegistry<T>::lookup(wrapper->cpp_object);
        if (entry) {
            *(void***)wrapper->cpp_object = entry->orig_vptr;
            AutoTrampolineRegistry<T>::unregister(wrapper->cpp_object);
        }
        delete wrapper->cpp_object;
    }
    Py_TYPE(self)->tp_free(self);
}

}  // namespace detail

// Magic-mode bind_class: Python subclasses can override any virtual method
// of T without a hand-written trampoline class. Reflection enumerates T's
// virtuals, generates per-virtual dispatchers whose signatures come entirely
// from the reflection-spliced return/param types, assembles a custom vtable,
// and swaps the instance's vptr on construction.
//
// Itanium ABI only (Linux/macOS). MSVC-compatible users should fall back to
// the explicit bind_class<T, TrampolineT> path.
template<Bindable T>
PyTypeObject* bind_class_auto(PyObject* module, const char* name,
                              const char* file_hash = nullptr) {
    // Leverage bind_class for the regular binding setup (methods, getters,
    // etc.), then override tp_new / tp_dealloc to install the vtable swap.
    PyTypeObject* type = bind_class<T>(module, name, file_hash);
    if (!type) return nullptr;
    type->tp_new = detail::py_new_auto<T>;
    type->tp_dealloc = detail::py_dealloc_auto<T>;
    return type;
}

}  // namespace mirror_bridge
