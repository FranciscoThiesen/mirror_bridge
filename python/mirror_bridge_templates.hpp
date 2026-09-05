#pragma once

// ============================================================================
// Mirror Bridge - Python Template Families
// ============================================================================
//
// A C++ template becomes ONE Python object that knows its bound
// instantiations, so Python code never has to spell a mangled name:
//
//   geom.Vector3[np.float32]       exact lookup by template argument
//   geom.Vector3["float"]          ... by C++ spelling
//   geom.Vector3(1.0, 2.0, 3.0)    CTAD-like: best constructor across instances
//   geom.Vector3.instantiations    ["float", "double", "int", "long", "bool"]
//   geom.clamp(1.5, 0, 1)          overload dispatch across clamp<double>, clamp<float>
//   geom.clamp[float]              exact lookup (Python float = C double)
//   v.cast[float]()                member template, a descriptor on the class
//   v.cast(...)                    dispatch by argument types
//
// Binding side (the CLI generates this from the approved plan):
//   templates::bind_instance<geom::Vector3<float>>(m, "Vec3f")
//       .member_template<"cast", double>();
//   templates::bind_instance<&geom::clamp<double>>(m);
//   templates::bind_member_template<geom::Robot, "distance_to", float>();
//
// Every instantiation is still an ordinary mirror_bridge binding (a real type
// object, a real PyCFunction): the family object is only a dispatcher over
// them, so the zero-overhead call path is unchanged.
//
// This header is included at the end of python/mirror_bridge_python.hpp; it
// can also be included directly.
//
// ============================================================================

