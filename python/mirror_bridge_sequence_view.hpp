#pragma once

// ============================================================================
// Mirror Bridge - Live views over std::vector data members
// ============================================================================
//
// A std::vector data member used to cross as a fresh array.array or list on
// every read. That is wrong twice over:
//
//   b.items.append(99)   # accepted, and silently discarded
//   b.items[0] = 42      # accepted, and silently discarded
//
// and it allocates and fills a whole new sequence for every attribute read,
// so walking a member vector of a million doubles costs 8 MB of copying each
// time somebody names it.
//
// This is the container half of what PR #32 did for class members. There,
// `o.leaf.x = 7` writes through because the member is handed back as a view
// onto the parent's storage rather than a copy. Interiority is known by
// construction for a non-static data member, so the same reasoning applies to
// a vector member and only the proxy type is different: a vector needs the
// sequence protocol where a class needed the attribute protocol.
//
// What the view deliberately is not:
//
// - It is not used for method returns. Interiority cannot be established for
//   one, so to_python keeps copying there and tests/bulk_transfer still gets
//   its array.array. Only py_getter reaches this.
// - It is not used for a const member. A view of one would hand Python a
//   writable reference to something C++ declared it may not change, which is
//   the rule is_viewable_member_v already applies to class members.
// - It is not a list. list(v), v.tolist() and v[:] each give a real copy, and
//   the view registers as collections.abc.MutableSequence so the stub can say
//   so and isinstance agrees.
//
// Lifetime: the view holds a strong reference to the Python object that owns
// the storage, exactly as a member view does, so the parent cannot be
// collected while a view of its member is alive.
//
// Invalidation: append/insert/clear can reallocate, which dangles any element
// view taken earlier (`p = b.points[0]; b.points.append(q); p.x`) in the same
// way the underlying C++ reference would. pybind11's bind_vector has the same
// hazard and documents it. Buffers are the exception and are not left to
// documentation: an exported Py_buffer pins the vector against resizing, so
// numpy cannot be left pointing at freed storage.

#include <Python.h>
#include <vector>
#include <algorithm>
#include <type_traits>
#include <unordered_map>

namespace mirror_bridge {

// is_viewable_vector_v and is_viewable_sequence_member_v are declared beside
// is_viewable_member_v in mirror_bridge_python.hpp. py_getter has to test
// them and is defined long before this header is included, and a trait that
// decides which members get a view belongs next to the one that already
// decides it for class members -- the two have to stay in step.

template<typename C>
struct PySequenceView {
    PyObject_HEAD
    C* container;
    PyObject* parent;       // strong reference: owner of the storage

