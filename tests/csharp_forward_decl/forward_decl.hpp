#pragma once

// C#-ONLY, and deliberately not in tests/csharp_bindings/csharp_bindings.hpp.
// That header is shared: tests/constexpr_budget builds it with --lang python,
// and the PYTHON backend has the same incomplete-type gap this fixture exists
// to pin for C# -- it emits a lambda whose return type is the incomplete type
// and fails with "incomplete result type 'ReturnType'". Putting these shapes
// in the shared header broke that test, which is how the Python gap was found.
// Leave them here until Python is fixed too.

// Declared, never defined. std::is_class_v is TRUE for a forward-declared
// class and core::Bindable only adds a reflection query an incomplete type can
// still satisfy, so this used to reach typeid -- which does not compile for an
// incomplete type, taking the whole module down with an error pointing into
// mirror_bridge's own source rather than naming a method.
//
// A MEMBER of an incomplete type is ill-formed C++, so it cannot be written
// here; a function that returns or takes one is legal to DECLARE, which is
// exactly how yaml-cpp's YAML::detail::iterator_value gets in. Nothing calls
// these, so they need no definition.
struct Opaque;

struct Forward {
    int id = 5;
    int doubled() const { return id * 2; }

    Opaque make() const;
    int take(Opaque o) const;
};