#include "python/mirror_bridge_python.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace mirror_bridge {
namespace templates {

// ------------------------------------------------------------ argument keys --

enum class Cat : unsigned char { Bool, Int, UInt, Float, String, Class, Value, Other };

// One template argument, described so a Python key (a type, a numpy dtype, a
// string, an int) can be matched against it.
struct ArgKey {
    Cat cat;
    unsigned size;                 // sizeof, scalars only (0 = don't care)
    const char* spelling;          // C++: "float", "geom::Robot", "3"
    const char* pretty;            // Python-facing: "float", "Robot", "3"
    PyTypeObject* (*resolve)();    // Cat::Class: the bound Python type, looked up lazily
};

// A consteval call that returns a std::string (heap storage) may only appear
// inside another constant evaluation, so runtime binding code reaches the
// spelling helpers through these NTTP wrappers.
template <std::meta::info R> consteval const char* c_spell()     { return define_static_string(spelling::spell(R)); }
template <std::meta::info R> consteval const char* c_args()      { return define_static_string(spelling::spell_args(spelling::args_of(R))); }
template <std::meta::info R> consteval const char* c_ident()     { return define_static_string(std::string(std::meta::identifier_of(R))); }
template <std::meta::info R> consteval const char* c_qualified() { return define_static_string(spelling::qualified(R)); }
// geom::clamp<double>, geom::Vector3<float>::cast<int>: specializations have no identifier of their own.
template <std::meta::info F> consteval const char* c_function() {
    if constexpr (std::meta::has_template_arguments(F))
        return define_static_string(spelling::qualified(std::meta::template_of(F)) + "<" + spelling::spell_args(spelling::args_of(F)) + ">");
    else
        return define_static_string(spelling::qualified(F));
}

template <std::meta::info A>
consteval ArgKey describe_arg() {
    if constexpr (std::meta::is_type(A)) {
        using T = typename[:std::meta::dealias(A):];
        ArgKey k{Cat::Other, 0, define_static_string(spelling::spell(A)),
                 define_static_string(spelling::pretty(A)), nullptr};
        if constexpr (std::is_same_v<T, bool>) { k.cat = Cat::Bool; k.size = 1; }
        else if constexpr (std::is_integral_v<T>) { k.cat = std::is_signed_v<T> ? Cat::Int : Cat::UInt; k.size = sizeof(T); }
        else if constexpr (std::is_floating_point_v<T>) { k.cat = Cat::Float; k.size = sizeof(T); }
        else if constexpr (StringLike<T>) { k.cat = Cat::String; }
        else if constexpr (Bindable<T>) {
            k.cat = Cat::Class;
            k.resolve = [] { PyTypeObject* t = lookup_type_in_python<T>(); return t ? t : TypeRegistry<T>::py_type; };
        }
        return k;
    } else {
        const char* v = define_static_string(std::string(std::meta::display_string_of(A)));
        return ArgKey{Cat::Value, 0, v, v, nullptr};
    }
}

template <std::meta::info Spec>
std::vector<ArgKey> keys_of() {
    std::vector<ArgKey> keys;
    template for (constexpr auto a : define_static_array(std::meta::template_arguments_of(Spec)))
        keys.push_back(describe_arg<a>());
    return keys;
}

// "float32" / "int" / "bool_" / "double" -> category + size. Understands Python
// builtins, numpy dtype names and C++ spellings alike.
inline bool parse_scalar_name(const char* n, Cat& cat, unsigned& size) {
    struct { const char* name; Cat cat; unsigned size; } table[] = {
        {"bool", Cat::Bool, 1}, {"bool_", Cat::Bool, 1},
        {"int", Cat::Int, sizeof(long)}, {"long", Cat::Int, sizeof(long)}, {"long long", Cat::Int, 8},
        {"short", Cat::Int, 2}, {"char", Cat::Int, 1}, {"signed char", Cat::Int, 1},
        {"int8", Cat::Int, 1}, {"int16", Cat::Int, 2}, {"int32", Cat::Int, 4}, {"int64", Cat::Int, 8},
        {"uint8", Cat::UInt, 1}, {"uint16", Cat::UInt, 2}, {"uint32", Cat::UInt, 4}, {"uint64", Cat::UInt, 8},
        {"unsigned", Cat::UInt, 4}, {"unsigned int", Cat::UInt, 4}, {"unsigned long", Cat::UInt, sizeof(long)},
        {"unsigned char", Cat::UInt, 1}, {"unsigned short", Cat::UInt, 2},
        {"float", Cat::Float, 8}, {"double", Cat::Float, 8}, {"float16", Cat::Float, 2},
        {"float32", Cat::Float, 4}, {"float64", Cat::Float, 8}, {"longdouble", Cat::Float, 16},
        {"str", Cat::String, 0}, {"str_", Cat::String, 0}, {"string", Cat::String, 0}, {"std::string", Cat::String, 0},
    };
    for (auto& e : table)
        if (std::strcmp(e.name, n) == 0) { cat = e.cat; size = e.size; return true; }
    return false;
}

inline const char* short_type_name(PyTypeObject* t) {
    const char* dot = std::strrchr(t->tp_name, '.');
    return dot ? dot + 1 : t->tp_name;
}

// 2 = exact, 1 = same category (int vs int32), 0 = no match
inline int match_key(PyObject* key, const ArgKey& k) {
    auto by_scalar = [&](const char* name) {
        Cat cat; unsigned size;
        if (!parse_scalar_name(name, cat, size)) return 0;
        if (cat != k.cat) return 0;
        return (size == 0 || k.size == 0 || size == k.size) ? 2 : 1;
    };
    if (PyType_Check(key)) {
        if (k.cat == Cat::Class && k.resolve && reinterpret_cast<PyTypeObject*>(key) == k.resolve()) return 2;
        return by_scalar(short_type_name(reinterpret_cast<PyTypeObject*>(key)));
    }
    if (PyUnicode_Check(key)) {
        const char* s = PyUnicode_AsUTF8(key);
        if (!s) { PyErr_Clear(); return 0; }
        if (std::strcmp(s, k.spelling) == 0 || std::strcmp(s, k.pretty) == 0) return 2;
        return by_scalar(s);
    }
    if (PyLong_Check(key) && !PyBool_Check(key)) {
        if (k.cat != Cat::Value) return 0;
        long long v = PyLong_AsLongLong(key);
        if (v == -1 && PyErr_Occurred()) { PyErr_Clear(); return 0; }
        return v == std::atoll(k.spelling) ? 2 : 0;
    }
    // numpy dtype instances: np.dtype("float32").name == "float32"
    if (PyObject* name = PyObject_GetAttrString(key, "name")) {
        int r = 0;
        if (PyUnicode_Check(name)) { const char* s = PyUnicode_AsUTF8(name); r = s ? by_scalar(s) : 0; }
        Py_DECREF(name);
        return r;
    }
    PyErr_Clear();
    return 0;
}

// ---------------------------------------------------------- call-time scoring --
// How well does a Python argument fit a C++ parameter type? 3 exact, 2 same
// category, 1 convertible, 0 reject. Ties resolve to declaration order, which
// is the planner's universe order (bool, long, double, string, ...).

inline int scalar_score(PyObject* o, Cat want, unsigned size) {
    if (want == Cat::Bool) return PyBool_Check(o) ? 3 : PyLong_Check(o) ? 1 : 0;
    if (want == Cat::Int || want == Cat::UInt) {
        if (PyBool_Check(o)) return 1;
        if (PyLong_Check(o)) return (want == Cat::Int && size == sizeof(long)) ? 3 : 2;
    }
    if (want == Cat::Float) {
        if (PyFloat_Check(o)) return size == sizeof(double) ? 3 : 2;
        if (PyLong_Check(o) && !PyBool_Check(o)) return 1;
    }
    // numpy scalars (np.float32(1.0)): match by dtype name
    Cat cat; unsigned sz;
    if (parse_scalar_name(short_type_name(Py_TYPE(o)), cat, sz)) {
        if (cat == want) return sz == size ? 3 : 2;
        if (cat != Cat::String && cat != Cat::Bool && want != Cat::Bool) return 1;
    }
    return 0;
}

template <typename P>
int score_arg(PyObject* o) {
    using D = std::remove_cvref_t<P>;
    if constexpr (std::is_same_v<D, bool>) return scalar_score(o, Cat::Bool, 1);
    else if constexpr (std::is_integral_v<D>) return scalar_score(o, std::is_signed_v<D> ? Cat::Int : Cat::UInt, sizeof(D));
    else if constexpr (std::is_floating_point_v<D>) return scalar_score(o, Cat::Float, sizeof(D));
    else if constexpr (StringLike<D>) return PyUnicode_Check(o) ? 3 : 0;
    else if constexpr (Bindable<D>) {
        PyTypeObject* t = lookup_type_in_python<D>();
        if (!t) t = TypeRegistry<D>::py_type;
        if (!t || !PyObject_TypeCheck(o, t)) return 0;
        return Py_TYPE(o) == t ? 3 : 2;
    }
    else return 1;   // containers, callables, ...: from_python decides at call time
}

template <typename... Ps>
int score_args(PyObject* args) {
    if (PyTuple_Size(args) != static_cast<Py_ssize_t>(sizeof...(Ps))) return 0;
    int total = 1;   // a viable zero-argument call scores 1
    std::size_t i = 0;
    bool ok = true;
    ([&] { int s = score_arg<Ps>(PyTuple_GET_ITEM(args, i++)); if (!s) ok = false; total += s; }(), ...);
    return ok ? total : 0;
}

template <typename Tuple> struct ScoreTuple;
template <typename... Ps> struct ScoreTuple<std::tuple<Ps...>> {
    static int score(PyObject* args) { return score_args<Ps...>(args); }
};

template <auto FuncPtr>
int score_function(PyObject* args) {
    return ScoreTuple<typename FunctionTraits<decltype(FuncPtr)>::ArgsTuple>::score(args);
}

template <typename T, std::size_t Ctor, std::size_t... Ps>
int score_constructor(PyObject* args, std::index_sequence<Ps...>) {
    if constexpr (constructor_params_all_bindable<T, Ctor>())
        return score_args<constructor_param_t<T, Ctor, Ps>...>(args);
    else
        return 0;
}

// Best constructor of T for these arguments (CTAD-like dispatch across a family).
template <typename T>
int score_constructors(PyObject* args) {
    int best = 0;
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((best = std::max(best, score_constructor<T, Is>(
              args, std::make_index_sequence<get_constructor_param_count<T, Is>()>{}))), ...);
    }(std::make_index_sequence<get_constructor_count<T>()>{});
    if (best == 0 && PyTuple_Size(args) == 0 && std::is_default_constructible_v<T>) best = 1;
    return best;
}