    // Per-view rather than per-type. Two consumers can hold a buffer on the
    // same vector at once, and a shared shape array means the second export
    // rewrites what the first one reported -- that exact bug was found and
    // fixed once already in mirror_bridge_buffer.hpp.
    Py_ssize_t shape[1];
    Py_ssize_t strides[1];
};

namespace seqview {

template<typename C>
inline PySequenceView<C>* self_of(PyObject* self) {
    return reinterpret_cast<PySequenceView<C>*>(self);
}

// Every entry point goes through this. A view whose parent has been torn down
// is not reachable through normal use -- the strong reference prevents it --
// but a view is an ordinary Python object and C++ code holding the vector can
// still outlive its own assumptions, so the null check is cheap insurance.
template<typename C>
inline C* vec(PyObject* self) {
    auto* v = self_of<C>(self);
    if (!v->container) {
        PyErr_SetString(PyExc_RuntimeError,
                        "this view's C++ container is gone");
        return nullptr;
    }
    return v->container;
}

// Outstanding Py_buffer exports, keyed by the address of the C++ container.
//
// Not a counter on the view object, which is what this was first written as
// and which does not work: every attribute read builds a *new* view of the
// same vector, so `mv = memoryview(b.coords)` exports from one object and
// `b.coords.append(x)` asks a different one whether resizing is safe. It
// always said yes, and the append then reallocated the storage numpy was
// still pointing into. Keying on the container is what makes the question
// mean "is anyone looking at this vector", which is the question being asked.
//
// Every caller holds the GIL -- bf_getbuffer, bf_releasebuffer and the
// mutators are all called from the interpreter -- so the map needs no lock.
inline std::unordered_map<const void*, Py_ssize_t>& buffer_export_counts() {
    static std::unordered_map<const void*, Py_ssize_t> counts;
    return counts;
}

inline void note_buffer_exported(const void* container) {
    ++buffer_export_counts()[container];
}

inline void note_buffer_released(const void* container) {
    auto& counts = buffer_export_counts();
    auto it = counts.find(container);
    if (it == counts.end()) return;
    if (--it->second <= 0) counts.erase(it);
}

inline bool container_is_pinned(const void* container) {
    auto& counts = buffer_export_counts();
    auto it = counts.find(container);
    return it != counts.end() && it->second > 0;
}

template<typename C>
bool resize_allowed(PyObject* self) {
    if (container_is_pinned(self_of<C>(self)->container)) {
        PyErr_SetString(PyExc_BufferError,
                        "cannot resize: a memoryview or numpy array is still "
                        "pointing at this data");
        return false;
    }
    return true;
}

template<typename C>
void dealloc(PyObject* self) {
    auto* v = self_of<C>(self);
    Py_CLEAR(v->parent);
    Py_TYPE(self)->tp_free(self);
}

template<typename C>
Py_ssize_t length(PyObject* self) {
    C* c = vec<C>(self);
    if (!c) return -1;
    return static_cast<Py_ssize_t>(c->size());
}

// Index normalisation shared by every indexed entry point. Returns false with
// the error already set.
template<typename C>
bool normalize(PyObject* self, Py_ssize_t& i, C*& c) {
    c = vec<C>(self);
    if (!c) return false;
    const auto n = static_cast<Py_ssize_t>(c->size());
    if (i < 0) i += n;
    if (i < 0 || i >= n) {
        PyErr_SetString(PyExc_IndexError, "index out of range");
        return false;
    }
    return true;
}

template<typename C>
PyObject* item(PyObject* self, Py_ssize_t i) {
    using ValueType = typename C::value_type;
    C* c;
    if (!normalize<C>(self, i, c)) return nullptr;

    // A class element is itself handed back as a view, with this view as its
    // parent, so `b.points[0].x = 5` writes into the vector. The view holds
    // the real owner, so the whole chain stays alive.
    if constexpr (is_viewable_member_v<ValueType>) {
        return to_python_member_view((*c)[i], self);
    } else {
        return to_python((*c)[i]);
    }
}

template<typename C>
int ass_item(PyObject* self, Py_ssize_t i, PyObject* value) {
    using ValueType = typename C::value_type;
    C* c;
    if (!normalize<C>(self, i, c)) return -1;

    if (!value) {
        if (!resize_allowed<C>(self)) return -1;
        c->erase(c->begin() + i);
        return 0;
    }

    ValueType tmp;
    if (!from_python(value, tmp)) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(PyExc_TypeError,
                            "cannot convert this value to the vector's "
                            "element type");
        }
        return -1;
    }
    (*c)[i] = std::move(tmp);
    return 0;
}

// A slice gives a real list, the way a list slice gives a real list. It is a
// copy on purpose: a view of a strided subrange would need stride-aware
// element access everywhere for a case nobody has asked for, and `v[:]` being
// a snapshot is the documented way to take one.
template<typename C>
PyObject* subscript(PyObject* self, PyObject* key) {
    C* c = vec<C>(self);
    if (!c) return nullptr;

    if (PySlice_Check(key)) {
        Py_ssize_t start, stop, step;
        if (PySlice_Unpack(key, &start, &stop, &step) < 0) return nullptr;
        const Py_ssize_t count = PySlice_AdjustIndices(
            static_cast<Py_ssize_t>(c->size()), &start, &stop, step);

        PyObject* out = PyList_New(count);
        if (!out) return nullptr;
        for (Py_ssize_t k = 0, idx = start; k < count; ++k, idx += step) {
            PyObject* elem = item<C>(self, idx);
            if (!elem) {
                Py_DECREF(out);
                return nullptr;
            }
            PyList_SET_ITEM(out, k, elem);
        }
        return out;
    }

    Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
        PyErr_Clear();
        PyErr_Format(PyExc_TypeError,
                     "vector indices must be integers or slices, not %s",
                     Py_TYPE(key)->tp_name);
        return nullptr;
    }
    return item<C>(self, i);
}

