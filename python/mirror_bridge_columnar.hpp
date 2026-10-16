#pragma once

// ============================================================================
// Mirror Bridge - Columnar returns for vectors of plain structs
// ============================================================================
//
// `std::vector<Point3>` returns to Python as a list of Point3 objects. That is
// one Python object and one `new Point3` per element, which for 100,000 points
// measured at 8.9 ms against 89 us for the same 2.4 MB as a
// std::vector<double>: a hundred times the cost of moving the bytes.
//
// The bytes are already contiguous and, for a struct whose members are all
// arithmetic, they are a complete description of the value. So they can be
// handed over as they lie. What this header adds is the description numpy
// needs in order to read them in place, built from reflection:
//
//     std::meta::offset_of        where each member starts
//     std::meta::size_of          how big the struct is, padding included
//     std::meta::identifier_of    what each member is called
//
// That is the whole of numpy's `__array_interface__`, and reading it off the
// type is the part no other binding generator can do: pybind11 wants
// PYBIND11_NUMPY_DTYPE(Point3, x, y, z) written out by hand for every struct,
// and nanobind's ndarray has no record dtype at all.
//
// Nothing here imports or links numpy. `__array_interface__` is a plain dict,
// so the module describes itself and numpy is only needed by whoever wants an
// array out of it.
//
// ============================================================================

#include <Python.h>