// --------------------------------------------------------- member templates --

template <std::size_t N>
struct Name {
    char data[N]{};
    consteval Name(const char (&s)[N]) { for (std::size_t i = 0; i < N; ++i) data[i] = s[i]; }
    consteval std::string_view view() const { return {data, N - 1}; }
};
template <std::size_t N> Name(const char (&)[N]) -> Name<N>;

template <typename T, Name N>
consteval std::meta::info find_member_template() {
    for (auto m : std::meta::members_of(^^T, std::meta::access_context::unchecked()))
        if (std::meta::is_function_template(m) && std::meta::has_identifier(m) && std::meta::identifier_of(m) == N.view())
            return m;
    throw "no such member template";
}

template <typename T, std::meta::info Inst, std::size_t... Is>
PyObject* call_member_impl(T& obj, PyObject* args, std::index_sequence<Is...>) {
    constexpr auto params = define_static_array(std::meta::parameters_of(Inst));
    if (PyTuple_Size(args) != static_cast<Py_ssize_t>(sizeof...(Is))) {
        PyErr_Format(PyExc_TypeError, "%s takes %zu argument(s) but %zd were given",
                     c_function<Inst>(), sizeof...(Is), PyTuple_Size(args));
        return nullptr;
    }
    std::tuple<std::remove_cvref_t<typename[:std::meta::type_of(params[Is]):]>...> cpp_args;
    bool ok = true;
    ([&] {
        if (!ok) return;
        if (!from_python(PyTuple_GET_ITEM(args, Is), std::get<Is>(cpp_args))) {
            if (!PyErr_Occurred()) PyErr_Format(PyExc_TypeError, "Argument %zu: type conversion failed", Is + 1);
            ok = false;
        }
    }(), ...);
    if (!ok) return nullptr;
    using R = typename[:std::meta::return_type_of(Inst):];
    if constexpr (std::is_void_v<R>) {
        (obj.[:Inst:])(std::get<Is>(cpp_args)...);
        Py_RETURN_NONE;
    } else {
        auto r = (obj.[:Inst:])(std::get<Is>(cpp_args)...);
        return to_python(r);
    }
}