template<typename C>
int ass_subscript(PyObject* self, PyObject* key, PyObject* value) {
    if (PySlice_Check(key)) {
        PyErr_SetString(PyExc_TypeError,
                        "slice assignment is not supported on a live vector "
                        "view; assign the whole attribute instead "
                        "(obj.field = [...])");
        return -1;
    }
    Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
        PyErr_Clear();
        PyErr_Format(PyExc_TypeError,
                     "vector indices must be integers, not %s",
                     Py_TYPE(key)->tp_name);
        return -1;
    }
    return ass_item<C>(self, i, value);
}

template<typename C>
int contains(PyObject* self, PyObject* target) {
    C* c = vec<C>(self);
    if (!c) return -1;
    const auto n = static_cast<Py_ssize_t>(c->size());
    for (Py_ssize_t i = 0; i < n; ++i) {
        PyObject* elem = item<C>(self, i);
        if (!elem) return -1;
        const int eq = PyObject_RichCompareBool(elem, target, Py_EQ);
        Py_DECREF(elem);
        if (eq < 0) return -1;
        if (eq) return 1;
    }
    return 0;
}

// Equality against any sequence, so code that compared a member against a
// literal list keeps working. Ordering is left to NotImplemented: a vector
// view is not a list and silently ordering one against another type is the
// kind of convenience that hides a bug.
template<typename C>
PyObject* richcompare(PyObject* self, PyObject* other, int op) {
    if (op != Py_EQ && op != Py_NE) Py_RETURN_NOTIMPLEMENTED;

    C* c = vec<C>(self);
    if (!c) return nullptr;

    // NotImplemented rather than False, so Python still gets to ask the other
    // operand. str and bytes are sequences and are excluded on purpose: a
    // vector<string> holding ['a', 'b'] comparing equal to "ab" is the kind
    // of accident being permissive buys.
    if (!PySequence_Check(other) || PyUnicode_Check(other) || PyBytes_Check(other)) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    const Py_ssize_t mine = static_cast<Py_ssize_t>(c->size());
    const Py_ssize_t theirs = PySequence_Size(other);
    if (theirs < 0) return nullptr;

    bool equal = (mine == theirs);
    for (Py_ssize_t i = 0; equal && i < mine; ++i) {
        PyObject* a = item<C>(self, i);
        if (!a) return nullptr;
        PyObject* b = PySequence_GetItem(other, i);
        if (!b) {
            Py_DECREF(a);
            return nullptr;
        }
        const int cmp = PyObject_RichCompareBool(a, b, Py_EQ);
        Py_DECREF(a);
        Py_DECREF(b);
        if (cmp < 0) return nullptr;
        equal = (cmp == 1);
    }

    const bool result = (op == Py_EQ) ? equal : !equal;
    if (result) Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

template<typename C>
PyObject* repr(PyObject* self) {
    C* c = vec<C>(self);
    if (!c) return nullptr;

    PyObject* as_list = PyList_New(0);
    if (!as_list) return nullptr;
    const auto n = static_cast<Py_ssize_t>(c->size());
    for (Py_ssize_t i = 0; i < n; ++i) {
        PyObject* elem = item<C>(self, i);
        if (!elem || PyList_Append(as_list, elem) < 0) {
            Py_XDECREF(elem);
            Py_DECREF(as_list);
            return nullptr;
        }
        Py_DECREF(elem);
    }
    PyObject* text = PyObject_Repr(as_list);
    Py_DECREF(as_list);
    return text;
}

template<typename C>
PyObject* method_tolist(PyObject* self, PyObject*) {
    C* c = vec<C>(self);
    if (!c) return nullptr;
    PyObject* out = PyList_New(static_cast<Py_ssize_t>(c->size()));
    if (!out) return nullptr;
    const auto n = static_cast<Py_ssize_t>(c->size());
    for (Py_ssize_t i = 0; i < n; ++i) {
        PyObject* elem = item<C>(self, i);
        if (!elem) {
            Py_DECREF(out);
            return nullptr;
        }
        PyList_SET_ITEM(out, i, elem);
    }
    return out;
}

template<typename C>
PyObject* method_append(PyObject* self, PyObject* value) {
    using ValueType = typename C::value_type;
    C* c = vec<C>(self);
    if (!c) return nullptr;
    if (!resize_allowed<C>(self)) return nullptr;

    if constexpr (std::is_default_constructible_v<ValueType>) {
        ValueType tmp;
        if (!from_python(value, tmp)) {
            if (!PyErr_Occurred()) {
                PyErr_SetString(PyExc_TypeError,
                                "cannot convert this value to the vector's "
                                "element type");
            }
            return nullptr;
        }
        c->push_back(std::move(tmp));
        Py_RETURN_NONE;
    } else {
        // from_python converts into an existing object, so an element type
        // with no default constructor has nothing to convert into. Reading
        // and indexing still work; only growth is refused, and saying which
        // type it is beats a generic failure.
        PyErr_Format(PyExc_TypeError,
                     "cannot append: this vector's element type is not "
                     "default-constructible, so there is nothing to convert "
                     "into");
        return nullptr;
    }
}

template<typename C>
PyObject* method_extend(PyObject* self, PyObject* iterable) {
    PyObject* iter = PyObject_GetIter(iterable);
    if (!iter) return nullptr;
    PyObject* next;
    while ((next = PyIter_Next(iter)) != nullptr) {
        PyObject* ok = method_append<C>(self, next);
        Py_DECREF(next);
        if (!ok) {
            Py_DECREF(iter);
            return nullptr;
        }
        Py_DECREF(ok);
    }
    Py_DECREF(iter);
    if (PyErr_Occurred()) return nullptr;
    Py_RETURN_NONE;
}

template<typename C>
PyObject* method_insert(PyObject* self, PyObject* const* args, Py_ssize_t nargs) {
    using ValueType = typename C::value_type;
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError, "insert() takes exactly 2 arguments");
        return nullptr;
    }
    C* c = vec<C>(self);
    if (!c) return nullptr;
    if (!resize_allowed<C>(self)) return nullptr;

    Py_ssize_t i = PyNumber_AsSsize_t(args[0], PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) return nullptr;

    // list.insert clamps rather than raising, and a view that behaved
    // differently would surprise anyone who reached for it by habit.
    const auto n = static_cast<Py_ssize_t>(c->size());
    if (i < 0) i += n;
    if (i < 0) i = 0;
    if (i > n) i = n;

    if constexpr (std::is_default_constructible_v<ValueType>) {
        ValueType tmp;
        if (!from_python(args[1], tmp)) {
            if (!PyErr_Occurred()) {
                PyErr_SetString(PyExc_TypeError,
                                "cannot convert this value to the vector's "
                                "element type");
            }
            return nullptr;
        }
        c->insert(c->begin() + i, std::move(tmp));
        Py_RETURN_NONE;
    } else {
        PyErr_SetString(PyExc_TypeError,
                        "cannot insert: this vector's element type is not "
                        "default-constructible");
        return nullptr;
    }
}

