// Does the layout read off a struct by reflection describe it to numpy
// exactly? "Exactly" means: the same field offsets the compiler chose, the
// same itemsize including trailing padding, and values that read back equal.
//
// A standalone program rather than a binding, because what it checks is the
// descriptor, not a bound method. Run through tests/columnar/test_columnar.sh.

#include "python/mirror_bridge_columnar.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("  %s %s\n", ok ? "\xE2\x9C\x93" : "\xE2\x9C\x97", what);
    if (!ok) ++failures;
}

// No padding at all.
struct Point3 {
    double x, y, z;
};

// Internal and trailing padding: 8 + 4 + 1 used of 16 stored. This is the
// shape a PEP 3118 format string cannot express.
struct Tick {
    double price;
    std::int32_t size;
    char side;
};

// A hole in the middle, which a descriptor that only pads at the end gets
// wrong: 1 byte, 7 of padding, 8, = 16.
struct Flagged {
    char flag;
    double value;
};

struct Mixed {
    std::uint16_t a;
    std::int64_t b;
    float c;
    bool d;
};

// Not describable, each for its own reason.
struct WithString {
    double x;
    const char* name;
};
struct Packed {
    unsigned int bits : 7;
    unsigned int rest : 25;
};
struct Empty {};
struct Nested {
    Point3 p;
};
struct Virtual {
    virtual ~Virtual() = default;
    double x;
};

static_assert(mirror_bridge::columnar::ColumnarElement<Point3>);
static_assert(mirror_bridge::columnar::ColumnarElement<Tick>);
static_assert(mirror_bridge::columnar::ColumnarElement<Flagged>);
static_assert(mirror_bridge::columnar::ColumnarElement<Mixed>);
static_assert(!mirror_bridge::columnar::ColumnarElement<WithString>,
              "a pointer member has no numpy spelling");
static_assert(!mirror_bridge::columnar::ColumnarElement<Packed>,
              "a bit-field has no byte offset of its own");
static_assert(!mirror_bridge::columnar::ColumnarElement<Empty>,
              "numpy has no zero-itemsize dtype");
static_assert(!mirror_bridge::columnar::ColumnarElement<Nested>,
              "a nested struct would need a dtype of its own");
static_assert(!mirror_bridge::columnar::ColumnarElement<Virtual>,
              "a polymorphic class is not its bytes");
static_assert(!mirror_bridge::columnar::ColumnarElement<double>,
              "the concept is about structs");

// Asking numpy what it made of the descriptor, rather than asserting on the
// descriptor's own text.
PyObject* as_array(PyObject* interface) {
    PyObject* numpy = PyImport_ImportModule("numpy");
    if (!numpy) return nullptr;

    // A bare object carrying __array_interface__ is all numpy needs.
    PyObject* types = PyImport_ImportModule("types");
    PyObject* holder = PyObject_CallMethod(types, "SimpleNamespace", nullptr);
    Py_DECREF(types);
    if (!holder) { Py_DECREF(numpy); return nullptr; }
    if (PyObject_SetAttrString(holder, "__array_interface__", interface) < 0) {
        Py_DECREF(holder);
        Py_DECREF(numpy);
        return nullptr;
    }
    PyObject* arr = PyObject_CallMethod(numpy, "asarray", "O", holder);
    Py_DECREF(holder);
    Py_DECREF(numpy);
    return arr;
}

// For an object that already carries __array_interface__, which the view does.
// This matters for ownership: numpy sets .base to whatever it was handed, so
// handing it a holder would hide whether the view is kept alive.
PyObject* as_array_direct(PyObject* obj) {
    PyObject* numpy = PyImport_ImportModule("numpy");
    if (!numpy) return nullptr;
    PyObject* arr = PyObject_CallMethod(numpy, "asarray", "O", obj);
    Py_DECREF(numpy);
    return arr;
}

long long int_attr(PyObject* obj, const char* path) {
    PyObject* v = PyObject_GetAttrString(obj, path);
    if (!v) { PyErr_Clear(); return -1; }
    long long n = PyLong_AsLongLong(v);
    Py_DECREF(v);
    return n;
}