template <typename T, std::meta::info Inst>
PyObject* call_member(PyObject* self, PyObject* args) {
    auto* w = reinterpret_cast<PyWrapper<T>*>(self);
    if (!w->cpp_object) { PyErr_SetString(PyExc_RuntimeError, "object is not initialized"); return nullptr; }
    constexpr std::size_t n = std::meta::parameters_of(Inst).size();
    return call_member_impl<T, Inst>(*w->cpp_object, args, std::make_index_sequence<n>{});
}

template <std::meta::info Inst, std::size_t... Is>
int score_member_impl(PyObject* args, std::index_sequence<Is...>) {
    constexpr auto params = define_static_array(std::meta::parameters_of(Inst));
    return score_args<typename[:std::meta::type_of(params[Is]):]...>(args);
}

template <std::meta::info Inst>
int score_member(PyObject* args) {
    constexpr std::size_t n = std::meta::parameters_of(Inst).size();
    return score_member_impl<Inst>(args, std::make_index_sequence<n>{});
}

// ----------------------------------------------------------- family object --

enum class Kind : unsigned char { Class, Function, Member };

struct Instance {
    std::vector<ArgKey> keys;
    const char* spelling;          // geom::Vector3<float>
    const char* args;              // "float"  |  "float, 3"
    PyObject* target;              // Class: the type object; Function: the PyCFunction
    PyMethodDef* def;              // Function / Member
    int (*score)(PyObject* args);  // call viability, 0 = not viable
};