template<typename C>
PyObject* method_pop(PyObject* self, PyObject* const* args, Py_ssize_t nargs) {
    if (nargs > 1) {
        PyErr_SetString(PyExc_TypeError, "pop() takes at most 1 argument");
        return nullptr;
    }
    C* c = vec<C>(self);
    if (!c) return nullptr;
    if (c->empty()) {
        PyErr_SetString(PyExc_IndexError, "pop from empty vector");
        return nullptr;
    }
    if (!resize_allowed<C>(self)) return nullptr;

    Py_ssize_t i = static_cast<Py_ssize_t>(c->size()) - 1;
    if (nargs == 1) {
        i = PyNumber_AsSsize_t(args[0], PyExc_IndexError);
        if (i == -1 && PyErr_Occurred()) return nullptr;
    }
    C* ignored;
    if (!normalize<C>(self, i, ignored)) return nullptr;

    // The element is copied out before it is erased: handing back a view of a
    // slot that is about to be destroyed is exactly the dangling read this
    // whole header is trying to remove.
    PyObject* out = to_python((*c)[i]);
    if (!out) return nullptr;
    c->erase(c->begin() + i);
    return out;
}

template<typename C>
PyObject* method_clear(PyObject* self, PyObject*) {
    C* c = vec<C>(self);
    if (!c) return nullptr;
    if (!resize_allowed<C>(self)) return nullptr;
    c->clear();
    Py_RETURN_NONE;
}

