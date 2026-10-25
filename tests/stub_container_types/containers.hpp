#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace stubs {

// One accessor per shape the array.array decision can land on, so the stub
// has to mirror the typecode table rather than approximate it with "is this
// numeric".
struct Shapes {
    double gain = 1.0;
    char tag = 'A';
    std::vector<double> xs{1.0, 2.0};

    std::vector<double> doubles() const { return {1.0, 2.0, 3.0}; }
    std::vector<int> ints() const { return {1, 2, 3}; }
    std::vector<float> floats() const { return {1.0f, 2.0f}; }
    std::vector<short> shorts() const { return {1, 2}; }

    // No typecode in the table, so these fall through to the per-element
    // path and come back as lists. They are what separates mirroring the
    // table from guessing at "numeric".
    std::vector<long double> long_doubles() const { return {1.0L, 2.0L}; }
    std::vector<signed char> signed_chars() const { return {1, 2}; }

    std::vector<bool> bools() const { return {true, false}; }
    std::vector<std::string> strings() const { return {"a", "b"}; }

    // The one producer of bytes.
    std::vector<unsigned char> blob() const { return {1, 2, 3}; }

    // Not a std::vector, so no bulk path however numeric it is.
    std::deque<double> deque_doubles() const { return {1.0, 2.0}; }

    // to_python has its own std::array overload, which builds a list. The
    // stub used to call the second one bytes.
    std::array<double, 3> fixed() const { return {1.0, 2.0, 3.0}; }
    std::array<unsigned char, 4> fixed_bytes() const { return {1, 2, 3, 4}; }

    // char is arithmetic and signed, so it crosses as an int.
    std::vector<char> chars() const { return {65, 66}; }
    char first_tag() const { return tag; }

    // The outer vector's element is not arithmetic, so only the inner one
    // takes the bulk path.
    std::vector<std::vector<double>> grid() const { return {{1.0}, {2.0}}; }

    // A parameter takes anything with the sequence protocol, which is wider
    // than a list and narrower than Sequence[float].
    double total(const std::vector<double>& values) const {
        double sum = 0.0;
        for (double v : values) sum += v;
        return sum;
    }
    std::size_t blob_size(const std::vector<unsigned char>& data) const {
        return data.size();
    }
};

}  // namespace stubs