struct Family {
    PyObject_HEAD
    std::vector<Instance>* instances;
    const char* name;              // Vector3
    const char* qualified;         // geom::Vector3
    Kind kind;
    PyTypeObject* owner;           // Member: the class the descriptor lives on
    PyObject* self;                // Member, bound through an instance
};

inline PyTypeObject FamilyType;

inline std::string instantiation_list(Family* f) {
    std::string s;
    for (auto& i : *f->instances) { if (!s.empty()) s += ", "; s += i.args; }
    return s;
}

inline void family_dealloc(PyObject* o) {
    auto* f = reinterpret_cast<Family*>(o);
    Py_XDECREF(f->self);
    Py_TYPE(o)->tp_free(o);
}

inline PyObject* family_repr(PyObject* o) {
    auto* f = reinterpret_cast<Family*>(o);
    return PyUnicode_FromFormat("<C++ template %s<...> with instantiations: %s>", f->qualified, instantiation_list(f).c_str());
}

// __getitem__: exact template arguments, or the closest by category.
inline Instance* pick_by_key(Family* f, PyObject* key) {
    PyObject* tup = PyTuple_Check(key) ? Py_NewRef(key) : PyTuple_Pack(1, key);
    if (!tup) return nullptr;
    Py_ssize_t n = PyTuple_Size(tup);
    Instance* best = nullptr;
    int best_score = 0;
    for (auto& inst : *f->instances) {
        if (static_cast<Py_ssize_t>(inst.keys.size()) != n) continue;
        int score = 2;
        for (Py_ssize_t i = 0; i < n && score; ++i)
            score = std::min(score, match_key(PyTuple_GET_ITEM(tup, i), inst.keys[i]));
        if (score > best_score) { best_score = score; best = &inst; }
    }
    Py_DECREF(tup);
    if (!best) {
        PyObject* r = PyObject_Repr(key);
        PyErr_Format(PyExc_KeyError, "%s has no instantiation for %U (available: %s)",
                     f->qualified, r, instantiation_list(f).c_str());
        Py_XDECREF(r);
    }
    return best;
}

inline PyObject* instance_object(Family* f, Instance* i) {
    switch (f->kind) {
    case Kind::Class:
    case Kind::Function:
        return Py_NewRef(i->target);
    case Kind::Member:
        return f->self ? PyCFunction_New(i->def, f->self)
                       : PyDescr_NewMethod(f->owner, i->def);
    }
    return nullptr;
}

inline PyObject* family_getitem(PyObject* o, PyObject* key) {
    auto* f = reinterpret_cast<Family*>(o);
    Instance* i = pick_by_key(f, key);
    return i ? instance_object(f, i) : nullptr;
}

inline Py_ssize_t family_len(PyObject* o) {
    return static_cast<Py_ssize_t>(reinterpret_cast<Family*>(o)->instances->size());
}

inline PyObject* family_call(PyObject* o, PyObject* args, PyObject* kwargs) {
    auto* f = reinterpret_cast<Family*>(o);
    if (f->kind == Kind::Member && !f->self) {
        PyErr_Format(PyExc_TypeError, "%s.%s must be called through an instance", f->owner->tp_name, f->name);
        return nullptr;
    }
    Instance* best = nullptr;
    int best_score = 0;
    for (auto& inst : *f->instances) {
        int s = inst.score(args);
        if (s > best_score) { best_score = s; best = &inst; }
    }
    if (!best) {
        std::string got;
        for (Py_ssize_t i = 0; i < PyTuple_Size(args); ++i) {
            if (i) got += ", ";
            got += Py_TYPE(PyTuple_GET_ITEM(args, i))->tp_name;
        }
        PyErr_Format(PyExc_TypeError, "no instantiation of %s accepts (%s); available: %s",
                     f->qualified, got.c_str(), instantiation_list(f).c_str());
        return nullptr;
    }
    switch (f->kind) {
    case Kind::Class:    return PyObject_Call(best->target, args, kwargs);
    case Kind::Function: return PyObject_Call(best->target, args, nullptr);
    case Kind::Member:   return reinterpret_cast<PyCFunction>(best->def->ml_meth)(f->self, args);
    }
    return nullptr;
}