// collections.abc.MutableSequence.register() makes isinstance say yes; it
// does not bring the ABC's mixin methods with it, the way inheriting would.
// So the ones a MutableSequence is expected to have are implemented here
// rather than left to raise AttributeError against a .pyi that promises
// them -- a stub that approves a call which cannot work is the defect this
// project keeps finding in itself.

template<typename C>
PyObject* method_index(PyObject* self, PyObject* const* args, Py_ssize_t nargs) {
    C* c = vec<C>(self);
    if (!c) return nullptr;
    if (nargs < 1 || nargs > 3) {
        PyErr_SetString(PyExc_TypeError, "index() takes 1 to 3 arguments");
        return nullptr;
    }
    const auto n = static_cast<Py_ssize_t>(c->size());
    Py_ssize_t start = 0, stop = n;
    if (nargs >= 2) {
        start = PyNumber_AsSsize_t(args[1], PyExc_IndexError);
        if (start == -1 && PyErr_Occurred()) return nullptr;
        if (start < 0) start = (start + n < 0) ? 0 : start + n;
    }
    if (nargs == 3) {
        stop = PyNumber_AsSsize_t(args[2], PyExc_IndexError);
        if (stop == -1 && PyErr_Occurred()) return nullptr;
        if (stop < 0) stop += n;
    }
    if (stop > n) stop = n;

    for (Py_ssize_t i = start; i < stop; ++i) {
        PyObject* elem = item<C>(self, i);
        if (!elem) return nullptr;
        const int eq = PyObject_RichCompareBool(elem, args[0], Py_EQ);
        Py_DECREF(elem);
        if (eq < 0) return nullptr;
        if (eq) return PyLong_FromSsize_t(i);
    }
    PyErr_SetString(PyExc_ValueError, "value is not in the vector");
    return nullptr;
}

template<typename C>
PyObject* method_count(PyObject* self, PyObject* target) {
    C* c = vec<C>(self);
    if (!c) return nullptr;
    Py_ssize_t found = 0;
    const auto n = static_cast<Py_ssize_t>(c->size());
    for (Py_ssize_t i = 0; i < n; ++i) {
        PyObject* elem = item<C>(self, i);
        if (!elem) return nullptr;
        const int eq = PyObject_RichCompareBool(elem, target, Py_EQ);
        Py_DECREF(elem);
        if (eq < 0) return nullptr;
        found += eq;
    }
    return PyLong_FromSsize_t(found);
}

template<typename C>
PyObject* method_remove(PyObject* self, PyObject* target) {
    C* c = vec<C>(self);
    if (!c) return nullptr;
    const auto n = static_cast<Py_ssize_t>(c->size());
    for (Py_ssize_t i = 0; i < n; ++i) {
        PyObject* elem = item<C>(self, i);
        if (!elem) return nullptr;
        const int eq = PyObject_RichCompareBool(elem, target, Py_EQ);
        Py_DECREF(elem);
        if (eq < 0) return nullptr;
        if (eq) {
            if (!resize_allowed<C>(self)) return nullptr;
            c->erase(c->begin() + i);
            Py_RETURN_NONE;
        }
    }
    PyErr_SetString(PyExc_ValueError, "value is not in the vector");
    return nullptr;
}

template<typename C>
PyObject* method_reverse(PyObject* self, PyObject*) {
    C* c = vec<C>(self);
    if (!c) return nullptr;
    // Reversing in place neither grows nor shrinks the vector, so it does
    // not invalidate an exported buffer and is allowed while one is held.
    std::reverse(c->begin(), c->end());
    Py_RETURN_NONE;
}

template<typename C>
PyObject* inplace_concat(PyObject* self, PyObject* other) {
    PyObject* ok = method_extend<C>(self, other);
    if (!ok) return nullptr;
    Py_DECREF(ok);
    Py_INCREF(self);
    return self;
}