#include <bit>
#include <cstddef>
#include <cstdio>
#include <new>
#include <cstdint>
#include <meta>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace mirror_bridge {
namespace columnar {

// ============================================================================
// numpy type strings
// ============================================================================

// An `__array_interface__` type string: byte order, kind, size in bytes.
// Returns nullptr for a type numpy cannot name, which is what decides whether
// a struct is describable at all.
template<typename T>
consteval const char* typestr() {
    using C = std::remove_cv_t<T>;
    constexpr bool le = (std::endian::native == std::endian::little);

    // One-byte types take '|', meaning byte order does not apply. Anything
    // wider has to say which end it starts at, because the consumer may be
    // reading the buffer on a machine that disagrees.
    if constexpr (std::is_same_v<C, bool>) {
        return sizeof(bool) == 1 ? "|b1" : nullptr;
    } else if constexpr (std::is_same_v<C, char>) {
        return std::is_signed_v<char> ? "|i1" : "|u1";
    } else if constexpr (std::is_same_v<C, signed char>) {
        return "|i1";
    } else if constexpr (std::is_same_v<C, unsigned char>) {
        return "|u1";
    } else if constexpr (std::is_same_v<C, short>) {
        return sizeof(short) == 2 ? (le ? "<i2" : ">i2") : nullptr;
    } else if constexpr (std::is_same_v<C, unsigned short>) {
        return sizeof(unsigned short) == 2 ? (le ? "<u2" : ">u2") : nullptr;
    } else if constexpr (std::is_same_v<C, int>) {
        return sizeof(int) == 4 ? (le ? "<i4" : ">i4") : nullptr;
    } else if constexpr (std::is_same_v<C, unsigned int>) {
        return sizeof(unsigned int) == 4 ? (le ? "<u4" : ">u4") : nullptr;
    } else if constexpr (std::is_same_v<C, long> || std::is_same_v<C, long long>) {
        // LP64 gives 8 and LLP64/ILP32 give 4. Anything else is a target this
        // does not know, and naming a width it does not have would shift every
        // column after it.
        if (sizeof(C) == 8) return le ? "<i8" : ">i8";
        return sizeof(C) == 4 ? (le ? "<i4" : ">i4") : nullptr;
    } else if constexpr (std::is_same_v<C, unsigned long> ||
                         std::is_same_v<C, unsigned long long>) {
        if (sizeof(C) == 8) return le ? "<u8" : ">u8";
        return sizeof(C) == 4 ? (le ? "<u4" : ">u4") : nullptr;
    } else if constexpr (std::is_same_v<C, float>) {
        return sizeof(float) == 4 ? (le ? "<f4" : ">f4") : nullptr;
    } else if constexpr (std::is_same_v<C, double>) {
        return sizeof(double) == 8 ? (le ? "<f8" : ">f8") : nullptr;
    } else {
        // long double is 64, 80 or 128 bits depending on the target and
        // numpy's longdouble is whatever its own build had. Refusing beats
        // agreeing on a size and disagreeing on the value. Enums, pointers
        // and nested structs land here too: each would need a dtype of its
        // own, which is a later job rather than a wrong answer now.
        return nullptr;
    }
}

// ============================================================================
// The layout, read off the type
// ============================================================================

namespace detail {

template<typename S>
consteval std::vector<std::meta::info> fields_of() {
    std::vector<std::meta::info> out;
    for (auto m : std::meta::nonstatic_data_members_of(
             ^^S, std::meta::access_context::current())) {
        out.push_back(m);
    }
    return out;
}

template<typename S>
consteval std::size_t field_count() {
    return fields_of<S>().size();
}

template<typename S, std::size_t I>
consteval std::meta::info field_at() {
    return fields_of<S>()[I];
}

// A bit-field has no byte offset of its own and an unnamed member cannot be a
// dtype field, so either one makes the struct undescribable rather than
// describable-but-wrong.
template<typename S>
consteval bool fields_are_addressable() {
    for (auto m : fields_of<S>()) {
        if (!std::meta::has_identifier(m)) return false;
        if (std::meta::is_bit_field(m)) return false;
        if (std::meta::offset_of(m).bits != 0) return false;
    }
    return true;
}

template<typename S, std::size_t I>
consteval const char* field_typestr() {
    return typestr<typename[:std::meta::type_of(field_at<S, I>()):]>();
}

template<typename S, std::size_t... Is>
consteval bool fields_are_nameable(std::index_sequence<Is...>) {
    return (... && (field_typestr<S, Is>() != nullptr));
}

template<typename S>
consteval bool all_fields_describable() {
    return fields_are_addressable<S>() &&
           fields_are_nameable<S>(std::make_index_sequence<field_count<S>()>{});
}

}  // namespace detail

// A struct whose bytes are a complete, self-describing account of its value.
//
// trivially_copyable rules out virtual functions, user-defined copy and move,
// and anything else whose value is not simply its bytes. standard_layout rules
// out mixed access control and the multiple-inheritance layouts, which is what
// makes an offset meaningful to a reader outside the program. An empty struct
// has no columns and numpy has no zero-itemsize dtype.
template<typename S>
concept ColumnarElement =
    std::is_class_v<S> &&
    std::is_trivially_copyable_v<S> &&
    std::is_standard_layout_v<S> &&
    (detail::field_count<S>() > 0) &&
    detail::all_fields_describable<S>();

namespace detail {

template<typename T>
struct is_vector : std::false_type {};
template<typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type {};

}  // namespace detail

// Exactly the condition under which a return comes back as a ColumnView. Both
// the conversion and the stub generator ask this one question, so a stub
// cannot claim a type the runtime does not produce.
template<typename T>
concept ColumnarReturn =
    detail::is_vector<std::remove_cvref_t<T>>::value &&
    ColumnarElement<typename std::remove_cvref_t<T>::value_type>;

// ============================================================================
// __array_interface__
// ============================================================================

// The `descr` list, as numpy's array interface wants it: one entry per member
// in offset order, plus an unnamed entry wherever the compiler left a hole.
//
// The padding entries are not optional. numpy derives itemsize by adding the
// entries up, so without them `struct Tick { double price; int32_t size; char
// side; }` describes itself as 13 bytes when it occupies 16, and the second
// element is read three bytes early. PEP 3118 format strings cannot express
// trailing padding at all, which is why this goes through the array interface
// rather than the buffer protocol.
template<typename S>
    requires ColumnarElement<S>
inline PyObject* build_descr() {
    PyObject* descr = PyList_New(0);
    if (!descr) return nullptr;

    std::size_t covered = 0;
    bool failed = false;

    auto add = [&](const char* name, const char* fmt) {
        if (failed) return;
        PyObject* entry = Py_BuildValue("(ss)", name, fmt);
        if (!entry || PyList_Append(descr, entry) < 0) failed = true;
        Py_XDECREF(entry);
    };

    auto pad_to = [&](std::size_t upto) {
        if (upto <= covered) return;
        char fmt[24];
        char name[24];
        std::snprintf(fmt, sizeof(fmt), "|V%zu", upto - covered);
        // Leaving the pad unnamed is not an option: numpy names an unnamed
        // entry f<index>, so a struct with a member actually called f1 gets
        // "field 'f1' occurs more than once" and np.asarray raises, a long way
        // from the cause. A leading space cannot begin a C++ identifier, so no
        // member name can collide with this one.
        std::snprintf(name, sizeof(name), " pad%zu", covered);
        add(name, fmt);
        covered = upto;
    };

    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr auto member = detail::field_at<S, Is>();
            constexpr std::size_t offset = std::meta::offset_of(member).bytes;
            constexpr std::size_t width =
                std::meta::size_of(std::meta::type_of(member));
            constexpr auto name = std::meta::identifier_of(member);

            pad_to(offset);
            add(name.data(), detail::field_typestr<S, Is>());
            covered = offset + width;
        }(), ...);
    }(std::make_index_sequence<detail::field_count<S>()>{});

    // Trailing padding, which is the case a hand-written descriptor forgets.
    pad_to(std::meta::size_of(^^S));

    if (failed) {
        Py_DECREF(descr);
        return nullptr;
    }
    return descr;
}