// Member template descriptor: `obj.cast` yields a copy of the family bound to obj.
inline PyObject* family_descr_get(PyObject* o, PyObject* obj, PyObject*) {
    auto* f = reinterpret_cast<Family*>(o);
    if (!obj || obj == Py_None) return Py_NewRef(o);
    auto* b = reinterpret_cast<Family*>(FamilyType.tp_alloc(&FamilyType, 0));
    if (!b) return nullptr;
    b->instances = f->instances;
    b->name = f->name;
    b->qualified = f->qualified;
    b->kind = f->kind;
    b->owner = f->owner;
    b->self = Py_NewRef(obj);
    return reinterpret_cast<PyObject*>(b);
}

inline PyObject* family_instantiations(PyObject* o, void*) {
    auto* f = reinterpret_cast<Family*>(o);
    PyObject* list = PyList_New(0);
    if (!list) return nullptr;
    for (auto& i : *f->instances) {
        PyObject* s = PyUnicode_FromString(i.args);
        if (!s || PyList_Append(list, s) < 0) { Py_XDECREF(s); Py_DECREF(list); return nullptr; }
        Py_DECREF(s);
    }
    return list;
}

inline PyObject* family_name(PyObject* o, void*) {
    return PyUnicode_FromString(reinterpret_cast<Family*>(o)->name);
}

inline PyGetSetDef family_getset[] = {
    {"instantiations", family_instantiations, nullptr, "template arguments of every bound instantiation", nullptr},
    {"__name__", family_name, nullptr, nullptr, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr},
};

inline PyMappingMethods family_mapping = { family_len, family_getitem, nullptr };

inline bool ensure_family_type() {
    static bool ready = false;
    if (ready) return true;
    FamilyType = PyTypeObject{ PyVarObject_HEAD_INIT(nullptr, 0) };
    FamilyType.tp_name = "mirror_bridge.Template";
    FamilyType.tp_basicsize = sizeof(Family);
    FamilyType.tp_dealloc = family_dealloc;
    FamilyType.tp_repr = family_repr;
    FamilyType.tp_call = family_call;
    FamilyType.tp_as_mapping = &family_mapping;
    FamilyType.tp_descr_get = family_descr_get;
    FamilyType.tp_getset = family_getset;
    FamilyType.tp_flags = Py_TPFLAGS_DEFAULT;
    FamilyType.tp_doc = "A C++ template: index with template arguments, or call to dispatch";
    ready = PyType_Ready(&FamilyType) == 0;
    return ready;
}

// The family object for a template, created on first use and stored under the
// template's own name (in the module, or in the class dict for member templates).
inline Family* family_for(PyObject* dict_owner, bool is_type, const char* name, const char* qualified,
                          Kind kind, PyTypeObject* owner) {
    if (!ensure_family_type()) return nullptr;
    PyObject* dict = is_type ? reinterpret_cast<PyTypeObject*>(dict_owner)->tp_dict : PyModule_GetDict(dict_owner);
    PyObject* existing = PyDict_GetItemString(dict, name);
    if (existing && Py_TYPE(existing) == &FamilyType) return reinterpret_cast<Family*>(existing);
    if (existing) {
        // Something else (an alias bound under the template's own name, a plain
        // method) already owns the name; replacing it would silently break the
        // binding that got there first.
        std::fprintf(stderr, "mirror_bridge: template '%s' not exposed: '%s' is already bound to something else\n",
                     qualified, name);
        return nullptr;
    }
    auto* f = reinterpret_cast<Family*>(FamilyType.tp_alloc(&FamilyType, 0));
    if (!f) return nullptr;
    f->instances = new std::vector<Instance>();   // lives as long as the module
    f->name = name;
    f->qualified = qualified;
    f->kind = kind;
    f->owner = owner;
    f->self = nullptr;
    PyDict_SetItemString(dict, name, reinterpret_cast<PyObject*>(f));
    Py_DECREF(f);   // the dict holds it
    if (is_type) PyType_Modified(reinterpret_cast<PyTypeObject*>(dict_owner));
    return f;
}

