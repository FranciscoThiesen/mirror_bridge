#pragma once

#include <string>
#include <vector>

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

// The other half of the same question. Tracked& above keeps the promise a
// non-const reference makes, because the argument aliases the object Python
// holds. None of these can: an int, a float, a str and a list all arrive as
// copies the caller never had the address of. Each of these used to bind, run,
// return None, and leave the caller's value exactly as it was -- a wrong
// answer rather than a missing name, which is the worse of the two.
struct LostWrites {
    int calls = 0;

    int usable(int v) { ++calls; return v * 2; }

    void sum_into(const std::vector<double>& xs, double& total) {
        total = 0.0;
        for (double x : xs) total += x;
    }
    void bump_int(int& n) { n += 1; }
    void append_to(std::string& s) { s += "!"; }
    void fill(std::vector<double>& out, int n) {
        out.clear();
        for (int i = 0; i < n; ++i) out.push_back(static_cast<double>(i));
    }

    static void static_bump(int& n) { n += 1; }
};