// The full dict. `data` is (address, read-only): read-only because the view
// hands out storage a C++ object owns, and a consumer writing through it would
// be mutating something Python does not own and cannot keep alive.
template<typename S>
    requires ColumnarElement<S>
inline PyObject* build_array_interface(const S* data, std::size_t count) {
    PyObject* descr = build_descr<S>();
    if (!descr) return nullptr;

    PyObject* shape = Py_BuildValue("(n)", static_cast<Py_ssize_t>(count));
    // An empty vector has no storage and data() is null, which numpy rejects:
    // it falls back to the sequence protocol and raises rather than returning
    // an empty array. It never dereferences the pointer for a zero-length
    // shape, so any non-null address will do, and the type's alignment is one
    // that cannot be mistaken for a real allocation.
    const auto address_bits =
        count ? reinterpret_cast<std::uintptr_t>(data)
              : static_cast<std::uintptr_t>(alignof(S));
    PyObject* address = Py_BuildValue(
        "(KO)", static_cast<unsigned long long>(address_bits), Py_True);

    PyObject* dict = nullptr;
    if (shape && address) {
        char itemsize[24];
        std::snprintf(itemsize, sizeof(itemsize), "|V%zu",
                      std::meta::size_of(^^S));
        // version, shape, typestr, descr, data. O rather than N throughout, so
        // the references are dropped once below whether this succeeded or not.
        dict = Py_BuildValue("{sisOsssOsO}",
                             "version", 3,
                             "shape", shape,
                             "typestr", itemsize,
                             "descr", descr,
                             "data", address);
    }

    Py_XDECREF(shape);
    Py_XDECREF(address);
    Py_DECREF(descr);
    return dict;
}

}  // namespace columnar
}  // namespace mirror_bridge

// ============================================================================
// The sequence view
// ============================================================================
//
// Returning a bare buffer would mean `pts[0].x` stops working, so this is a
// sequence that also describes its own memory. Indexing copies one element out
// on access, which is what the list held too -- mutating a list element never
// reached the C++ side either -- so indexing, iterating and len() behave as
// before and `list(pts)` reconstructs exactly the old result.
//
// element_to_python is a function pointer rather than a call to
// mirror_bridge::to_python, so this header stays below the binding header
// instead of including it back.