// -------------------------------------------------------------- binding API --

// One instantiation of T's member template N, added to the family that hangs
// off T's Python type as a descriptor.
template <typename T, Name N, typename... Args>
bool add_member_instance(PyTypeObject* type) {
    if (!type) return false;
    constexpr auto mt = find_member_template<T, N>();
    constexpr auto inst = std::meta::substitute(mt, {^^Args...});
    static PyMethodDef def = { N.data, reinterpret_cast<PyCFunction>(call_member<T, inst>), METH_VARARGS, nullptr };
    Family* f = family_for(reinterpret_cast<PyObject*>(type), true, N.data, c_qualified<mt>(), Kind::Member, type);
    if (!f) return false;
    // GCC 16 rejects `push_back(Instance{...})` here ("consteval-only
    // expressions are only allowed in a constant-evaluated context") because
    // the braced temporary spells a template-id with an info argument; the bare
    // braced-init-list form is accepted by both compilers.
    f->instances->push_back({keys_of<inst>(), c_function<inst>(), c_args<inst>(),
                                     nullptr, &def, &score_member<inst>});
    return true;
}

template <typename T>
struct BoundInstance {
    BoundClass<T> cls;

    template <Name N, typename... Args>
    BoundInstance& member_template() {
        add_member_instance<T, N, Args...>(cls.type);
        return *this;
    }

    BoundInstance& release_gil(bool enabled = true) { cls.release_gil(enabled); return *this; }
    explicit operator bool() const { return cls.type != nullptr; }
};

// A class template specialization: bound like any class, plus registered with
// its family so Python can find it by template argument.
template <typename T>
BoundInstance<T> bind_instance(PyObject* m, const char* name) {
    static_assert(std::meta::has_template_arguments(^^T), "bind_instance<T>: T must be a template specialization");
    BoundInstance<T> b{bind_class_when_bindable<T>(m, name)};
    if (!b.cls.type) return b;
    // GCC 16 rejects `^^T` spelled directly as a template argument inside a
    // template body ("consteval-only expressions are only allowed in a
    // constant-evaluated context"); a constexpr variable is fine on both.
    constexpr auto self = ^^T;
    constexpr auto tmpl = std::meta::template_of(self);
    Family* f = family_for(m, false, c_ident<tmpl>(), c_qualified<tmpl>(), Kind::Class, nullptr);
    if (!f) return b;
    f->instances->push_back({keys_of<self>(), c_spell<self>(), c_args<self>(),
                                     reinterpret_cast<PyObject*>(b.cls.type), nullptr, &score_constructors<T>});
    return b;
}

// A function template specialization, by address: reflect_function recovers
// the template and its arguments, so the call site needs no name at all.
template <auto FuncPtr>
bool bind_instance(PyObject* m) {
    constexpr auto fn = std::meta::reflect_function(*FuncPtr);
    static_assert(std::meta::has_template_arguments(fn), "bind_instance<&f>: f must be a function template specialization");
    constexpr auto tmpl = std::meta::template_of(fn);
    constexpr const char* name = c_ident<tmpl>();
    static PyMethodDef def = { name, reinterpret_cast<PyCFunction>(call_free_function<FuncPtr>), METH_VARARGS, nullptr };
    PyObject* func = PyCFunction_New(&def, nullptr);
    if (!func) return false;
    Family* f = family_for(m, false, name, c_qualified<tmpl>(), Kind::Function, nullptr);
    if (!f) { Py_DECREF(func); return false; }
    f->instances->push_back({keys_of<fn>(), c_function<fn>(), c_args<fn>(),
                                     func, &def, &score_function<FuncPtr>});
    return true;
}

// A member template of a class bound earlier with bind_class (plain classes:
// the planner emits this right after the class binding).
template <typename T, Name N, typename... Args>
bool bind_member_template() {
    return add_member_instance<T, N, Args...>(TypeRegistry<T>::py_type);
}

} // namespace templates
} // namespace mirror_bridge