// The reason a numeric member view is worth having at all: numpy reads the
// vector's own storage instead of a freshly built array.array. Only offered
// where the element is one crosses_as_array already vouches for, so the
// buffer and the stub cannot disagree about which members are numeric.
template<typename C>
int getbuffer(PyObject* self, Py_buffer* view, int flags) {
    using ValueType = typename C::value_type;
    auto* v = self_of<C>(self);
    C* c = vec<C>(self);
    if (!c) return -1;

    view->readonly = 0;
    view->obj = self;
    Py_INCREF(self);
    // An empty vector's data() is allowed to be null, and a Py_buffer whose
    // buf is null trips assertions inside a debug CPython even when len is 0.
    // Pointing at this object instead is harmless: nothing may be read
    // through a zero-length buffer.
    view->buf = c->empty() ? static_cast<void*>(self)
                           : static_cast<void*>(c->data());
    view->len = static_cast<Py_ssize_t>(c->size() * sizeof(ValueType));
    view->itemsize = static_cast<Py_ssize_t>(sizeof(ValueType));
    view->ndim = 1;
    view->format = (flags & PyBUF_FORMAT)
                       ? const_cast<char*>(get_buffer_format<ValueType>())
                       : nullptr;
    v->shape[0] = static_cast<Py_ssize_t>(c->size());
    v->strides[0] = static_cast<Py_ssize_t>(sizeof(ValueType));
    view->shape = (flags & PyBUF_ND) ? v->shape : nullptr;
    view->strides = (flags & PyBUF_STRIDES) ? v->strides : nullptr;
    view->suboffsets = nullptr;
    view->internal = nullptr;

    note_buffer_exported(v->container);
    return 0;
}

template<typename C>
void releasebuffer(PyObject* self, Py_buffer*) {
    note_buffer_released(self_of<C>(self)->container);
}

}  // namespace seqview

// Slot tables and the type object, one instantiation per container type.
// Built lazily on first use rather than at bind_class time: a module can bind
// a class whose vector members are never read, and PyType_Ready is not free.

template<typename C>
PyMethodDef* sequence_view_methods() {
    static PyMethodDef methods[] = {
        {"append", (PyCFunction)seqview::method_append<C>, METH_O,
         "Append a value to the underlying C++ vector."},
        {"extend", (PyCFunction)seqview::method_extend<C>, METH_O,
         "Append every item of an iterable to the underlying C++ vector."},
        {"insert", (PyCFunction)seqview::method_insert<C>, METH_FASTCALL,
         "Insert a value before the given index."},
        {"pop", (PyCFunction)seqview::method_pop<C>, METH_FASTCALL,
         "Remove and return the item at the given index (default last)."},
        {"clear", (PyCFunction)seqview::method_clear<C>, METH_NOARGS,
         "Remove every element from the underlying C++ vector."},
        {"index", (PyCFunction)seqview::method_index<C>, METH_FASTCALL,
         "Return the first index of a value, optionally within [start, stop)."},
        {"count", (PyCFunction)seqview::method_count<C>, METH_O,
         "Return how many elements equal a value."},
        {"remove", (PyCFunction)seqview::method_remove<C>, METH_O,
         "Remove the first element equal to a value."},
        {"reverse", (PyCFunction)seqview::method_reverse<C>, METH_NOARGS,
         "Reverse the underlying C++ vector in place."},
        {"tolist", (PyCFunction)seqview::method_tolist<C>, METH_NOARGS,
         "Return a plain list copy of the current contents."},
        {nullptr, nullptr, 0, nullptr}
    };
    return methods;
}

template<typename C>
PySequenceMethods* sequence_view_as_sequence() {
    static PySequenceMethods slots = [] {
        PySequenceMethods s{};
        s.sq_length = seqview::length<C>;
        s.sq_item = seqview::item<C>;
        s.sq_ass_item = seqview::ass_item<C>;
        s.sq_contains = seqview::contains<C>;
        s.sq_inplace_concat = seqview::inplace_concat<C>;
        return s;
    }();
    return &slots;
}

template<typename C>
PyMappingMethods* sequence_view_as_mapping() {
    static PyMappingMethods slots = [] {
        PyMappingMethods s{};
        s.mp_length = seqview::length<C>;
        s.mp_subscript = seqview::subscript<C>;
        s.mp_ass_subscript = seqview::ass_subscript<C>;
        return s;
    }();
    return &slots;
}