template <typename S>
void check_layout(const char* label, const std::vector<S>& data) {
    std::printf("%s (sizeof = %zu):\n", label, sizeof(S));

    PyObject* iface = mirror_bridge::columnar::build_array_interface<S>(
        data.data(), data.size());
    if (!iface) { PyErr_Print(); ++failures; return; }

    PyObject* arr = as_array(iface);
    Py_DECREF(iface);
    if (!arr) { PyErr_Print(); ++failures; return; }

    PyObject* dtype = PyObject_GetAttrString(arr, "dtype");
    check(int_attr(dtype, "itemsize") == static_cast<long long>(sizeof(S)),
          "itemsize matches sizeof, padding included");

    PyObject* len = PyObject_CallMethod(arr, "__len__", nullptr);
    check(PyLong_AsLongLong(len) == static_cast<long long>(data.size()),
          "length matches the vector");
    Py_DECREF(len);

    // Offsets, member by member, against what the compiler actually chose.
    PyObject* fields = PyObject_GetAttrString(dtype, "fields");
    bool offsets_ok = true;
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr auto member =
                mirror_bridge::columnar::detail::field_at<S, Is>();
            constexpr std::size_t want = std::meta::offset_of(member).bytes;
            constexpr auto name = std::meta::identifier_of(member);

            PyObject* entry = PyMapping_GetItemString(
                fields, const_cast<char*>(name.data()));
            if (!entry) {
                PyErr_Clear();
                std::printf("      %s: numpy's dtype has no such field\n",
                            name.data());
                offsets_ok = false;
                return;
            }
            PyObject* off = PyTuple_GetItem(entry, 1);
            if (PyLong_AsSsize_t(off) != static_cast<Py_ssize_t>(want)) {
                std::printf("      %s: numpy says %zd, the compiler says %zu\n",
                            name.data(), PyLong_AsSsize_t(off), want);
                offsets_ok = false;
            }
            Py_DECREF(entry);
        }(), ...);
    }(std::make_index_sequence<
          mirror_bridge::columnar::detail::field_count<S>()>{});
    check(offsets_ok, "every member sits where the compiler put it");
    Py_DECREF(fields);
    Py_DECREF(dtype);

    // And it is a view, not a copy.
    PyObject* flags = PyObject_GetAttrString(arr, "flags");
    PyObject* owndata = PyObject_GetAttrString(flags, "owndata");
    check(owndata == Py_False, "numpy read the buffer in place");
    PyObject* writeable = PyObject_GetAttrString(flags, "writeable");
    check(writeable == Py_False, "and treats it as read-only");
    Py_XDECREF(owndata);
    Py_XDECREF(writeable);
    Py_DECREF(flags);
    Py_DECREF(arr);
}

// The view calls out for an element conversion rather than knowing about
// mirror_bridge::to_python, which lives above this header. A tuple stands in
// for the bound wrapper here; the binding passes the real one.
PyObject* tick_to_python(const Tick& t) {
    return Py_BuildValue("(dic)", t.price, t.size, t.side);
}

void check_view() {
    std::puts("the view behaves as a sequence and describes itself:");
    std::vector<Tick> ticks{{1.5, 10, 'B'}, {2.5, 20, 'S'}, {3.5, 30, 'B'}};

    PyObject* view = mirror_bridge::columnar::to_columnar<Tick>(ticks, tick_to_python);
    if (!view) { PyErr_Print(); ++failures; return; }

    check(PyObject_Length(view) == 3, "len() is the element count");

    PyObject* item = PySequence_GetItem(view, 1);
    check(item && PyTuple_Check(item) &&
              PyFloat_AsDouble(PyTuple_GetItem(item, 0)) == 2.5,
          "indexing copies one element out");
    Py_XDECREF(item);

    PyObject* last = PySequence_GetItem(view, -1);
    check(last && PyFloat_AsDouble(PyTuple_GetItem(last, 0)) == 3.5,
          "a negative index counts from the end");
    Py_XDECREF(last);

    PyObject* oob = PySequence_GetItem(view, 3);
    check(oob == nullptr && PyErr_ExceptionMatches(PyExc_IndexError),
          "past the end raises IndexError");
    PyErr_Clear();
    Py_XDECREF(oob);

    PyObject* as_list = PyObject_CallMethod(view, "tolist", nullptr);
    check(as_list && PyList_Check(as_list) && PyList_GET_SIZE(as_list) == 3,
          "tolist() gives back the list this used to be");
    Py_XDECREF(as_list);

    // Iteration comes free from sq_item, with no tp_iter of our own.
    PyObject* iter = PyObject_GetIter(view);
    long seen = 0;
    if (iter) {
        while (PyObject* next = PyIter_Next(iter)) { ++seen; Py_DECREF(next); }
        Py_DECREF(iter);
    }
    check(seen == 3, "iteration walks every element");

    PyObject* arr = as_array_direct(view);
    check(arr != nullptr, "numpy accepts the view's array interface");
    if (arr) {
        PyObject* key = PyUnicode_FromString("price");
        PyObject* col = PyObject_GetItem(arr, key);
        Py_DECREF(key);
        PyObject* v = col ? PySequence_GetItem(col, 2) : nullptr;
        check(v && PyFloat_AsDouble(v) == 3.5,
              "and reads the third price without copying");
        Py_XDECREF(v);
        Py_XDECREF(col);
        Py_DECREF(arr);
    }

    // The view owns its copy, so the source dying does not matter.
    ticks.clear();
    ticks.shrink_to_fit();
    PyObject* after = PySequence_GetItem(view, 0);
    check(after && PyFloat_AsDouble(PyTuple_GetItem(after, 0)) == 1.5,
          "the view survives the vector it was built from");
    Py_XDECREF(after);

    check(Py_REFCNT(view) == 1, "and holds no reference anyone has to break");

    // The array interface hands out a raw pointer into the view's storage, so
    // the array has to keep the view alive. numpy does this by setting .base,
    // which is what makes `np.asarray(obj.get_points())` -- with nothing
    // holding the view -- safe rather than a dangling read.
    PyObject* arr2 = as_array_direct(view);
    if (arr2) {
        PyObject* base = PyObject_GetAttrString(arr2, "base");
        check(base == view, "numpy's array points its base at the view");
        Py_XDECREF(base);
        check(Py_REFCNT(view) > 1, "so the view cannot be collected under it");

        // Drop our reference and read again: the array is the only owner now.
        Py_DECREF(view);
        PyObject* key = PyUnicode_FromString("price");
        PyObject* col = PyObject_GetItem(arr2, key);
        Py_DECREF(key);
        PyObject* v = col ? PySequence_GetItem(col, 1) : nullptr;
        PyObject* as_f = v ? PyNumber_Float(v) : nullptr;
        check(as_f && PyFloat_AsDouble(as_f) == 2.5,
              "and the array still reads after the last other reference goes");
        Py_XDECREF(as_f);
        Py_XDECREF(v);
        Py_XDECREF(col);
        Py_DECREF(arr2);
    } else {
        PyErr_Print();
        ++failures;
        Py_DECREF(view);
    }
}

}  // namespace

