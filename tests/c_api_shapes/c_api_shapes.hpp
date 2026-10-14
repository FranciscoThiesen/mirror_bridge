#pragma once

#include <string>

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

// What a C API hands back. A returned pointer says nothing about who owns
// the thing, so returned pointers are refused -- but `const char*` is the one
// whose convention is settled: the caller borrows it and does not free it.
// to_python copies it into a Python str before anything else runs, so nothing
// of it outlives the call, and a null reads as None the way the member above
// does. 1,282 functions in the compatibility corpus return one, among them
// every `what()`, SQLiteCpp's getColumnName and tinyxml2's Attribute().
struct Attributes {
    std::string body = "text";
    const char* tag = "node";

    const char* c_str() const { return body.c_str(); }

    // tinyxml2's shape exactly: present answers with a string, absent with a
    // null the caller is expected to test.
    const char* attribute(const char* key) const {
        return (key && std::string(key) == "tag") ? tag : nullptr;
    }

    // Refused, and it has to stay refused: by convention a `char*` return is
    // the caller's to free, so copying it and walking away would leak.
    char* owned() { static char buf[] = "owned"; return buf; }
};

}  // namespace capi