template<typename C>
PyBufferProcs* sequence_view_as_buffer() {
    static PyBufferProcs slots = [] {
        PyBufferProcs s{};
        s.bf_getbuffer = seqview::getbuffer<C>;
        s.bf_releasebuffer = seqview::releasebuffer<C>;
        return s;
    }();
    return &slots;
}

// Tell collections.abc that this really is a MutableSequence. The stub says
// MutableSequence[T] for a vector member, so isinstance has to agree with it
// or the annotation is the same kind of lie the .pyi used to tell about
// array.array. Failure is not fatal: the view works either way, and a missing
// registration is worth less than refusing to hand back the member at all.
inline void register_as_mutable_sequence(PyTypeObject* type) {
    PyObject* abc = PyImport_ImportModule("collections.abc");
    if (!abc) {
        PyErr_Clear();
        return;
    }
    PyObject* base = PyObject_GetAttrString(abc, "MutableSequence");
    Py_DECREF(abc);
    if (!base) {
        PyErr_Clear();
        return;
    }
    PyObject* result = PyObject_CallMethod(base, "register", "O",
                                           reinterpret_cast<PyObject*>(type));
    Py_DECREF(base);
    Py_XDECREF(result);
    if (!result) PyErr_Clear();
}

template<typename C>
PyTypeObject* sequence_view_type() {
    static PyTypeObject type_object = {
        // Designated ob_base keeps every clause designated: GCC rejects
        // mixing the positional HEAD_INIT macro with designated fields.
        .ob_base = PyVarObject_HEAD_INIT(nullptr, 0)
        .tp_name = crosses_as_array_v<C> ? "mirror_bridge.NumericVectorView"
                                         : "mirror_bridge.VectorView",
        .tp_basicsize = sizeof(PySequenceView<C>),
        .tp_itemsize = 0,
        .tp_dealloc = seqview::dealloc<C>,
        .tp_repr = seqview::repr<C>,
        .tp_as_sequence = sequence_view_as_sequence<C>(),
        .tp_as_mapping = sequence_view_as_mapping<C>(),
        // Declaration order matters: GCC rejects designators that do not
        // follow PyTypeObject's own field order, where tp_hash sits between
        // tp_as_mapping and tp_as_buffer. clang accepts any order, so this
        // only shows up on the other compiler.
        //
        // Mutable, so unhashable, the same choice list makes. Leaving it
        // inherited would be worse than merely wrong: every attribute read
        // builds a new view, so an identity hash would differ between two
        // reads of the same member.
        .tp_hash = PyObject_HashNotImplemented,
        .tp_as_buffer = crosses_as_array_v<C> ? sequence_view_as_buffer<C>()
                                              : nullptr,
        .tp_flags = Py_TPFLAGS_DEFAULT,
        .tp_doc = "A live view of a C++ std::vector data member. Writes go "
                  "straight into the C++ object; list(v) or v.tolist() takes "
                  "a copy.",
        .tp_richcompare = seqview::richcompare<C>,
        // Index-based iteration, which is what makes mutation during a loop
        // raise IndexError or stop short instead of walking an iterator the
        // C++ side has already invalidated.
        .tp_iter = PySeqIter_New,
        .tp_methods = sequence_view_methods<C>(),
    };

    static bool ready = false;
    if (!ready) {
        if (PyType_Ready(&type_object) < 0) return nullptr;
        register_as_mutable_sequence(&type_object);
        ready = true;
    }
    return &type_object;
}

// Hand back a live view of `ref` that keeps `parent` alive, the sequence
// counterpart of to_python_member_view.
template<typename C>
PyObject* to_python_sequence_view(C& ref, PyObject* parent) {
    PyTypeObject* type = sequence_view_type<C>();
    if (!type) {
        // No view type means no view. The copy is what this returned before
        // and is still a correct answer, just not a live one.
        PyErr_Clear();
        return to_python(ref);
    }

    auto* view = reinterpret_cast<PySequenceView<C>*>(type->tp_alloc(type, 0));
    if (!view) return nullptr;

    view->container = std::addressof(ref);
    view->shape[0] = 0;
    view->strides[0] = 0;
    Py_INCREF(parent);
    view->parent = parent;
    return reinterpret_cast<PyObject*>(view);
}

}  // namespace mirror_bridge
