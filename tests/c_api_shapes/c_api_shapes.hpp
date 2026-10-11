#pragma once

// Shapes a C-style or C-adjacent API has, each of which used to fail the whole
// module rather than costing itself. All three came from pointing `generate` at
// ImGui and Open3D.
namespace capi {

// ImGui's ImFontGlyph packs its fields. A reference cannot bind to a bit-field,
// so the generated getter did not compile: "non-const reference cannot bind to
// bit-field 'Codepoint'".
struct Glyph {
    unsigned int colored : 1;
    unsigned int visible : 1;
    unsigned int codepoint : 30;
    float advance_x;
};

// ImGuiStoragePair is this: an anonymous union is a nonstatic data member with
// no name, and identifier_of has a precondition rather than a fallback, so
// asking for its name stopped the member cache being a constant expression.
// The union itself is declined -- three aliased attributes where reading the
// one that was not written is undefined is not an API worth exposing -- but
// the named members beside it must survive.
struct StoragePair {
    int key = 0;
    union { int val_i; float val_f; void* val_p; };
    int tag = 7;
};

// Open3D forward-declares camera::PinholeCameraParameters and names it only
// by pointer and reference. That is ordinary legal C++, and it reached
// lookup_type_in_python<T>, where `typeid(T)` on an incomplete type is
// ill-formed -- so one forward declaration failed the module. The member
// template is what carries it there: the planner instantiates a member
// template over every type the headers name, and this one is in that universe.
struct Opaque;

struct HoldsOpaque {
    int size = 3;
    Opaque* handle = nullptr;   // never dereferenced here

    template <typename T>
    int count_of() const { return size; }
};

// A C API says "absent" with a null pointer. PyUnicode_FromString(nullptr)
// does not fail, it dereferences: reading this member took the whole
// interpreter down with SIGSEGV, no traceback, nothing to catch.
struct Label {
    const char* text = nullptr;
    int id = 1;
};

}  // namespace capi