int main() {
    Py_Initialize();

    std::vector<Point3> points{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    std::vector<Tick> ticks{{1.5, 10, 'B'}, {2.5, 20, 'S'}, {3.5, 30, 'B'}};
    std::vector<Flagged> flagged{{'a', 1.25}, {'b', 2.5}};
    std::vector<Mixed> mixed{{1, 2, 3.0f, true}, {4, 5, 6.0f, false}};

    check_layout("Point3", points);
    check_layout("Tick", ticks);
    check_layout("Flagged", flagged);
    check_layout("Mixed", mixed);

    // Values, not just shape: read a column back out and compare.
    std::puts("values survive the round trip:");
    PyObject* iface = mirror_bridge::columnar::build_array_interface<Tick>(
        ticks.data(), ticks.size());
    PyObject* arr = iface ? as_array(iface) : nullptr;
    Py_XDECREF(iface);
    if (arr) {
        PyObject* price_key = PyUnicode_FromString("price");
        PyObject* col = PyObject_GetItem(arr, price_key);
        Py_DECREF(price_key);
        bool ok = col != nullptr;
        for (Py_ssize_t i = 0; ok && i < 3; ++i) {
            PyObject* v = PySequence_GetItem(col, i);
            ok = v && PyFloat_AsDouble(v) == ticks[static_cast<std::size_t>(i)].price;
            Py_XDECREF(v);
        }
        check(ok, "the price column equals what C++ wrote");
        Py_XDECREF(col);

        // A char member already reads back as a number through the scalar
        // path (t.side is 66, not b'B'), so the column agrees with the scalar
        // rather than with intuition about character types. Mapping char to
        // numpy's |S1 would read better and disagree with the rest of the
        // binding.
        PyObject* side_key = PyUnicode_FromString("side");
        PyObject* side = PyObject_GetItem(arr, side_key);
        Py_DECREF(side_key);
        PyObject* first = side ? PySequence_GetItem(side, 0) : nullptr;
        // A numpy scalar is not a Python int, so ask it for one rather than
        // testing its type.
        PyObject* as_int = first ? PyNumber_Long(first) : nullptr;
        check(as_int && PyLong_AsLong(as_int) == 'B',
              "and a char column is an integer column, as the scalar path is");
        Py_XDECREF(as_int);
        Py_XDECREF(first);
        Py_XDECREF(side);
        Py_DECREF(arr);
    } else {
        PyErr_Print();
        ++failures;
    }

    check_view();

    std::printf("\n%s\n", failures == 0
                              ? "PASS: reflection describes these structs exactly"
                              : "FAIL");
    return failures == 0 ? 0 : 1;
}
