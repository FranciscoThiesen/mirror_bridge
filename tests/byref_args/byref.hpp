#pragma once

// Instrumented types for verifying by-reference argument passing:
// lvalue-reference parameters of bound classes must alias the Python-held
// object (zero copies, mutations visible), while by-value parameters keep
// copy semantics.

struct Tracked {
    int value = 0;
    static inline int copy_count = 0;

    Tracked() = default;
    Tracked(const Tracked& other) : value(other.value) { ++copy_count; }
    Tracked& operator=(const Tracked& other) {
        value = other.value;
        ++copy_count;
        return *this;
    }

    static int copies() { return copy_count; }
    static void reset_copies() { copy_count = 0; }
};

struct Mutator {
    int calls = 0;

    // T&: mutation must land on the Python-visible object
    void bump(Tracked& t) {
        ++calls;
        t.value += 1;
    }

    // const T&: must not copy the argument
    int read(const Tracked& t) {
        ++calls;
        return t.value;
    }

    // by value: copy semantics preserved
    int read_value(Tracked t) {
        ++calls;
        return t.value;
    }
};