namespace mirror_bridge {
namespace columnar {

template<typename S>
struct ColumnViewObject {
    PyObject_HEAD
    std::vector<S>* data;
    PyObject* (*element_to_python)(const S&);
};

template<typename S>
void columnview_dealloc(PyObject* self) {
    auto* view = reinterpret_cast<ColumnViewObject<S>*>(self);
    delete view->data;
    Py_TYPE(self)->tp_free(self);
}

template<typename S>
Py_ssize_t columnview_length(PyObject* self) {
    auto* view = reinterpret_cast<ColumnViewObject<S>*>(self);
    return static_cast<Py_ssize_t>(view->data->size());
}

template<typename S>
PyObject* columnview_item(PyObject* self, Py_ssize_t index) {
    auto* view = reinterpret_cast<ColumnViewObject<S>*>(self);
    const auto count = static_cast<Py_ssize_t>(view->data->size());
    // sq_item receives an already-normalised index for a negative subscript,
    // but PySequence_GetItem can be called directly with anything.
    if (index < 0) index += count;
    if (index < 0 || index >= count) {
        PyErr_SetString(PyExc_IndexError, "index out of range");
        return nullptr;
    }
    return view->element_to_python((*view->data)[static_cast<std::size_t>(index)]);
}

template<typename S>
PyObject* columnview_array_interface(PyObject* self, void*) {
    auto* view = reinterpret_cast<ColumnViewObject<S>*>(self);
    return build_array_interface<S>(view->data->data(), view->data->size());
}

// A list supports pts[0:2], and sq_item cannot: it only ever sees an integer.
// Without this a slice is a TypeError, which is one of the things that made
// the view not quite a drop-in.
template<typename S>
PyObject* columnview_subscript(PyObject* self, PyObject* key) {
    auto* view = reinterpret_cast<ColumnViewObject<S>*>(self);
    const auto count = static_cast<Py_ssize_t>(view->data->size());

    if (PySlice_Check(key)) {
        Py_ssize_t start = 0, stop = 0, step = 0, length = 0;
        if (PySlice_GetIndicesEx(key, count, &start, &stop, &step, &length) < 0) {
            return nullptr;
        }
        PyObject* out = PyList_New(length);
        if (!out) return nullptr;
        for (Py_ssize_t i = 0, at = start; i < length; ++i, at += step) {
            PyObject* item =
                view->element_to_python((*view->data)[static_cast<std::size_t>(at)]);
            if (!item) {
                Py_DECREF(out);
                return nullptr;
            }
            PyList_SET_ITEM(out, i, item);
        }
        return out;
    }

    Py_ssize_t index = PyNumber_AsSsize_t(key, PyExc_IndexError);
    if (index == -1 && PyErr_Occurred()) {
        PyErr_Clear();
        PyErr_Format(PyExc_TypeError,
                     "indices must be integers or slices, not %s",
                     Py_TYPE(key)->tp_name);
        return nullptr;
    }
    return columnview_item<S>(self, index);
}

// Copy and pickle both go through __reduce__, and reconstructing as a list is
// the honest answer: a view is a window onto storage a C++ object owns, and a
// copy of it cannot be.
template<typename S>
PyObject* columnview_reduce(PyObject* self, PyObject*);

template<typename S>
PyObject* columnview_tolist(PyObject* self, PyObject*) {
    auto* view = reinterpret_cast<ColumnViewObject<S>*>(self);
    const auto count = static_cast<Py_ssize_t>(view->data->size());
    PyObject* list = PyList_New(count);
    if (!list) return nullptr;
    for (Py_ssize_t i = 0; i < count; ++i) {
        PyObject* item =
            view->element_to_python((*view->data)[static_cast<std::size_t>(i)]);
        if (!item) {
            Py_DECREF(list);
            return nullptr;
        }
        PyList_SET_ITEM(list, i, item);
    }
    return list;
}

template<typename S>
PyObject* columnview_repr(PyObject* self) {
    auto* view = reinterpret_cast<ColumnViewObject<S>*>(self);
    return PyUnicode_FromFormat("<%s of %zd %s>", Py_TYPE(self)->tp_name,
                                static_cast<Py_ssize_t>(view->data->size()),
                                std::meta::identifier_of(^^S).data());
}

template<typename S>
PyObject* columnview_reduce(PyObject* self, PyObject*) {
    PyObject* items = columnview_tolist<S>(self, nullptr);
    if (!items) return nullptr;
    PyObject* args = PyTuple_Pack(1, items);
    Py_DECREF(items);
    if (!args) return nullptr;
    PyObject* result = Py_BuildValue("(OO)", reinterpret_cast<PyObject*>(&PyList_Type), args);
    Py_DECREF(args);
    return result;
}

template<typename S>
PySequenceMethods columnview_as_sequence = {
    .sq_length = columnview_length<S>,
    .sq_item = columnview_item<S>,
};

template<typename S>
PyMappingMethods columnview_as_mapping = {
    .mp_length = columnview_length<S>,
    .mp_subscript = columnview_subscript<S>,
};

template<typename S>
PyMethodDef columnview_methods[] = {
    {"tolist", columnview_tolist<S>, METH_NOARGS,
     "Return the elements as a list, which is what this used to be."},
    {"__reduce__", columnview_reduce<S>, METH_NOARGS,
     "Reconstruct as a list, so copy and pickle work."},
    {nullptr, nullptr, 0, nullptr},
};

template<typename S>
PyGetSetDef columnview_getset[] = {
    {const_cast<char*>("__array_interface__"), columnview_array_interface<S>,
     nullptr, const_cast<char*>("numpy's array interface, built from the "
                                "struct's reflected layout."),
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr},
};

template<typename S>
PyTypeObject* get_columnview_type() {
    // One name per element type, so a repr and a traceback say which.
    static const std::string name =
        std::string("mirror_bridge.ColumnView[") +
        std::string(std::meta::identifier_of(^^S)) + "]";

    static PyTypeObject type_object = {
        .ob_base = PyVarObject_HEAD_INIT(nullptr, 0)
        .tp_name = name.c_str(),
        .tp_basicsize = sizeof(ColumnViewObject<S>),
        .tp_itemsize = 0,
        .tp_dealloc = columnview_dealloc<S>,
        .tp_repr = columnview_repr<S>,
        .tp_as_sequence = &columnview_as_sequence<S>,
        .tp_as_mapping = &columnview_as_mapping<S>,
        // SEQUENCE is what lets `match pts: case [a, b, c]` match. Without it
        // a structural pattern silently takes a different branch, which is a
        // worse failure than the TypeError a missing slice gives.
        .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_SEQUENCE,
        .tp_doc = "A sequence of C++ structs that also describes its own "
                  "memory, so numpy can read it without copying.",
        .tp_methods = columnview_methods<S>,
        .tp_getset = columnview_getset<S>,
    };

    static bool ready = false;
    if (!ready) {
        if (PyType_Ready(&type_object) < 0) return nullptr;

        // Py_TPFLAGS_SEQUENCE makes `match` work but says nothing to
        // collections.abc, and isinstance(pts, Sequence) is the ordinary way
        // to ask "can I index and iterate this". Registering is the only way
        // to answer it truthfully for a type that is not a list subclass.
        // A failure here costs the isinstance answer and nothing else, so it
        // is cleared rather than propagated.
        if (PyObject* abc = PyImport_ImportModule("collections.abc")) {
            if (PyObject* seq = PyObject_GetAttrString(abc, "Sequence")) {
                PyObject* reg = PyObject_CallMethod(
                    seq, "register", "O", reinterpret_cast<PyObject*>(&type_object));
                Py_XDECREF(reg);
                Py_DECREF(seq);
            }
            Py_DECREF(abc);
        }
        PyErr_Clear();
        ready = true;
    }
    return &type_object;
}

// Takes the vector by value and keeps it: the array interface hands out a
// pointer into this storage, so it has to outlive the call and die with the
// view.
template<ColumnarElement S>
PyObject* to_columnar(std::vector<S> data,
                      PyObject* (*element_to_python)(const S&)) {
    PyTypeObject* type = get_columnview_type<S>();
    if (!type) return nullptr;

    auto* view = reinterpret_cast<ColumnViewObject<S>*>(type->tp_alloc(type, 0));
    if (!view) return nullptr;

    view->data = new (std::nothrow) std::vector<S>(std::move(data));
    if (!view->data) {
        Py_DECREF(view);
        return PyErr_NoMemory();
    }
    view->element_to_python = element_to_python;
    return reinterpret_cast<PyObject*>(view);
}

}  // namespace columnar
}  // namespace mirror_bridge
